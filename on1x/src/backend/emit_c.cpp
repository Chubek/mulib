#include <on1x/backend/mir.hpp>
#include <limits>
#include <map>
#include <sstream>

namespace on1x::mir {
namespace {
std::string ctype(const Type& t){return t==Type::I64?"int64_t":t==Type::Bool?"bool":"void";}
std::string value(Id id){return "v"+std::to_string(id);}
std::string name(Id id){return "on1x_mir_f"+std::to_string(id);}
std::string literal(std::int64_t n){
 if(n==std::numeric_limits<std::int64_t>::min())return "(-INT64_C(9223372036854775807)-INT64_C(1))";
 return "INT64_C("+std::to_string(n)+")";
}
void signature(std::ostream& out,const Function& f){
 out<<ctype(f.result)<<" "<<name(f.id)<<"(";const auto& params=f.blocks.front().parameters;
 if(params.empty())out<<"void";
 for(std::size_t n=0;n<params.size();++n){if(n)out<<", ";out<<ctype(params[n].type)<<" "<<value(params[n].id);}
 out<<")";
}
}
std::string emit_c(const Module& module){
 verify(module);const Function* main=nullptr;
 for(const auto& f:module.functions)if(f.name=="main")main=&f;
 if(!main||!main->blocks.front().parameters.empty()||(main->result!=Type::I64&&main->result!=Type::Void))
  throw Error({"<mir>"},"ON1X-MIR-C11: main must have no parameters and return i64 or void");
 std::ostringstream out;out<<"/* On1x scalar MIR v1: checked C11 lowering. */\n#include <on1x_runtime.h>\n\n";
 for(const auto& f:module.functions){signature(out,f);out<<";\n";}
 for(const auto& f:module.functions){
  std::map<Id,Type> value_types;std::map<Id,const Block*> blocks;
  for(const auto& b:f.blocks){blocks.emplace(b.id,&b);for(const auto& p:b.parameters)value_types.emplace(p.id,p.type);for(const auto& i:b.instructions)value_types.emplace(i.id,i.type);}
  signature(out,f);out<<" {\n";
  for(const auto& p:f.blocks.front().parameters)out<<"  (void)"<<value(p.id)<<";\n";
  auto declare=[&](Id id,const Type& type){if(type!=Type::Void)out<<"  "<<ctype(type)<<" "<<value(id)<<" = 0; (void)"<<value(id)<<";\n";};
  for(const auto& s:f.slots)declare(s.id,s.type);
  for(const auto& b:f.blocks){
   if(&b!=&f.blocks.front())for(const auto& p:b.parameters)declare(p.id,p.type);
   for(const auto& i:b.instructions)declare(i.id,i.type);
  }
  out<<"  goto b"<<f.blocks.front().id<<";\n";
  for(const auto& b:f.blocks){
   out<<"b"<<b.id<<":;\n";
   for(const auto& i:b.instructions){
    auto a=i.operands.empty()?std::string{}:value(i.operands[0]);
    auto c=i.operands.size()<2?std::string{}:value(i.operands[1]);
    out<<"  ";if(i.type!=Type::Void)out<<value(i.id)<<" = ";
    switch(i.op){
    case Op::Constant:out<<(i.type==Type::Bool?(i.immediate?"true":"false"):literal(i.immediate));break;
    case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Neg:{
     auto op=op_name(i.op);out<<"on1x_"<<op.substr(0,op.find('.'))<<"_i64("<<a;
     if(i.op!=Op::Neg)out<<", "<<c;
     out<<")";break;
    }
    case Op::Not:out<<"(!"<<a<<")";break;
    case Op::Eq:case Op::Ne:case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:{
     const char* op=i.op==Op::Eq?"==":i.op==Op::Ne?"!=":i.op==Op::Lt?"<":i.op==Op::Le?"<=":i.op==Op::Gt?">":">=";
     out<<"("<<a<<" "<<op<<" "<<c<<")";break;
    }
    case Op::Load:out<<value(i.target);break;
    case Op::Store:out<<value(i.target)<<" = "<<a;break;
    case Op::Call:
     out<<name(i.target)<<"(";
     for(std::size_t n=0;n<i.operands.size();++n){if(n)out<<", ";out<<value(i.operands[n]);}
     out<<")";break;
    case Op::Print:{
     auto type=value_types.at(i.operands[0]);
     out<<"on1x_print_"<<(type==Type::Bool?"bool":"i64")<<"("<<a<<")";break;
    }
    case Op::Assert:out<<"on1x_assert("<<a<<")";break;
    }
    out<<";\n";
   }
   const auto& t=b.exit;
   auto edge=[&](const Edge& e){
    const Block* target=blocks.at(e.block);
    out<<" {\n";
    // Phi transfers are simultaneous, even when an edge permutes loop values.
    for(std::size_t n=0;n<e.arguments.size();++n)out<<"    "<<ctype(target->parameters[n].type)<<" edge"<<n<<" = "<<value(e.arguments[n])<<";\n";
    for(std::size_t n=0;n<e.arguments.size();++n)out<<"    "<<value(target->parameters[n].id)<<" = edge"<<n<<";\n";
    out<<"    goto b"<<e.block<<";\n  }\n";
   };
   if(t.kind==Exit::Return){out<<"  return";if(t.value)out<<" "<<value(t.value);out<<";\n";}
   else if(t.kind==Exit::Jump){out<<" ";edge(t.edges[0]);}
   else{out<<"  if ("<<value(t.value)<<")";edge(t.edges[0]);out<<"  else";edge(t.edges[1]);}
  }
  out<<"}\n\n";
 }
 if(main->result==Type::Void)out<<"int main(void) { "<<name(main->id)<<"(); return 0; }\n";
 else out<<"int main(void) { return (int)"<<name(main->id)<<"(); }\n";
 return out.str();
}
}

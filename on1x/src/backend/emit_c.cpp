#include <on1x/backend/mir.hpp>
#include <limits>
#include <map>
#include <sstream>
#include <algorithm>
#include <bit>
#include "numeric.hpp"

namespace on1x::mir {
namespace {
class Types {
 std::map<Type,std::string> names;
public:
  std::ostringstream declarations;bool floats=false,singles=false,words=false;
 std::string ctype(const Type& t){
   if(t==Type::F64){floats=true;return "double";}
   if(t==Type::F32){floats=singles=true;return "float";}
   if(t.is_unsigned()){words=true;return "uint"+type_name(t).substr(1)+"_t";}
  if(!t.is_tuple())return t==Type::I64?"int64_t":t==Type::Bool?"bool":"void";
  if(auto found=names.find(t);found!=names.end())return found->second;
  std::vector<std::string> fields;for(const auto& field:t.elements())fields.push_back(ctype(field));
  auto name="on1x_mir_tuple_"+std::to_string(names.size()+1);names.emplace(t,name);
  declarations<<"typedef struct {\n";
  for(std::size_t i=0;i<fields.size();++i)declarations<<"  "<<fields[i]<<" field_"<<i<<";\n";
  declarations<<"} "<<name<<";\n";return name;
 }
};
std::string value(Id id){return "v"+std::to_string(id);}
std::string name(Id id){return "on1x_mir_f"+std::to_string(id);}
std::string literal(std::int64_t n){
 if(n==std::numeric_limits<std::int64_t>::min())return "(-INT64_C(9223372036854775807)-INT64_C(1))";
 return "INT64_C("+std::to_string(n)+")";
}
void signature(std::ostream& out,const Function& f,Types& types){
 out<<types.ctype(f.result)<<" "<<name(f.id)<<"(";const auto& params=f.blocks.front().parameters;
 if(params.empty())out<<"void";
 for(std::size_t n=0;n<params.size();++n){if(n)out<<", ";out<<types.ctype(params[n].type)<<" "<<value(params[n].id);}
 out<<")";
}
}
std::string emit_c(const Module& module){
 verify(module);const Function* main=nullptr;
 for(const auto& f:module.functions)if(f.name=="main")main=&f;
 if(!main||!main->blocks.front().parameters.empty()||(main->result!=Type::I64&&main->result!=Type::Void))
  throw Error({"<mir>"},"ON1X-MIR-C11: main must have no parameters and return i64 or void");
 Types types;
 for(const auto& f:module.functions){
  types.ctype(f.result);for(const auto& slot:f.slots)types.ctype(slot.type);
  for(const auto& b:f.blocks){for(const auto& p:b.parameters)types.ctype(p.type);for(const auto& i:b.instructions)types.ctype(i.type);}
 }
 std::ostringstream out;out<<"/* On1x scalar MIR v"<<module.version<<": checked C11 lowering. */\n#include <on1x_runtime.h>\n\n";
  if(types.floats)out<<"#if !defined(ON1X_F64_ABI_VERSION) || ON1X_F64_ABI_VERSION != 1\n#error \"unsupported On1x f64 target or runtime ABI\"\n#endif\n"
   "#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n#error \"On1x f64 forbids fast/finite-only math and relaxed IEEE modes\"\n#endif\n\n";
  if(types.singles)out<<"#if !defined(ON1X_F32_ABI_VERSION) || ON1X_F32_ABI_VERSION != 1\n#error \"unsupported On1x f32 runtime ABI\"\n#endif\n";
  if(types.words)out<<"#if !defined(ON1X_UNSIGNED_ABI_VERSION) || ON1X_UNSIGNED_ABI_VERSION != 1\n#error \"unsupported On1x unsigned runtime ABI\"\n#endif\n";
 out<<types.declarations.str();
 for(const auto& f:module.functions){signature(out,f,types);out<<";\n";}
 for(const auto& f:module.functions){
  std::map<Id,Type> value_types;std::map<Id,const Block*> blocks;
  for(const auto& b:f.blocks){blocks.emplace(b.id,&b);for(const auto& p:b.parameters)value_types.emplace(p.id,p.type);for(const auto& i:b.instructions)value_types.emplace(i.id,i.type);}
  signature(out,f,types);out<<" {\n";
  for(const auto& p:f.blocks.front().parameters)out<<"  (void)"<<value(p.id)<<";\n";
  auto declare=[&](Id id,const Type& type){if(type!=Type::Void)out<<"  "<<types.ctype(type)<<" "<<value(id)<<" = "<<(type.is_tuple()?"{0}":"0")<<"; (void)"<<value(id)<<";\n";};
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
    case Op::Constant:
     if(i.type==Type::F64)out<<"on1x_f64_from_bits("<<literal(i.immediate)<<")";
      else if(i.type==Type::F32)out<<"on1x_f32_from_bits("<<literal(i.immediate)<<")";
      else if(i.type.is_unsigned())out<<"UINT64_C("<<std::bit_cast<std::uint64_t>(i.immediate)<<")";
     else out<<(i.type==Type::Bool?(i.immediate?"true":"false"):literal(i.immediate));
     break;
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
      out<<"on1x_print_"<<type_name(type)<<"("<<a<<")";break;
    }
    case Op::Assert:out<<"on1x_assert("<<a<<")";break;
    case Op::Tuple:
     out<<"("<<types.ctype(i.type)<<"){";
     for(std::size_t n=0;n<i.operands.size();++n){if(n)out<<", ";out<<value(i.operands[n]);}
     out<<"}";break;
     case Op::Extract:out<<a<<".field_"<<i.immediate;break;
     case Op::Numeric:{
      if(detail::unsigned_comparison(i.primitive)){
       auto method=i.primitive.substr(0,i.primitive.find('_'));
       out<<"("<<a<<" "<<(method=="eq"?"==":method=="ne"?"!=":method=="lt"?"<":method=="le"?"<=":method=="gt"?">":">=")<<" "<<c<<")";
      }else{
       out<<"on1x_"<<i.primitive<<"(";
       for(std::size_t n=0;n<i.operands.size();++n){if(n)out<<", ";out<<value(i.operands[n]);}
       out<<")";
      }
      break;
     }
    case Op::FloatAdd:case Op::FloatSub:case Op::FloatMul:case Op::FloatDiv:case Op::FloatNeg:
    case Op::FloatEq:case Op::FloatNe:case Op::FloatLt:case Op::FloatLe:case Op::FloatGt:case Op::FloatGe:
    case Op::FloatFromI64:case Op::FloatToI64:case Op::FloatFromBits:case Op::FloatToBits:
    case Op::FloatIsNaN:case Op::FloatIsFinite:case Op::FloatSignbit:{
     std::string method(op_name(i.op).substr(4));std::replace(method.begin(),method.end(),'-','_');
     out<<(i.op<=Op::FloatGe?"on1x_"+method+"_f64":"on1x_f64_"+method)<<"("<<a;
     if(i.operands.size()==2)out<<", "<<c;
     out<<")";break;
    }
    }
    out<<";\n";
   }
   const auto& t=b.exit;
   auto edge=[&](const Edge& e){
    const Block* target=blocks.at(e.block);
    out<<" {\n";
    // Phi transfers are simultaneous, even when an edge permutes loop values.
    for(std::size_t n=0;n<e.arguments.size();++n)out<<"    "<<types.ctype(target->parameters[n].type)<<" edge"<<n<<" = "<<value(e.arguments[n])<<";\n";
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

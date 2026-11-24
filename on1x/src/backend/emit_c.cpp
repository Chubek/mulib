#include <on1x/backend/mir.hpp>
#include <on1x/backend/mir_c11.hpp>
#include <on1x/backend/mir_directives.hpp>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <bit>
#include "numeric.hpp"
#include "../../hir/MEMGraph/storage.hpp"

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
std::string emit_c(const Module& module){return emit_c(module,std::nullopt);}

std::string emit_c(const Module& module,std::optional<std::size_t> stack_bytes){
 return emit_directed_c(module,stack_bytes,{},false);
}

std::string emit_directed_c(const Module& input,std::optional<std::size_t> stack_bytes,
                            std::string_view target,bool speculate){
   if(stack_bytes&&*stack_bytes>memgraph::max_stack_bytes)
    throw std::invalid_argument("MEMGraph scratch stack byte budget exceeds 65536");
 std::optional<Module> selected;
 std::set<Id> hot,hidden;
 if(input.version==directive_format_version){
  selected=specialize(input,"mir-c11",target,speculate);
  for(const auto& d:input.directives){
   if(d.kind!="hint"||(!d.backend.empty()&&d.backend!="mir-c11")||
      (!d.target.empty()&&d.target!=target)||(d.speculative&&!speculate))continue;
   if(d.action=="kernel-hot")hot.insert(d.function);
   if(d.action=="private-code")hidden.insert(d.function);
  }
 }else verify(input);
 const auto& module=selected?*selected:input;
 const Function* main=nullptr;
 for(const auto& f:module.functions)if(f.name=="main")main=&f;
 if(!main||!main->blocks.front().parameters.empty()||(main->result!=Type::I64&&main->result!=Type::Void))
  throw Error({"<mir>"},"ON1X-MIR-C11: main must have no parameters and return i64 or void");
 Types types;
 for(const auto& f:module.functions){
  types.ctype(f.result);for(const auto& slot:f.slots)types.ctype(slot.type);
  for(const auto& b:f.blocks){for(const auto& p:b.parameters)types.ctype(p.type);for(const auto& i:b.instructions)types.ctype(i.type);}
 }
  std::ostringstream out;out<<"/* On1x scalar MIR v"<<module.version<<": checked C11 lowering. */\n#include <on1x_runtime.h>\n\n";
  const std::string cell="on1x_mir_memgraph_cell";
  using memgraph::Placement;
  using Plan=std::map<Id,Placement>;
  std::map<Id,Plan> placement;
  if(stack_bytes){
   bool have_heap=false;
   for(const auto& f:module.functions){
    auto plan=memgraph::slot_placement(f,*stack_bytes);
    have_heap|=std::any_of(plan.begin(),plan.end(),[](const auto& entry){return entry.second==Placement::Heap;});
    if(!plan.empty())placement.emplace(f.id,std::move(plan));
   }
   if(!placement.empty())out<<memgraph::c11_scratch_layout();
   if(have_heap)out<<memgraph::c11_heap_cells(cell);
  }
  if(types.floats)out<<"#if !defined(ON1X_F64_ABI_VERSION) || ON1X_F64_ABI_VERSION != 1\n#error \"unsupported On1x f64 target or runtime ABI\"\n#endif\n"
   "#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n#error \"On1x f64 forbids fast/finite-only math and relaxed IEEE modes\"\n#endif\n\n";
  if(types.singles)out<<"#if !defined(ON1X_F32_ABI_VERSION) || ON1X_F32_ABI_VERSION != 1\n#error \"unsupported On1x f32 runtime ABI\"\n#endif\n";
  if(types.words)out<<"#if !defined(ON1X_UNSIGNED_ABI_VERSION) || ON1X_UNSIGNED_ABI_VERSION != 1\n#error \"unsupported On1x unsigned runtime ABI\"\n#endif\n";
 out<<types.declarations.str();
  if(!hot.empty()||!hidden.empty())
   out<<"#if defined(__GNUC__) || defined(__clang__)\n"
        "#define ON1X_MIR_HOT __attribute__((hot))\n"
        "#else\n#define ON1X_MIR_HOT\n#endif\n"
        "#if defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))\n"
        "#define ON1X_MIR_PRIVATE __attribute__((visibility(\"hidden\")))\n"
        "#else\n#define ON1X_MIR_PRIVATE\n#endif\n";
  for(const auto& f:module.functions){
   if(hot.contains(f.id)||hot.contains(0))out<<"ON1X_MIR_HOT ";
   if(hidden.contains(f.id)||hidden.contains(0))out<<"ON1X_MIR_PRIVATE ";
   signature(out,f,types);out<<";\n";
  }
 for(const auto& f:module.functions){
  std::map<Id,Type> value_types;std::map<Id,const Block*> blocks;
  for(const auto& b:f.blocks){blocks.emplace(b.id,&b);for(const auto& p:b.parameters)value_types.emplace(p.id,p.type);for(const auto& i:b.instructions)value_types.emplace(i.id,i.type);}
   signature(out,f,types);out<<" {\n";
   const Plan no_placement;
   const auto& plan=placement.contains(f.id)?placement.at(f.id):no_placement;
   auto is_heap=[&](Id id){const auto it=plan.find(id);return it!=plan.end()&&it->second==Placement::Heap;};
   const bool cleanup=std::any_of(plan.begin(),plan.end(),[](const auto& p){return p.second==Placement::Heap;})&&
    std::any_of(f.blocks.begin(),f.blocks.end(),[](const auto& b){return b.exit.kind==Exit::Return;});
   if(cleanup&&f.result!=Type::Void)
    out<<"  "<<types.ctype(f.result)<<" on1x_memgraph_result;\n";
  for(const auto& p:f.blocks.front().parameters)out<<"  (void)"<<value(p.id)<<";\n";
  auto declare=[&](Id id,const Type& type){if(type!=Type::Void)out<<"  "<<types.ctype(type)<<" "<<value(id)<<" = "<<(type.is_tuple()?"{0}":"0")<<"; (void)"<<value(id)<<";\n";};
   for(const auto& s:f.slots){
    if(is_heap(s.id))
     out<<"  "<<cell<<" "<<value(s.id)<<" = {0}; /* MEMGraph: heap slot */\n";
    else if(plan.contains(s.id))
     out<<"  int64_t "<<value(s.id)<<" = 0; (void)"<<value(s.id)<<"; /* MEMGraph: stack slot */\n";
    else declare(s.id,s.type);
   }
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
      // Slot forwarding can turn two scalar reads into the same SSA value.
      // These non-floating comparisons are reflexive and their operands
      // have already been evaluated; avoid a tautological C comparison.
      if(i.operands[0]==i.operands[1]){
       out<<(i.op==Op::Eq||i.op==Op::Le||i.op==Op::Ge?"true":"false");break;
      }
      const char* op=i.op==Op::Eq?"==":i.op==Op::Ne?"!=":i.op==Op::Lt?"<":i.op==Op::Le?"<=":i.op==Op::Gt?">":">=";
     out<<"("<<a<<" "<<op<<" "<<c<<")";break;
    }
     case Op::Load:
      if(is_heap(i.target))out<<"*"<<cell<<"_ptr(&"<<value(i.target)<<")";
      else out<<value(i.target);
      break;
     case Op::Store:
      if(is_heap(i.target)){
       const auto slot=value(i.target);
       out<<"if (!"<<slot<<".data) "<<cell<<"_alloc(&"<<slot<<", "<<a<<", \"<mir>\", "
          <<i.where.line<<", "<<i.where.column<<"); else *"<<cell<<"_ptr(&"<<slot<<") = "<<a;
      }else out<<value(i.target)<<" = "<<a;
      break;
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
        if(i.operands[0]==i.operands[1])out<<(method=="eq"||method=="le"||method=="ge"?"true":"false");
        else out<<"("<<a<<" "<<(method=="eq"?"==":method=="ne"?"!=":method=="lt"?"<":method=="le"?"<=":method=="gt"?">":">=")<<" "<<c<<")";
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
    if(t.kind==Exit::Return){
     if(cleanup){
      if(t.value)out<<"  on1x_memgraph_result = "<<value(t.value)<<";\n";
      out<<"  goto on1x_memgraph_return;\n";
     }else{
      out<<"  return";if(t.value)out<<" "<<value(t.value);out<<";\n";
     }
    }
   else if(t.kind==Exit::Jump){out<<" ";edge(t.edges[0]);}
   else{out<<"  if ("<<value(t.value)<<")";edge(t.edges[0]);out<<"  else";edge(t.edges[1]);}
  }
  if(cleanup){
   out<<"on1x_memgraph_return:;\n";
   for(const auto& [id,where]:plan)if(where==Placement::Heap)
    out<<"  free("<<value(id)<<".data);\n";
   out<<"  return"<<(f.result==Type::Void?"":" on1x_memgraph_result")<<";\n";
  }
  out<<"}\n\n";
 }
 if(main->result==Type::Void)out<<"int main(void) { "<<name(main->id)<<"(); return 0; }\n";
 else out<<"int main(void) { return (int)"<<name(main->id)<<"(); }\n";
 return out.str();
}
}

#include <on1x/backend/mir.hpp>
#include "checked.hpp"
#include "floating.hpp"
#include "numeric_eval.hpp"
#include <algorithm>
#include <limits>
#include <map>

namespace on1x::mir {
namespace {
struct Halt {Status status;Location where;std::string diagnostic;};
class Evaluator {
 std::map<Id,const Function*> functions;
 struct Index {std::map<Id,const Block*> blocks;std::map<Id,Type> types;std::size_t entries=0;};
 std::map<Id,Index> indices;
 Evaluation result;std::size_t remaining,depth_limit;
 std::size_t live_entries=0;
 void tick(const Location& at){
  if(!remaining)throw Halt{Status::BudgetExhausted,at,"evaluation step budget exhausted"};
  --remaining;++result.steps;
 }
 [[noreturn]] void trap(const Location& at,const char* message){throw Halt{Status::Trapped,at,message};}
   std::int64_t arithmetic(Op op,std::int64_t a,std::int64_t b,const Location& at){
   auto value=detail::arithmetic(op,a,b);if(value.trap)trap(at,value.trap);return value.value;
   }
 RuntimeValue copy(const RuntimeValue& value,const Location& at){
  auto charge=[&](auto&& self,const RuntimeValue& v)->void{for(const auto& child:v.elements){tick(at);self(self,child);}};
  charge(charge,value);return value;
 }
 RuntimeValue call(const Function& f,const std::vector<RuntimeValue>& args,std::size_t depth){
  if(depth>=depth_limit)throw Halt{Status::BudgetExhausted,f.where,"evaluation call-depth budget exhausted"};
  const auto& index=indices.at(f.id);const auto& blocks=index.blocks;const auto& types=index.types;
  if(index.entries>262144-live_entries)throw Halt{Status::BudgetExhausted,f.where,"evaluation frame-storage budget exhausted"};
  live_entries+=index.entries;
  struct Frame {std::size_t& live;std::size_t entries;~Frame(){live-=entries;}} frame{live_entries,index.entries};
  std::map<Id,RuntimeValue> values,slots;
  const Block* b=&f.blocks.front();std::vector<RuntimeValue> incoming=args;
  for(;;){
   tick(b->where);
   for(std::size_t n=0;n<incoming.size();++n)values[b->parameters[n].id]=copy(incoming[n],b->where);
   for(const auto& i:b->instructions){
    tick(i.where);RuntimeValue empty,value;
    const auto& first=i.operands.empty()?empty:values.at(i.operands[0]);
    auto a=first.scalar,c=i.operands.size()<2?0:values.at(i.operands[1]).scalar;
    switch(i.op){
    case Op::Constant:value.scalar=i.immediate;break;
    case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Neg:value.scalar=arithmetic(i.op,a,c,i.where);break;
    case Op::Not:value.scalar=!a;break;
    case Op::Eq:value.scalar=a==c;break;case Op::Ne:value.scalar=a!=c;break;
    case Op::Lt:value.scalar=a<c;break;case Op::Le:value.scalar=a<=c;break;case Op::Gt:value.scalar=a>c;break;case Op::Ge:value.scalar=a>=c;break;
    case Op::Load:value=copy(slots.at(i.target),i.where);break;
    case Op::Store:slots[i.target]=copy(first,i.where);break;
    case Op::Call:{
     std::vector<RuntimeValue> parameters;for(auto id:i.operands)parameters.push_back(copy(values.at(id),i.where));
     value=call(*functions.at(i.target),parameters,depth+1);break;
    }
    case Op::Print:{
      auto type=types.at(i.operands[0]);auto line=type==Type::Bool?std::string(a?"true":"false"):type==Type::F64?detail::float_text(a):type==Type::F32?detail::single_text(a):type.is_unsigned()?std::to_string(std::bit_cast<std::uint64_t>(a)):std::to_string(a);line+='\n';
     if(result.output.size()+line.size()>4*1024*1024)throw Halt{Status::BudgetExhausted,i.where,"evaluation output budget exhausted"};
     result.output+=line;break;
    }
    case Op::Assert:if(!a)trap(i.where,"assertion failed");break;
    case Op::Tuple:
     for(auto id:i.operands)value.elements.push_back(copy(values.at(id),i.where));
     break;
     case Op::Extract:value=copy(first.elements.at(static_cast<std::size_t>(i.immediate)),i.where);break;
     case Op::Numeric:{
      auto computed=detail::numeric(i.primitive,a,c,i.where);if(computed.trap)trap(i.where,computed.trap);value.scalar=computed.value;break;
     }
    default:{
     if(!detail::floating_op(i.op))throw std::logic_error("unmodeled MIR opcode");
     auto computed=detail::floating(i.op,a,c,i.where);if(computed.trap)trap(i.where,computed.trap);value.scalar=computed.value;break;
    }
    }
    if(i.type!=Type::Void)values[i.id]=std::move(value);
   }
   tick(b->exit.where);const auto& t=b->exit;
   if(t.kind==Exit::Return)return t.value?copy(values.at(t.value),t.where):RuntimeValue{};
   const auto& edge=t.edges[t.kind==Exit::Branch&&!values.at(t.value).scalar?1:0];
   incoming.clear();for(auto id:edge.arguments)incoming.push_back(copy(values.at(id),t.where));
   b=blocks.at(edge.block);
  }
 }
public:
  Evaluator(const Module& m,std::size_t steps,std::size_t depth):remaining(steps),depth_limit(std::min<std::size_t>(depth,256)){
   auto width=[&](auto&& self,const Type& t)->std::size_t{
    std::size_t n=1;if(t.is_tuple())for(const auto& field:t.elements()){auto child=self(self,field);if(child>262144-n)return 262145;n+=child;}return n;
   };
  for(const auto& f:m.functions){
    functions.emplace(f.id,&f);auto& index=indices[f.id];
    for(const auto& slot:f.slots)index.entries=std::min<std::size_t>(262145,index.entries+width(width,slot.type));
   for(const auto& b:f.blocks){
    index.blocks.emplace(b.id,&b);
     for(const auto& p:b.parameters){index.types.emplace(p.id,p.type);index.entries=std::min<std::size_t>(262145,index.entries+width(width,p.type));}
     for(const auto& i:b.instructions)if(i.type!=Type::Void){index.types.emplace(i.id,i.type);index.entries=std::min<std::size_t>(262145,index.entries+width(width,i.type));}
   }
  }
 }
 Evaluation run(const Function& f,const std::vector<RuntimeValue>& args){
  const auto& params=f.blocks.front().parameters;
  if(params.size()!=args.size())throw Error(f.where,"ON1X-MIR-EVAL: argument arity mismatch");
  auto validate=[&](auto&& self,const RuntimeValue& value,const Type& type,const Location& at)->void{
   if(type.is_tuple()){
    if(value.scalar||value.elements.size()!=type.elements().size())throw Error(at,"ON1X-MIR-EVAL: tuple argument shape mismatch");
    for(std::size_t i=0;i<value.elements.size();++i)self(self,value.elements[i],type.elements()[i],at);
   }else{
    if(!value.elements.empty())throw Error(at,"ON1X-MIR-EVAL: scalar argument has elements");
    if(type==Type::Bool&&value.scalar!=0&&value.scalar!=1)throw Error(at,"ON1X-MIR-EVAL: bool argument must be 0 or 1");
     if(type==Type::F64&&detail::canonical_float(std::bit_cast<std::uint64_t>(value.scalar))!=std::bit_cast<std::uint64_t>(value.scalar))throw Error(at,"ON1X-MIR-EVAL: noncanonical f64 NaN argument");
     auto bits=std::bit_cast<std::uint64_t>(value.scalar);
     if(type==Type::F32&&(bits>UINT32_MAX||detail::canonical_single(static_cast<std::uint32_t>(bits))!=bits))throw Error(at,"ON1X-MIR-EVAL: invalid binary32 argument payload");
     if(type.is_unsigned()&&bits>detail::unsigned_max(type))throw Error(at,"ON1X-MIR-EVAL: unsigned argument out of range");
   }
  };
  for(std::size_t n=0;n<args.size();++n)validate(validate,args[n],params[n].type,params[n].where);
  result.type=f.result;
  try{result.data=call(f,args,0);result.value=result.data.scalar;}
  catch(const Halt& halt){result.status=halt.status;result.diagnostic=halt.diagnostic;result.where=halt.where;}
  return std::move(result);
 }
};
}
Evaluation evaluate(const Module& module,std::string_view name,const std::vector<std::int64_t>& args,std::size_t steps,std::size_t depth){
 verify(module);
 for(const auto& f:module.functions)if(f.name==name){
  if(args.size()!=f.blocks.front().parameters.size())throw Error(f.where,"ON1X-MIR-EVAL: argument arity mismatch");
  std::vector<RuntimeValue> values;for(auto scalar:args)values.push_back({scalar,{}});
  return Evaluator(module,steps,depth).run(f,values);
 }
 throw Error({"<mir>"},"ON1X-MIR-EVAL: function not found");
}
Evaluation evaluate_values(const Module& module,std::string_view name,const std::vector<RuntimeValue>& args,std::size_t steps,std::size_t depth){
 verify(module);
 for(const auto& f:module.functions)if(f.name==name)return Evaluator(module,steps,depth).run(f,args);
 throw Error({"<mir>"},"ON1X-MIR-EVAL: function not found");
}
}

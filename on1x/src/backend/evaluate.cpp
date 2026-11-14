#include <on1x/backend/mir.hpp>
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
  constexpr auto lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();
  switch(op){
  case Op::Add:if((b>0&&a>hi-b)||(b<0&&a<lo-b))trap(at,"addition overflow");return a+b;
  case Op::Sub:if((b<0&&a>hi+b)||(b>0&&a<lo+b))trap(at,"subtraction overflow");return a-b;
  case Op::Mul:{
   // Unsigned magnitudes avoid both signed overflow and abs(INT64_MIN).
   auto magnitude=[](std::int64_t n){return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);};
   auto x=magnitude(a),y=magnitude(b);bool negative=(a<0)!=(b<0);
   auto bound=std::uint64_t(hi)+(negative?1:0);
   if(y&&x>bound/y)trap(at,"multiplication overflow");
   auto product=x*y;if(negative&&product==std::uint64_t(hi)+1)return lo;
   return negative?-static_cast<std::int64_t>(product):static_cast<std::int64_t>(product);
  }
  case Op::Div:if(!b||(a==lo&&b==-1))trap(at,"invalid division");return a/b;
  case Op::Mod:if(!b||(a==lo&&b==-1))trap(at,"invalid modulo");return a%b;
  case Op::Neg:if(a==lo)trap(at,"negation overflow");return -a;
  default:throw std::logic_error("invalid checked arithmetic opcode");
  }
 }
 std::int64_t call(const Function& f,const std::vector<std::int64_t>& args,std::size_t depth){
  if(depth>=depth_limit)throw Halt{Status::BudgetExhausted,f.where,"evaluation call-depth budget exhausted"};
  const auto& index=indices.at(f.id);const auto& blocks=index.blocks;const auto& types=index.types;
  if(index.entries>262144-live_entries)throw Halt{Status::BudgetExhausted,f.where,"evaluation frame-storage budget exhausted"};
  live_entries+=index.entries;
  struct Frame {std::size_t& live;std::size_t entries;~Frame(){live-=entries;}} frame{live_entries,index.entries};
  std::map<Id,std::int64_t> values,slots;
  const Block* b=&f.blocks.front();std::vector<std::int64_t> incoming=args;
  for(;;){
   tick(b->where);
   for(std::size_t n=0;n<incoming.size();++n)values[b->parameters[n].id]=incoming[n];
   for(const auto& i:b->instructions){
    tick(i.where);auto a=i.operands.empty()?0:values.at(i.operands[0]);
    auto c=i.operands.size()<2?0:values.at(i.operands[1]);std::int64_t value=0;
    switch(i.op){
    case Op::Constant:value=i.immediate;break;
    case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Neg:value=arithmetic(i.op,a,c,i.where);break;
    case Op::Not:value=!a;break;
    case Op::Eq:value=a==c;break;case Op::Ne:value=a!=c;break;
    case Op::Lt:value=a<c;break;case Op::Le:value=a<=c;break;case Op::Gt:value=a>c;break;case Op::Ge:value=a>=c;break;
    case Op::Load:value=slots.at(i.target);break;
    case Op::Store:slots[i.target]=a;break;
    case Op::Call:{
     std::vector<std::int64_t> parameters;for(auto id:i.operands)parameters.push_back(values.at(id));
     value=call(*functions.at(i.target),parameters,depth+1);break;
    }
    case Op::Print:{
     auto line=types.at(i.operands[0])==Type::Bool?std::string(a?"true":"false"):std::to_string(a);line+='\n';
     if(result.output.size()+line.size()>4*1024*1024)throw Halt{Status::BudgetExhausted,i.where,"evaluation output budget exhausted"};
     result.output+=line;break;
    }
    case Op::Assert:if(!a)trap(i.where,"assertion failed");break;
    }
    if(i.type!=Type::Void)values[i.id]=value;
   }
   tick(b->exit.where);const auto& t=b->exit;
   if(t.kind==Exit::Return)return t.value?values.at(t.value):0;
   const auto& edge=t.edges[t.kind==Exit::Branch&&!values.at(t.value)?1:0];
   incoming.clear();for(auto id:edge.arguments)incoming.push_back(values.at(id));
   b=blocks.at(edge.block);
  }
 }
public:
 Evaluator(const Module& m,std::size_t steps,std::size_t depth):remaining(steps),depth_limit(std::min<std::size_t>(depth,256)){
  for(const auto& f:m.functions){
   functions.emplace(f.id,&f);auto& index=indices[f.id];index.entries=f.slots.size();
   for(const auto& b:f.blocks){
    index.blocks.emplace(b.id,&b);
    for(const auto& p:b.parameters){index.types.emplace(p.id,p.type);++index.entries;}
    for(const auto& i:b.instructions)if(i.type!=Type::Void){index.types.emplace(i.id,i.type);++index.entries;}
   }
  }
 }
 Evaluation run(const Function& f,const std::vector<std::int64_t>& args){
  const auto& params=f.blocks.front().parameters;
  if(params.size()!=args.size())throw Error(f.where,"ON1X-MIR-EVAL: argument arity mismatch");
  for(std::size_t n=0;n<args.size();++n)if(params[n].type==Type::Bool&&args[n]!=0&&args[n]!=1)throw Error(params[n].where,"ON1X-MIR-EVAL: bool argument must be 0 or 1");
  result.type=f.result;
  try{result.value=call(f,args,0);}
  catch(const Halt& halt){result.status=halt.status;result.diagnostic=halt.diagnostic;result.where=halt.where;}
  return std::move(result);
 }
};
}
Evaluation evaluate(const Module& module,std::string_view name,const std::vector<std::int64_t>& args,std::size_t steps,std::size_t depth){
 verify(module);
 for(const auto& f:module.functions)if(f.name==name)return Evaluator(module,steps,depth).run(f,args);
 throw Error({"<mir>"},"ON1X-MIR-EVAL: function not found");
}
}

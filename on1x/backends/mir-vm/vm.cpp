#include "wire.hpp"
#include "On1xVM.h"
#include "checked.hpp"
#include "numeric_eval.hpp"
#include <algorithm>
#include <limits>
#include <map>

namespace on1x::vm {
namespace {
using namespace mir;
using V=RuntimeValue;
constexpr std::size_t frame_limit=262144;
struct Record {
 Location at;std::vector<std::size_t> src,params;
 struct Edge {std::size_t pc;std::vector<std::size_t> args;};
 std::vector<Edge> edges;
 std::size_t dst=0,target=0;std::int64_t imm=0;std::string primitive;Type type;
};
struct Layout {
 std::string name;Location at;Type result;std::vector<Type> args;
 std::size_t entry=0,registers=0,cells=0;
 std::map<Id,std::size_t> regs,blocks;
 std::map<Id,Type> types;
};
struct Image {
 std::vector<On1xVM_instruction> tape;
 std::vector<Record> desc;
 std::vector<Layout> functions;
 void put(unsigned op,Record d){
  On1xVM_instruction i{};i.opcode=op;i.operand_count=1;
  i.operands[0]=static_cast<std::int64_t>(desc.size());tape.push_back(i);desc.push_back(std::move(d));
 }
 static std::size_t width(const Type& t){
  std::size_t n=1;if(t.is_tuple())for(const auto& child:t.elements())n=std::min(frame_limit+1,n+width(child));return n;
 }
 explicit Image(const Module& m){
  std::size_t pc=0;std::map<Id,std::size_t> fn;
  for(const auto& f:m.functions){
   fn[f.id]=functions.size();Layout l;l.name=f.name;l.at=f.where;l.result=f.result;l.entry=pc;
   auto reg=[&](Id id,const Type& t){l.regs.emplace(id,l.registers++);l.types.emplace(id,t);
    l.cells=std::min(frame_limit+1,l.cells+width(t));};
   for(const auto& s:f.slots)reg(s.id,s.type);
   for(const auto& p:f.blocks.front().parameters)l.args.push_back(p.type);
   for(const auto& b:f.blocks){
    l.blocks[b.id]=pc;pc+=2+b.instructions.size();
    for(const auto& p:b.parameters)reg(p.id,p.type);
    for(const auto& i:b.instructions)if(i.type!=Type::Void)reg(i.id,i.type);
   }
   functions.push_back(std::move(l));
  }
  for(const auto& f:m.functions){
   const auto& l=functions[fn.at(f.id)];
   auto regs=[&](const std::vector<Id>& ids){
    std::vector<std::size_t> v;for(auto id:ids)v.push_back(l.regs.at(id));return v;};
   for(const auto& b:f.blocks){
    Record d;d.at=b.where;for(const auto& p:b.parameters)d.params.push_back(l.regs.at(p.id));
    put(On1xVM_opcode_Enter,std::move(d));
    for(const auto& i:b.instructions){
     Record op;op.at=i.where;op.src=regs(i.operands);op.imm=i.immediate;op.primitive=i.primitive;
     if(i.type!=Type::Void)op.dst=l.regs.at(i.id);
     if(i.op==Op::Call){op.target=fn.at(i.target);op.type=i.type;}
     if(i.op==Op::Load||i.op==Op::Store)op.target=l.regs.at(i.target);
     if(i.op==Op::Print)op.type=l.types.at(i.operands[0]);
     put(1+static_cast<unsigned>(i.op),std::move(op));
    }
    auto& t=b.exit;Record end;end.at=t.where;
    if(t.value)end.src={l.regs.at(t.value)};
    for(const auto& e:t.edges)end.edges.push_back({l.blocks.at(e.block),regs(e.arguments)});
    put(t.kind==Exit::Return?On1xVM_opcode_Return:t.kind==Exit::Jump?On1xVM_opcode_Jump:On1xVM_opcode_Branch,std::move(end));
   }
  }
 }
};
struct Stop {Status status;const char* message;};
struct State {
 const Image& image;std::size_t step_limit,depth_limit,live=0;Evaluation result;
 struct Frame {std::vector<V> r;std::size_t cells,dest;bool returns_value;};
 std::vector<Frame> frames;
 std::vector<V> incoming;
 State(const Image& i,std::size_t s,std::size_t d):image(i),step_limit(s),depth_limit(std::min<std::size_t>(d,256)){}
 void tick(){if(result.steps==step_limit)throw Stop{Status::BudgetExhausted,"evaluation step budget exhausted"};++result.steps;}
 void charge(const V& v){for(const auto& item:v.elements){tick();charge(item);}}
 V copy(const V& v){charge(v);return v;}
 void push(const Layout& f,std::size_t dst,bool returns){
  if(frames.size()>=depth_limit)throw Stop{Status::BudgetExhausted,"evaluation call-depth budget exhausted"};
  if(f.cells>frame_limit-live)throw Stop{Status::BudgetExhausted,"evaluation frame-storage budget exhausted"};
  frames.push_back({std::vector<V>(f.registers),f.cells,dst,returns});live+=f.cells;
 }
 void args(const std::vector<std::size_t>& regs){
  incoming.clear();std::size_t cells=0;
  for(auto r:regs){
   const auto& v=frames.back().r[r];
   auto count=[&](auto&& self,const V& value)->void{
    if(++cells>frame_limit)throw Stop{Status::BudgetExhausted,"evaluation argument-storage budget exhausted"};
    for(const auto& e:value.elements)self(self,e);};
   count(count,v);incoming.push_back(copy(v));
  }
 }
 template<unsigned op> std::int64_t exec(const Record& d){
  tick();auto& r=frames.back().r;
  auto scalar=[&](std::size_t k){return r[d.src[k]].scalar;};
  if constexpr(op==On1xVM_opcode_Enter){
   for(std::size_t i=0;i<d.params.size();++i){charge(incoming[i]);r[d.params[i]]=std::move(incoming[i]);}
   incoming.clear();
  }else if constexpr(op==On1xVM_opcode_Jump||op==On1xVM_opcode_Branch){
   std::size_t n=0;if constexpr(op==On1xVM_opcode_Branch)n=!scalar(0);
   const auto& e=d.edges[n];args(e.args);return e.pc;
  }else if constexpr(op==On1xVM_opcode_Call){
   args(d.src);const auto& f=image.functions[d.target];push(f,d.dst,d.type!=Type::Void);return f.entry;
  }else if constexpr(op==On1xVM_opcode_Return){
   V v=d.src.empty()?V{}:copy(r[d.src[0]]);auto dest=frames.back().dest;bool save=frames.back().returns_value;
   live-=frames.back().cells;frames.pop_back();
   if(frames.empty()){result.data=std::move(v);result.value=result.data.scalar;return -1;}
   if(save)frames.back().r[dest]=std::move(v);
  }else if constexpr(op==On1xVM_opcode_Tuple){
   V v;for(auto src:d.src)v.elements.push_back(copy(r[src]));r[d.dst]=std::move(v);
  }else if constexpr(op==On1xVM_opcode_Extract)r[d.dst]=copy(r[d.src[0]].elements[d.imm]);
  else if constexpr(op==On1xVM_opcode_Store)r[d.target]=copy(r[d.src[0]]);
  else if constexpr(op==On1xVM_opcode_Load)r[d.dst]=copy(r[d.target]);
  else if constexpr(op==On1xVM_opcode_Assert){if(!scalar(0))throw Stop{Status::Trapped,"assertion failed"};}
  else if constexpr(op==On1xVM_opcode_Print){
   auto a=scalar(0);auto text=d.type==Type::Bool?std::string(a?"true":"false"):
    d.type==Type::F64?detail::float_text(a):d.type==Type::F32?detail::single_text(a):
    d.type.is_unsigned()?std::to_string(std::bit_cast<std::uint64_t>(a)):std::to_string(a);
   if(result.output.size()+text.size()+1>4*1024*1024)throw Stop{Status::BudgetExhausted,"evaluation output budget exhausted"};
   result.output+=text;result.output+='\n';
  }else{
   constexpr auto k=static_cast<Op>(op-1);auto a=d.src.empty()?0:scalar(0),b=d.src.size()<2?0:scalar(1);
   std::int64_t value=0;
   if constexpr(k==Op::Constant)value=d.imm;
   else if constexpr(k>=Op::Add&&k<=Op::Neg){auto v=detail::arithmetic(k,a,b);if(v.trap)throw Stop{Status::Trapped,v.trap};value=v.value;}
   else if constexpr(k==Op::Not)value=!a;
   else if constexpr(k==Op::Eq)value=a==b;else if constexpr(k==Op::Ne)value=a!=b;
   else if constexpr(k==Op::Lt)value=a<b;else if constexpr(k==Op::Le)value=a<=b;
   else if constexpr(k==Op::Gt)value=a>b;else if constexpr(k==Op::Ge)value=a>=b;
   else if constexpr(k==Op::Numeric){auto v=detail::numeric(d.primitive,a,b,d.at);if(v.trap)throw Stop{Status::Trapped,v.trap};value=v.value;}
   else {static_assert(k>=Op::FloatAdd&&k<=Op::FloatSignbit);
    auto v=detail::floating(k,a,b,d.at);if(v.trap)throw Stop{Status::Trapped,v.trap};value=v.value;}
   r[d.dst].scalar=value;
  }
  return 0;
 }
 template<unsigned op> std::int64_t handler(std::int64_t index) noexcept {
  const auto& d=image.desc[std::size_t(index)];
  try{return exec<op>(d);}
  catch(const Stop& e){result.status=e.status;result.diagnostic=e.message;}
  catch(const std::exception& e){result.status=Status::Trapped;result.diagnostic=e.what();}
  result.where=d.at;return -2;
 }
};
void validate_arg(const V& v,const Type& t,const Location& at){
 bool ok=true;
 if(t.is_tuple()){ok=!v.scalar&&v.elements.size()==t.elements().size();
  if(ok)for(std::size_t i=0;i<v.elements.size();++i)validate_arg(v.elements[i],t.elements()[i],at);
 }else{auto bits=std::bit_cast<std::uint64_t>(v.scalar);ok=v.elements.empty();
  if(t==Type::Bool)ok&=bits<=1;
  if(t.is_unsigned())ok&=bits<=detail::unsigned_max(t);
  if(t==Type::F64)ok&=detail::canonical_float(bits)==bits;
  if(t==Type::F32)ok&=bits<=UINT32_MAX&&detail::canonical_single(static_cast<std::uint32_t>(bits))==bits;
 }
 if(!ok)throw Error(at,"ON1X-VM: invalid function argument");
}
}
#define O1VM_OP(name) \
 extern "C" std::int64_t o1vm_##name(void* state,std::int64_t index){ \
  return static_cast<State*>(state)->handler<On1xVM_opcode_##name>(index); }
#define O1VM_MIR(name) static_assert(On1xVM_opcode_##name==1+static_cast<unsigned>(mir::Op::name)); O1VM_OP(name)
O1VM_MIR(Constant) O1VM_MIR(Add) O1VM_MIR(Sub) O1VM_MIR(Mul) O1VM_MIR(Div)
O1VM_MIR(Mod) O1VM_MIR(Neg) O1VM_MIR(Not) O1VM_MIR(Eq) O1VM_MIR(Ne)
O1VM_MIR(Lt) O1VM_MIR(Le) O1VM_MIR(Gt) O1VM_MIR(Ge) O1VM_MIR(Load)
O1VM_MIR(Store) O1VM_MIR(Call) O1VM_MIR(Print) O1VM_MIR(Assert) O1VM_MIR(FloatAdd)
O1VM_MIR(FloatSub) O1VM_MIR(FloatMul) O1VM_MIR(FloatDiv) O1VM_MIR(FloatNeg)
O1VM_MIR(FloatEq) O1VM_MIR(FloatNe) O1VM_MIR(FloatLt) O1VM_MIR(FloatLe)
O1VM_MIR(FloatGt) O1VM_MIR(FloatGe) O1VM_MIR(FloatFromI64) O1VM_MIR(FloatToI64)
O1VM_MIR(FloatFromBits) O1VM_MIR(FloatToBits) O1VM_MIR(FloatIsNaN)
O1VM_MIR(FloatIsFinite) O1VM_MIR(FloatSignbit) O1VM_MIR(Tuple) O1VM_MIR(Extract) O1VM_MIR(Numeric)
O1VM_OP(Enter) O1VM_OP(Jump) O1VM_OP(Branch) O1VM_OP(Return)
#undef O1VM_MIR
#undef O1VM_OP
static mir::Evaluation execute(std::string_view bytes,std::string_view function,const std::vector<mir::RuntimeValue>& args,
                               std::size_t steps,std::size_t depth,bool process_entry){
 Image image(decode(bytes));auto f=std::find_if(image.functions.begin(),image.functions.end(),[&](const auto& x){return x.name==function;});
 if(f==image.functions.end())throw Error({"<o1vm>"},"ON1X-VM: function not found");
 if(process_entry&&(f->result!=Type::Void&&f->result!=Type::I64))
  throw Error(f->at,"ON1X-VM: main must return i64 or void");
 if(args.size()!=f->args.size())throw Error(f->at,"ON1X-VM: argument count mismatch");
 State state(image,steps,depth);state.result.type=f->result;
 try{state.push(*f,0,false);}
 catch(const Stop& e){state.result.status=e.status;state.result.diagnostic=e.message;state.result.where=f->at;return state.result;}
 for(std::size_t i=0;i<args.size();++i)validate_arg(args[i],f->args[i],f->at);
 state.incoming=args;On1xVM_vm vm;On1xVM_init(&vm);vm.state=&state;vm.vw_pc=f->entry;
 On1xVM_tape tape{image.tape.data(),image.tape.size(),image.tape.size()};
 // Individual handlers debit a shared instruction + tuple-copy budget. The
 // generated dispatch loop also enforces its own instruction budget.
 auto status=On1xVM_run(&vm,&tape,steps==SIZE_MAX?steps:steps+1);
 if(status&&state.result.status==Status::Returned)throw Error(f->at,"ON1X-VM: VMWeave dispatch error "+std::to_string(status));
 return std::move(state.result);
}
mir::Evaluation run(std::string_view bytes,std::string_view function,const std::vector<mir::RuntimeValue>& args,
                    std::size_t steps,std::size_t depth){return execute(bytes,function,args,steps,depth,false);}
mir::Evaluation run_main(std::string_view bytes,std::size_t steps,std::size_t depth){
 return execute(bytes,"main",{},steps,depth,true);
}
}

#include <on1x/backend/vm.hpp>
#include "numeric.hpp"
#include <bit>
#include <iostream>
#include <map>
#include <set>

namespace {
using namespace on1x;
using namespace on1x::mir;
const Location at{"vm-conformance.on1x",12,3,4};
void require(bool x,const std::string& why){if(!x)throw std::runtime_error(why);}
template<class F> void rejects(F f,const std::string& why){
 try{f();}catch(const Error&){return;}throw std::runtime_error("accepted "+why);
}
std::set<Op> covered;
std::set<std::string> descriptors;
struct Builder{
 Module m;Id fresh=5,token=4;
 Builder(unsigned version=3){
  m.version=version;mir::Function f;f.id=2;f.name="main";f.where=at;
  Block b;b.id=3;b.effect=4;b.where=at;f.blocks.push_back(b);m.functions.push_back(f);
 }
 Id add(Op op,Type t,std::vector<Id> inputs={},std::int64_t imm=0,Id target=0,std::string name={}){
  Id id=fresh++;Instruction i{id,t,op,std::move(inputs),imm,target,0,0,at,std::move(name)};
  if(effect(op)!=Effect::Pure){i.effect_in=token;i.effect_out=token=fresh++;}
  m.functions[0].blocks[0].instructions.push_back(i);covered.insert(op);
  if(op==Op::Numeric)descriptors.insert(i.primitive);
  return id;
 }
 Id constant(Type t,std::int64_t v){return add(Op::Constant,t,{},v);}
 Id slot(Type t){auto id=fresh++;m.functions[0].slots.push_back({id,t,at});return id;}
 void end(Id result=0,Type type=Type::Void){
  m.functions[0].result=type;m.functions[0].blocks[0].exit={fresh++,Exit::Return,result,token,{},at};
 }
};
Evaluation differential(const Module& m,std::size_t steps=1000000,std::size_t depth=256){
 auto bytes=vm::compile(m);vm::verify(bytes);require(bytes==vm::compile(m),"nondeterministic bytecode");
 auto a=evaluate(m,"main",{},steps,depth),b=vm::run(bytes,"main",{},steps,depth);
 require(a.status==b.status&&a.output==b.output&&a.diagnostic==b.diagnostic&&a.value==b.value&&
         a.type==b.type&&a.where.line==b.where.line&&a.steps==b.steps,"VM/reference mismatch: "+b.diagnostic);return b;
}
void instructions(){
 Builder x;auto i=x.constant(Type::I64,6),j=x.constant(Type::I64,2),yes=x.constant(Type::Bool,1);
 auto f=x.constant(Type::F64,std::bit_cast<std::int64_t>(6.0)),g=x.constant(Type::F64,std::bit_cast<std::int64_t>(2.0));
 auto print=[&](Id r){x.add(Op::Print,Type::Void,{r});};
 x.add(Op::Assert,Type::Void,{yes});print(x.add(Op::Not,Type::Bool,{yes}));
 for(auto op:{Op::Add,Op::Sub,Op::Mul,Op::Div,Op::Mod})print(x.add(op,Type::I64,{i,j}));
 print(x.add(Op::Neg,Type::I64,{j}));
 for(auto op:{Op::Eq,Op::Ne,Op::Lt,Op::Le,Op::Gt,Op::Ge})print(x.add(op,Type::Bool,{i,j}));
 for(auto op:{Op::FloatAdd,Op::FloatSub,Op::FloatMul,Op::FloatDiv})print(x.add(op,Type::F64,{f,g}));
 for(auto op:{Op::FloatEq,Op::FloatNe,Op::FloatLt,Op::FloatLe,Op::FloatGt,Op::FloatGe})print(x.add(op,Type::Bool,{f,g}));
 for(auto op:{Op::FloatFromI64,Op::FloatFromBits})print(x.add(op,Type::F64,{i}));
 for(auto op:{Op::FloatToI64,Op::FloatToBits})print(x.add(op,Type::I64,{f}));
 for(auto op:{Op::FloatIsNaN,Op::FloatIsFinite,Op::FloatSignbit})print(x.add(op,Type::Bool,{f}));
 print(x.add(Op::FloatNeg,Type::F64,{g}));
 auto tuple=Type::tuple({Type::I64,Type::F64});auto pair=x.add(Op::Tuple,tuple,{i,g});auto slot=x.slot(tuple);
 x.add(Op::Store,Type::Void,{pair},0,slot);print(x.add(Op::Extract,Type::F64,{x.add(Op::Load,tuple,{},0,slot)},1));
 // Enumerate the exact frontend descriptor families, including operators only
 // representable via MIR wire (e.g. f64_from_i64 under the Numeric opcode).
 std::set<std::string> names;
 for(const std::string t:{"f32","f64","u8","u16","u32","u64"}){
  for(auto suffix:{"from_i64","to_i64","from_bits","to_bits","is_nan","is_finite","signbit","wrapping_add","wrapping_sub",
                   "wrapping_mul","and","or","xor","not","shl","shr"})names.insert(t+"_"+suffix);
  for(auto s:{"f32","f64","u8","u16","u32","u64"})names.insert(t+"_from_"+s);
  for(auto op:{"add","sub","mul","div","mod","neg","eq","ne","lt","le","gt","ge"})names.insert(std::string(op)+"_"+t);
 }
 for(const auto& n:names)if(auto sig=detail::numeric_signature(n)){
  std::vector<Id> args;for(const auto& t:sig->parameters())args.push_back(x.constant(t,t==Type::F64?
   std::bit_cast<std::int64_t>(1.0):t==Type::F32?std::bit_cast<std::uint32_t>(1.f):1));
  print(x.add(Op::Numeric,sig->result(),args,0,0,n));
 }
 x.end();auto r=differential(x.m);require(r.status==Status::Returned&&descriptors.size()==155,"not all numeric descriptors executed");
}
void diamonds(){
 // Swap the same loop's block arguments on the backedge. Serial assignments
 // would print 20,20. The done block physically precedes the loop on tape.
 Builder x(1);auto a=x.constant(Type::I64,10),b=x.constant(Type::I64,20),yes=x.constant(Type::Bool,1);
 auto& f=x.m.functions.front();f.blocks[0].exit={8,Exit::Jump,0,4,{{9,{a,b,yes}}},at};
 Block loop;loop.id=9;loop.effect=10;loop.where=at;loop.parameters={{11,Type::I64,at},{12,Type::I64,at},{13,Type::Bool,at}};
 loop.instructions={{14,Type::Bool,Op::Constant,{},0,0,0,0,at}};
 loop.exit={15,Exit::Branch,13,10,{{9,{12,11,14}},{16,{11,12}}},at};
 Block done;done.id=16;done.effect=17;done.where=at;done.parameters={{18,Type::I64,at},{19,Type::I64,at}};
 done.instructions={{20,Type::Void,Op::Print,{18},0,0,17,21,at},{22,Type::Void,Op::Print,{19},0,0,21,23,at}};
 done.exit={24,Exit::Return,0,23,{},at};f.blocks.push_back(done);f.blocks.push_back(loop);
 require(differential(x.m).output=="20\n10\n","parallel edge copy");
 auto saved=vm::compile(x.m);
 auto portable=x.m;portable.version=directive_format_version;
 portable.directives={{"mir-vm","","hint","kernel-hot"}};
 rejects([&]{vm::compile(portable);},"unspecialized portable MIR");
 auto unsupported=saved;unsupported[std::string_view("O1VM\x01\r\n").size()]=directive_format_version;
 rejects([&]{vm::verify(unsupported);},"portable MIR version in executable VM wire");
 // IDs here fit a single varint. Locate the branch record via independent
 // wire marker [branch,exitID,condition,effect,edge count,first block].
 std::string marker{43,15,13,10,2,9};auto pos=saved.find(marker);require(pos!=saved.npos,"branch marker");
 auto corrupt=[&](std::size_t i,char c){auto v=saved;v[i]=c;rejects([&]{vm::verify(v);},"invalid branch wire");};
 corrupt(pos,99);corrupt(pos+2,12);corrupt(pos+3,3);corrupt(pos+4,1);corrupt(pos+5,99);
 for(std::size_t n=0;n<saved.size();++n)rejects([&]{vm::verify(std::string_view(saved).substr(0,n));},"truncated bytecode");
 rejects([&]{vm::verify(saved+"!");},"trailing bytecode");differential(x.m,3);
}
void calls(){
 auto u=parse(scan("fn double(x:i64)->i64{return x*2;}fn main(){println(double(double(5)));}","calls.on1x"));
 check(u);auto m=lower(u);
 require(differential(m).output=="20\n","nested VM calls");covered.insert(Op::Call);
 auto s=vm::compile(m);
 auto id=m.functions.front().id;
 for(auto& f:m.functions)for(auto& block:f.blocks)for(auto& i:block.instructions)if(i.op==Op::Call){
  // Call record, operand register, immediate 0, target. IDs are deliberately small.
  std::string marker{17,char(i.id),2,1,char(i.operands[0]),0,char(id)};
  auto pos=s.find(marker);require(pos!=s.npos,"call marker");auto bad=s;bad[pos+6]=127;
  rejects([&]{vm::run(bad);},"unknown callee");}
 require(vm::run(s,"double",{{-7,{}}}).value==-14,"external scalar arguments");
 require(vm::run_main(s,10000,1).status==Status::BudgetExhausted,"call-depth limit");
 auto tuple=Type::tuple({Type::Bool,Type::tuple({Type::U64,Type::F32})});Builder idfn;
 idfn.m.functions[0].blocks[0].parameters={{idfn.fresh++,tuple,at}};idfn.end(5,tuple);
 s=vm::compile(idfn.m);RuntimeValue v{0,{{1,{}},{0,{{-1,{}},{0x80000000LL,{}}}}}};
 require(vm::run(s,"main",{v}).data.elements[1].elements[0].scalar==-1,"nested tuple return");
 rejects([&]{vm::run(s,"main",{{1,{}}});},"tuple argument");rejects([&]{vm::run_main(s);},"invalid process entry");
 Builder trap(1);auto zero=trap.constant(Type::I64,0),min=trap.constant(Type::I64,INT64_MIN);
 trap.add(Op::Print,Type::Void,{zero});trap.add(Op::Neg,Type::I64,{min});trap.end();
 auto r=differential(trap.m);require(r.status==Status::Trapped&&r.output=="0\n"&&r.diagnostic=="negation overflow","checked trap");
}
void bounds(){
 // Each register fits independently, but aggregate frame widths, not module
 // node counts or host machine stack exhaustion, bound recursive execution.
 auto u=parse(scan("fn deep(n:i64)->i64{if n==0{return n;}return deep(n-1);}fn main(){println(deep(50));}","depth.on1x"));
 check(u);auto m=lower(u);
 auto bytes=vm::compile(m);auto r=vm::run_main(bytes,100000,8);
 require(r.status==Status::BudgetExhausted&&r.diagnostic=="evaluation call-depth budget exhausted","recursive frame limit");
 require(vm::run_main(bytes).output=="0\n","recursive calls");
 Builder wide(2);auto wide_type=Type::tuple(std::vector<Type>(257,Type::I64));
 for(unsigned i=0;i<1020;++i)wide.slot(wide_type);
 wide.end();
 auto& oversized=wide.m.functions.front();oversized.name="oversized";oversized.where={"callee.on1x",20,5,2};
 auto callee=oversized.id;auto where=oversized.where;
 mir::Function caller;caller.id=wide.fresh++;caller.name="main";caller.where=at;
 Block entry;entry.id=wide.fresh++;entry.effect=wide.fresh++;entry.where=at;
 auto call=wide.fresh++,after=wide.fresh++;
 entry.instructions={{call,Type::Void,Op::Call,{},0,callee,entry.effect,after,at}};
 entry.exit={wide.fresh++,Exit::Return,0,after,{},at};
 caller.blocks.push_back(std::move(entry));wide.m.functions.push_back(std::move(caller));
 bytes=vm::compile(wide.m);vm::verify(bytes);
 for(auto name:{"oversized","main"}){
  r=vm::run(bytes,name);
  require(r.status==Status::BudgetExhausted&&r.diagnostic=="evaluation frame-storage budget exhausted",
          "wide tuple frame limit");
  require(r.where.file==where.file&&r.where.offset==where.offset&&
          r.where.line==where.line&&r.where.column==where.column,"wrong rejected frame location");
 }
 // Keep embedded helpers/closure conversion in the sanitizer path as well.
 u=parse(scan("fn wrap(k:i64)->fn(i64)->i64{return fn(n:i64)->i64{return n+k;};}"
              "fn call(f:fn(i64)->i64)->i64{return f(5);}fn main(){println(call(wrap(37)));}",
              "captured.on1x"));check(u);
 m=lower_for_vm(u);require(differential(m).output=="42\n","VM closure lowering");
}
}
int main(){
 try{instructions();diamonds();calls();bounds();require(covered.size()==40,"missing MIR opcode");
  std::cout<<covered.size()<<" MIR opcodes, "<<descriptors.size()<<" numeric descriptors, calls/tuples/parallel CFG, traps, malformed tapes: PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

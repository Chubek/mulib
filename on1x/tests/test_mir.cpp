#include <on1x/backend/mir.hpp>
#include <iostream>
#include <fstream>
#include <limits>

namespace {
namespace m=on1x::mir;
using on1x::Type;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
m::Module lower(const std::string& source){auto u=on1x::parse(on1x::scan(source,"mir-test.on1x"));on1x::check(u);return m::lower(u);}
template<class F>void rejected(F operation,const std::string& reason){
 try{operation();}catch(const on1x::Error& e){
  if(std::string(e.what()).find(reason)==std::string::npos)throw std::runtime_error("expected "+reason+", got "+e.what());
  return;
 }
 throw std::runtime_error("expected rejection: "+reason);
}
template<class F>void malformed(const m::Module& source,F change,const std::string& reason){
 auto candidate=source;change(candidate);rejected([&]{m::verify(candidate);},reason);
}
void rethread(m::Block& b){
 auto token=b.effect;
 for(auto& i:b.instructions)if(m::effect(i.op)!=m::Effect::Pure){i.effect_in=token;token=i.effect_out;}
 b.exit.effect=token;
}
}
int main(int argc,char** argv){
 try{
  auto module=lower("fn twice(x:i64)->i64{return x*2;} fn main(){var n=0;while n<4{println(twice(n));n=n+1;} assert(n==4);}");
  auto wire=m::serialize(module);auto decoded=m::deserialize(wire);
  require(m::structurally_equal(module,decoded),"round-trip changed MIR");
  require(wire==m::serialize(decoded),"serialization is not canonical");
  require(m::emit_c(module)==m::emit_c(decoded),"round-trip changed executable C");
  require(m::cfg_dot(module)==m::cfg_dot(decoded),"round-trip changed CFG");
  auto execution=m::evaluate(decoded);
  require(execution.status==m::Status::Returned&&execution.output=="0\n2\n4\n6\n","loop/call evaluation mismatch");
  require(m::evaluate(module,"twice",{21}).value==42,"parameter evaluation mismatch");
  require(m::evaluate(module,"twice",{std::numeric_limits<std::int64_t>::min()}).status==m::Status::Trapped,"multiplication overflow lost");
  rejected([&]{m::evaluate(module,"twice",{});},"arity");
  rejected([&]{m::evaluate(module,"missing");},"not found");
  auto boolean=lower("fn flip(x:bool)->bool{return !x;} fn main(){}");
  rejected([&]{m::evaluate(boolean,"flip",{2});},"bool argument");
  require(m::evaluate(module,"main",{},1).status==m::Status::BudgetExhausted,"step budget not enforced");
  auto recursive=lower("fn again()->i64{return again();} fn main(){again();}");
  auto exhausted=m::evaluate(recursive,"main",{},100000,8);
  require(exhausted.status==m::Status::BudgetExhausted&&exhausted.diagnostic.find("call-depth")!=std::string::npos,"recursion budget not enforced");
  auto infinite=lower("fn main(){while true{}}");
  require(m::evaluate(infinite,"main",{},30).status==m::Status::BudgetExhausted,"loop budget not enforced");
  std::string large="fn again()->i64{";
  for(int n=0;n<1100;++n)large+="let x"+std::to_string(n)+"=1;";
  large+="return again();}fn main(){again();}";
  auto storage=m::evaluate(lower(large));
  require(storage.status==m::Status::BudgetExhausted&&storage.diagnostic.find("frame-storage")!=std::string::npos,"frame-storage budget not enforced");
  rejected([&]{m::verify(module,{1,1000});},"node budget");
  rejected([&]{m::verify(module,{65536,1});},"analysis budget");
  auto u=on1x::parse(on1x::scan("fn main(){println(42);}","transaction.on1x"));on1x::check(u);
  auto original=on1x::emit_c(u,"");rejected([&]{m::lower(u,{1,1});},"budget");
  require(original==on1x::emit_c(u,""),"failed lowering mutated HIR");
  require(m::serialize(m::lower(u))==m::serialize(m::lower(u)),"unstable lowering IDs");

  auto simple=lower("fn main(){println(1+2);}");
  malformed(simple,[](auto& m){m.version=2;},"version");
  malformed(simple,[](auto& m){m.id=0;},"identity");
  malformed(simple,[](auto& m){m.functions[0].id=m.id;},"identity");
  malformed(simple,[](auto& m){m.functions[0].blocks.clear();},"entry");
  malformed(simple,[](auto& m){m.functions[0].result=Type::String;},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].where.line=0;},"source location");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].type=Type::code(Type::I64);},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.kind=m::Exit::Invalid;},"terminator");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.effect=0;},"effects");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].effect_in=0;},"effect ordering");
  malformed(simple,[](auto& m){auto& i=m.functions[0].blocks[0].instructions;std::swap(i[2],i[3]);},"use before definition");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].op=static_cast<m::Op>(999);},"unknown opcode");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].type=Type::Json;},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[1].type=Type::Bool;},"bool constant");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].operands[0]=99999;},"non-value");
  malformed(simple,[](auto& m){auto& i=m.functions[0].blocks[0].instructions[2];i.operands[0]=i.id;},"use before definition");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].type=Type::Bool;},"type/arity");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].immediate=1;},"unexpected immediate");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].target=1;},"unexpected target");
  malformed(simple,[](auto& m){auto& b=m.functions[0].blocks[0];b.instructions[0].effect_in=b.effect;},"pure operation");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.value=m.functions[0].blocks[0].instructions[0].id;},"unexpected terminator value");
  malformed(module,[](auto& m){m.functions[0].blocks[0].exit.value=0;},"non-value");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges[0].block=9999;},"unknown CFG");
  malformed(module,[](auto& m){auto& b=m.functions[1].blocks[0];b.exit.edges[0].block=b.id;},"entry block cannot");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges.clear();},"edge count");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges[0].arguments={99};},"argument arity");
  malformed(module,[](auto& m){m.functions[1].blocks[1].exit.value=m.functions[1].blocks[0].instructions[0].id;},"requires bool");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Call)i.target=99999;},"call target");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Call)i.operands.clear();},"type/arity");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Load)i.target=99999;},"local slot");
  malformed(module,[](auto& m){auto& b=m.functions[1].blocks[0];b.instructions.pop_back();rethread(b);},"uninitialized slot");

  auto branches=lower("fn main(){if true{println(11);}else{println(22);}println(33);}");
  malformed(branches,[](auto& m){auto& b=m.functions[0].blocks;b[2].instructions[1].operands[0]=b[1].instructions[0].id;},"does not dominate");
  malformed(branches,[](auto& m){auto& b=m.functions[0].blocks;b[0].exit.edges[1]=b[0].exit.edges[0];},"unreachable");
  auto logic=lower("fn main(){println(true&&false);}");
  malformed(logic,[](auto& m){m.functions[0].blocks[2].parameters[0].type=Type::I64;},"argument type");
  auto effect_order=lower("fn main(){println(1);println(2);}");
  std::swap(effect_order.functions[0].blocks[0].instructions[1],effect_order.functions[0].blocks[0].instructions[2]);
  m::verify(effect_order);
  malformed(effect_order,[](auto& m){auto& i=m.functions[0].blocks[0].instructions;std::swap(i[2],i[3]);},"effect ordering");

  rejected([&]{m::deserialize("(on1x.mir 1 1 ()) extra");},"trailing input");
  rejected([&]{m::deserialize("(on1x.mir 999 1 ())");},"version");
  rejected([&]{m::deserialize("(on1x.mir 1 -1 ())");},"integer");
  rejected([&]{m::deserialize("(on1x.mir 1 1 () unknown)");},"fields");
  auto unknown=m::serialize(simple);unknown.replace(unknown.find("add.checked"),11,"unknown.op");
  rejected([&]{m::deserialize(unknown);},"unknown opcode");
  auto huge=std::string(4*1024*1024+1,' ');rejected([&]{m::deserialize(huge);},"byte limit");

  // Independently authored loop phis: swap must copy simultaneously. Loop
  // values do not depend on vector/block order or process-local identities.
  on1x::Location at{"phi.on1x",0,1,1};m::Module phi; m::Function f;f.id=2;f.name="main";f.where=at;
  m::Block entry;entry.id=3;entry.effect=4;entry.where=at;
  entry.instructions={{5,Type::I64,m::Op::Constant,{},10,0,0,0,at},{6,Type::I64,m::Op::Constant,{},20,0,0,0,at},{7,Type::Bool,m::Op::Constant,{},1,0,0,0,at}};
  entry.exit={8,m::Exit::Jump,0,4,{{9,{5,6,7}}},at};
  m::Block loop;loop.id=9;loop.effect=10;loop.where=at;loop.parameters={{11,Type::I64,at},{12,Type::I64,at},{13,Type::Bool,at}};
  loop.instructions={{14,Type::Bool,m::Op::Constant,{},0,0,0,0,at}};
  loop.exit={15,m::Exit::Branch,13,10,{{9,{12,11,14}},{16,{11,12}}},at};
  m::Block done;done.id=16;done.effect=17;done.where=at;done.parameters={{18,Type::I64,at},{19,Type::I64,at}};
  done.instructions={{20,Type::Void,m::Op::Print,{18},0,0,17,21,at},{22,Type::Void,m::Op::Print,{19},0,0,21,23,at}};
  done.exit={24,m::Exit::Return,0,23,{},at};f.blocks={entry,done,loop};phi.functions={f};
  m::verify(phi);require(m::evaluate(phi).output=="20\n10\n","parallel loop phi copy failed");
  if(argc==2){std::ofstream out(argv[1]);out<<m::emit_c(phi);require(bool(out),"cannot write phi C fixture");}
  std::cout<<"MIR verifier, round-trip, evaluator, budgets and transactional lowering PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

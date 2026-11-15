#include <on1x/backend/passes.hpp>
#include <on1x/meta.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <tuple>

namespace {
namespace m=on1x::mir;namespace p=m::passes;
using on1x::Type;
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
template<class F>void rejected(F operation,const std::string& reason){
 try{operation();}catch(const on1x::Error& e){require(std::string(e.what()).find(reason)!=std::string::npos,"unexpected rejection: "+std::string(e.what()));return;}
 throw std::runtime_error("expected rejection: "+reason);
}
m::Module lower(const std::string& text){auto u=on1x::parse(on1x::scan(text,"passes.on1x"));on1x::check(u);return m::lower(u);}
std::size_t count(const m::Module& module,m::Op op){
 std::size_t n=0;for(const auto& f:module.functions)for(const auto& b:f.blocks)for(const auto& i:b.instructions)n+=i.op==op;return n;
}
void equivalent(const m::Module& a,const m::Module& b,std::string function="main",std::vector<std::int64_t> args={}){
 auto x=m::evaluate(a,function,args),y=m::evaluate(b,function,args);
 require(x.status!=m::Status::BudgetExhausted&&y.status!=m::Status::BudgetExhausted,"inconclusive differential evaluation");
 require(x.status==y.status&&x.type==y.type&&x.value==y.value&&x.output==y.output&&x.diagnostic==y.diagnostic,"optimizer changed semantic observations");
 if(x.status==m::Status::Trapped)require(std::tie(x.where.file,x.where.offset,x.where.line,x.where.column)==std::tie(y.where.file,y.where.offset,y.where.line,y.where.column),"trap source point changed");
}
void rethread(m::Block& b){auto token=b.effect;for(auto& i:b.instructions)if(m::effect(i.op)!=m::Effect::Pure){i.effect_in=token;token=i.effect_out;}b.exit.effect=token;}
m::Module phis(){
 on1x::Location at{"pass-phis.on1x"};m::Module module;m::Function f;f.id=2;f.name="main";f.where=at;
 m::Block entry;entry.id=3;entry.effect=4;entry.where=at;
 entry.instructions={{5,Type::I64,m::Op::Constant,{},10,0,0,0,at},{6,Type::I64,m::Op::Constant,{},20,0,0,0,at},{7,Type::Bool,m::Op::Constant,{},1,0,0,0,at}};
 entry.exit={8,m::Exit::Jump,0,4,{{9,{5,6,7,5,6}}},at};
 m::Block loop;loop.id=9;loop.effect=10;loop.where=at;
 loop.parameters={{11,Type::I64,at},{12,Type::I64,at},{13,Type::Bool,at},{25,Type::I64,at},{26,Type::I64,at}};
 loop.instructions={{14,Type::Bool,m::Op::Constant,{},0,0,0,0,at}};
 loop.exit={15,m::Exit::Branch,13,10,{{9,{12,11,14,26,25}},{16,{11,12}}},at};
 m::Block done;done.id=16;done.effect=17;done.where=at;done.parameters={{18,Type::I64,at},{19,Type::I64,at}};
 done.instructions={{20,Type::Void,m::Op::Print,{18},0,0,17,21,at},{22,Type::Void,m::Op::Print,{19},0,0,21,23,at}};
 done.exit={24,m::Exit::Return,0,23,{},at};f.blocks={entry,done,loop};module.functions={f};return module;
}
}
int main(int argc,char** argv){
 try{
  auto source=lower("fn main(){let unused=1==2;println((2+3)*4);if (3<4)&&true{println(42);}else{println(1/0);}}");
  auto wire=m::serialize(source);auto optimized=p::run(source);
  require(optimized.status()==p::Status::Complete&&optimized.changed(),"pipeline did not complete/change fixture");
  require(optimized.source()==wire&&m::serialize(source)==wire,"source mutated or identity lost");
  require(optimized.output()==m::serialize(optimized.module()),"result artifact mismatch");
  equivalent(source,optimized.module());require(m::evaluate(optimized.module()).output=="20\n42\n","wrong folded output");
  require(count(optimized.module(),m::Op::Add)==0&&count(optimized.module(),m::Op::Mul)==0&&count(optimized.module(),m::Op::Div)==0,"fold/CFG did not remove eligible ops");
  auto again=p::run(optimized.module());require(!again.changed()&&again.status()==p::Status::Complete,"pipeline not idempotent at fixed point");
  require(p::report(optimized)==p::report(p::run(source)),"nondeterministic pass provenance");
  (void)on1x::meta::read_sexpr(p::report(optimized),"pass-report");
  p::Options budget;budget.work=0;auto empty=p::run(source,budget);
  require(empty.status()==p::Status::BudgetExhausted&&!empty.changed()&&empty.work()==0,"zero work failed to preserve source");
  budget={};budget.rewrites=1;auto midway=p::run(source,budget);
  require(midway.status()==p::Status::BudgetExhausted&&!midway.changed()&&midway.attempted_rewrites()==1,"partial folding escaped failed transaction");
  require(midway.events().back().output_revision==0,"failed candidate advanced revision");
  // Finish folding, exhaust during CFG cleanup: return exactly the folded prefix.
  budget={};budget.pipeline={p::Pass::ConstantFold};auto folded=p::run(source,budget);
  budget={};budget.rewrites=folded.attempted_rewrites();auto prefix=p::run(source,budget);
  require(prefix.status()==p::Status::BudgetExhausted&&prefix.output()==folded.output(),"failed later pass lost verified prefix");equivalent(source,prefix.module());
  budget={};budget.rounds=1;auto rounds=p::run(source,budget);
  require(rounds.status()==p::Status::BudgetExhausted&&rounds.changed(),"round exhaustion not distinguished");equivalent(source,rounds.module());
  // Sweep budgets across construction/analysis/rewrite boundaries.
  for(std::size_t work=0;work<optimized.work();work+=37){budget={};budget.work=work;auto limited=p::run(source,budget);m::verify(limited.module());equivalent(source,limited.module());require(limited.work()<=work,"work overspent");}
  budget={};budget.verification.nodes=1;rejected([&]{p::run(source,budget);},"node budget");
  budget={};budget.target="native";rejected([&]{p::run(source,budget);},"unsupported pass target");
  budget={};budget.pipeline={static_cast<p::Pass>(99)};rejected([&]{p::run(source,budget);},"unknown pass");
  budget={};budget.pipeline.clear();rejected([&]{p::run(source,budget);},"1..16 passes");
  budget={};budget.rounds=0;rejected([&]{p::run(source,budget);},"1..64 rounds");
  auto malformed=source;malformed.functions[0].blocks[0].exit.effect=0;
  rejected([&]{p::run(malformed);},"effects");
  require(m::serialize(source)==wire,"failed passes mutated source");

  // Preservation metadata is enforced even for structurally valid mutants.
  auto roots=lower("fn main(){println(1);println(2);}");auto mutant=roots;
  auto& b=mutant.functions[0].blocks[0];
  auto print=std::find_if(b.instructions.begin(),b.instructions.end(),[](auto& i){return i.op==m::Op::Print;});b.instructions.erase(print);rethread(b);m::verify(mutant);
  rejected([&]{p::verify_transition(roots,mutant,p::Pass::DeadPure);},"preservation of effects");
  mutant=roots;mutant.functions[0].name="renamed";
  rejected([&]{p::verify_transition(roots,mutant,p::Pass::ConstantFold);},"preservation of signatures");
  mutant=roots;mutant.functions[0].blocks[0].instructions[0].where.line++;
  rejected([&]{p::verify_transition(roots,mutant,p::Pass::DeadPure);},"source location");
  auto graph=lower("fn main(){if true{println(1);}else{println(2);}}");
  p::Options cfg;cfg.pipeline={p::Pass::SimplifyCFG};auto simplified=p::run(graph,cfg);
  rejected([&]{p::verify_transition(graph,simplified.module(),p::Pass::ConstantFold);},"preservation of cfg");

  for(const auto& expression:{"9223372036854775807+1","-9223372036854775808-1","9223372036854775807*2", "-9223372036854775808*-1",
    "-(-9223372036854775808)","1/0","-9223372036854775808/-1","1%0","-9223372036854775808%-1"}){
   auto trap=lower(std::string("fn main(){println(7);let unused=")+expression+";println(9);}");
   auto result=p::run(trap);equivalent(trap,result.module());
   require(m::evaluate(result.module()).status==m::Status::Trapped&&m::evaluate(result.module()).output=="7\n","unused trap deleted");
  }
  auto effects=lower("fn call(x:i64)->i64{println(x);return x;}fn main(){var n=0;n=call(1);let dead=n;assert(true);call(2);}");
  auto effects_after=p::run(effects);equivalent(effects,effects_after.module());
  for(auto op:{m::Op::Call,m::Op::Assert,m::Op::Print})require(count(effects,op)==count(effects_after.module(),op),"nonmemory effect root deleted");
  p::Options slots_only;slots_only.pipeline={p::Pass::LocalSlots};
  auto slots=lower("fn call(x:i64)->i64{println(x);return x;}fn main(){var n=1;let old=n;n=call(2);println(old);println(n);n=n+3;println(n);}");
  auto slots_wire=m::serialize(slots);auto slots_after=p::run(slots,slots_only);
  equivalent(slots,slots_after.module());require(m::evaluate(slots_after.module()).output=="2\n1\n2\n5\n","slot trace changed");
  require(count(slots_after.module(),m::Op::Load)==0&&count(slots_after.module(),m::Op::Store)==1,"local slots not eliminated");
  require(!p::run(slots_after.module(),slots_only).changed(),"slot pass not idempotent");
  for(std::size_t work=0;work<slots_after.work();work+=23){auto o=slots_only;o.work=work;auto r=p::run(slots,o);equivalent(slots,r.module());require(m::serialize(slots)==slots_wire,"slot budget mutated source");}
  auto limited_slots=slots_only;limited_slots.rewrites=1;
  auto rolled_slots=p::run(slots,limited_slots);require(!rolled_slots.changed()&&rolled_slots.status()==p::Status::BudgetExhausted,"partial slot edits escaped");
  // Each invocation owns its slots; recursive calls cannot mutate the caller.
  auto recursive_slots=lower("fn f(x:i64)->i64{var n=x;if x==0{return n;}let v=f(x-1);return n+v;}fn main(){println(f(4));}");
  equivalent(recursive_slots,p::run(recursive_slots).module());
  // Join inputs differ and loop-carried slots change every iteration.
  auto control_slots=lower("fn f(flag:bool)->i64{var x=1;if flag{x=7;}else{x=9;}return x+x;}fn main(){var i=0;while i<4{println(f(i<2));i=i+1;}}");
  auto control_after=p::run(control_slots,slots_only);equivalent(control_slots,control_after.module());
  require(count(control_after.module(),m::Op::Load)>0,"cross-block slot state invented");
  // Nonmemory barrier mutations reject even if effect chains are repaired.
  mutant=roots;mutant.functions[0].blocks[0].instructions.erase(mutant.functions[0].blocks[0].instructions.begin()+1);rethread(mutant.functions[0].blocks[0]);
  rejected([&]{p::verify_transition(roots,mutant,p::Pass::LocalSlots);},"slots/nonmemory barriers");
  auto unordered_slots=lower("fn f(flag:bool)->i64{var n=1;if flag{n=7;let x=n;if flag{return x;}return x+1;}return n;}fn main(){}");
  auto& blocks=unordered_slots.functions[0].blocks;std::reverse(blocks.begin()+1,blocks.end());
  auto unordered_after=p::run(unordered_slots,slots_only);
  equivalent(unordered_slots,unordered_after.module(),"f",{0});equivalent(unordered_slots,unordered_after.module(),"f",{1});
  auto slot_trap=lower("fn main(){var x=1;println(x);x=1/0;x=2;println(x);}");
  equivalent(slot_trap,p::run(slot_trap).module());
  auto infinite=lower("fn main(){while true{let dead=1==1;}}");auto infinite_after=p::run(infinite);
  require(m::evaluate(infinite_after.module(),"main",{},50).status==m::Status::BudgetExhausted,"CFG optimization removed divergence");
  auto recursion=lower("fn again()->i64{return again();}fn main(){again();}");auto rec_after=p::run(recursion);
  require(count(rec_after.module(),m::Op::Call)==2&&m::evaluate(rec_after.module(),"main",{},100,8).status==m::Status::BudgetExhausted,"DCE removed recursive call");

  auto phi=phis();auto phi_after=p::run(phi);equivalent(phi,phi_after.module());
  require(phi_after.module().functions[0].blocks[2].parameters.size()==3,"dead cyclic phis retained");
  require(phi_after.module().functions[0].blocks[2].exit.edges[0].arguments.size()==3,"phi backedge arguments not repaired");
  require(m::evaluate(phi_after.module()).output=="20\n10\n","live parallel phis changed");
  if(argc==2){std::ofstream out(argv[1]);out<<m::emit_c(phi_after.module());require(bool(out),"cannot emit optimized phi fixture");}
  auto dead_cycle=phi;auto& done=dead_cycle.functions[0].blocks[1];done.instructions.clear();done.exit.effect=done.effect;
  auto removed_cycle=p::run(dead_cycle);equivalent(dead_cycle,removed_cycle.module());
  require(removed_cycle.module().functions[0].blocks[2].parameters.size()==1,"dead pure value SCC not removed");

  // Same target but different argument vectors must not become a single edge.
  auto parallel=phi;auto& entry=parallel.functions[0].blocks[0];
  entry.parameters={{30,Type::Bool,entry.where}};entry.exit={8,m::Exit::Branch,30,4,{{16,{5,6}},{16,{6,5}}},entry.where};
  parallel.functions[0].blocks.erase(parallel.functions[0].blocks.begin()+2);m::verify(parallel);
  auto parallel_after=p::run(parallel);require(parallel_after.module().functions[0].blocks[0].exit.kind==m::Exit::Branch,"unequal phi transfers merged");
  equivalent(parallel,parallel_after.module(),"main",{0});equivalent(parallel,parallel_after.module(),"main",{1});
  parallel.functions[0].blocks[0].exit.edges[1]=parallel.functions[0].blocks[0].exit.edges[0];
  auto identical=p::run(parallel);require(identical.module().functions[0].blocks[0].exit.kind==m::Exit::Jump,"identical edges not simplified");
  equivalent(parallel,identical.module(),"main",{0});equivalent(parallel,identical.module(),"main",{1});

  // Different literal IDs with equal values can collapse a nonentry parameter.
  auto equal=lower("fn choose(flag:bool)->bool{return flag&&false;}fn main(){}");
  // Author both incoming values as literals; the source lowerer uses `flag` on
  // the false edge, which would require path-sensitive reasoning to fold.
  auto& equal_entry=equal.functions[0].blocks[0];
  equal_entry.instructions.push_back({1000000,Type::Bool,m::Op::Constant,{},0,0,0,0,equal_entry.where});
  equal_entry.exit.edges[1].arguments={1000000};
  auto equal_after=p::run(equal);equivalent(equal,equal_after.module(),"choose",{0});equivalent(equal,equal_after.module(),"choose",{1});
  for(const auto& block:equal_after.module().functions[0].blocks)if(block.id!=equal_after.module().functions[0].blocks[0].id)require(block.parameters.empty(),"uniform constant parameter not folded");
  // Pass order is explicit, verified at every step; alternate order converges.
  budget={};budget.pipeline={p::Pass::DeadPure,p::Pass::SimplifyCFG,p::Pass::ConstantFold};
  auto reordered=p::run(source,budget);require(reordered.status()==p::Status::Complete,"alternate pass order failed");equivalent(source,reordered.module());
  std::cout<<"MIR passes: semantics, phi cycles, preservation, budgets and rollback PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

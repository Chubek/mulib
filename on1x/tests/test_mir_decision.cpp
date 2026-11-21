#include <on1x/backend/mir.hpp>
#include <iostream>

namespace {
using namespace on1x;
namespace m=on1x::mir;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Unit fixture(){
 auto u=parse(scan("fn pick(c:bool,a:bool,b:bool)->bool{return c&&a;}fn main(){}","decision.on1x"));
 check(u);return u;
}
Expr& region(Unit& u){return *u.functions[0].body[0].value;}
ExprPtr local(const std::string& name){
 auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variable;e->type=Type::Bool;
 e->reference=Expr::Reference::Local;e->text=name;e->where={"decision.on1x",15,2,3};return e;
}
void rename(Unit& u,const std::string& name){
 auto& e=region(u);e.body[0].name=name;e.body[1].body[0].name=name;
 e.body[1].otherwise[0].name=name;e.children[0]->text=name;
}
template<class F> void rejected(F run,const std::string& reason){
 try{run();}catch(const Error& e){
  require(std::string(e.what()).find(reason)!=std::string::npos,"unexpected rejection");
  if(reason.find("budget")==std::string::npos)
   require(e.where.file=="decision.on1x","lost rejection source");
  return;
 }
 throw std::runtime_error("malformed decision accepted");
}
void truth(const m::Module& module){
 for(int c=0;c<2;++c)for(int a=0;a<2;++a)for(int b=0;b<2;++b)
  require(m::evaluate(module,"pick",{c,a,b}).value==(c?a:b),"decision truth table");
}
}
int main(){
 try{
  auto u=fixture();
  require(region(u).kind==Expr::Kind::Region,"processor Select did not execute");
  auto lazy=m::lower(u);
  require(lazy.functions[0].slots.empty(),"canonical boolean still uses a slot");
  require(lazy.functions[0].blocks.size()==3,"identity arm still uses an extra block");
  auto disjunction=parse(scan("fn pick(c:bool,a:bool,b:bool)->bool{return c||a;}fn main(){}","decision.on1x"));
  check(disjunction);auto either=m::lower(disjunction);
  require(either.functions[0].slots.empty()&&either.functions[0].blocks.size()==3,
          "true identity arm still uses storage/block");
  require(m::evaluate(either,"pick",{0,1,0}).value==1&&
          m::evaluate(either,"pick",{1,0,0}).value==1,"true identity result");
  auto identity=fixture();auto& true_arm=*region(identity).body[1].body[0].value;
  true_arm.kind=Expr::Kind::Boolean;true_arm.reference=Expr::Reference::Unresolved;true_arm.text="true";
  auto both=m::lower(identity);
  require(both.functions[0].blocks.size()==2&&both.functions[0].slots.empty(),
          "two identity arms need only the phi join");
  require(m::evaluate(both,"pick",{0,1,1}).value==0&&
          m::evaluate(both,"pick",{1,0,0}).value==1,"duplicate-target phi edges");
  // No private-prefix authority: arbitrary ordinary spelling has identical eligibility.
  rename(u,"temporary");
  region(u).body[1].otherwise[0].value=local("b");
  auto general=m::lower(u);truth(general);
  require(general.functions[0].slots.empty(),"general boolean decision still uses storage");
  require(general.functions[0].blocks.size()==4,"general decision CFG");
  require(m::structurally_equal(general,m::deserialize(m::serialize(general))),"phi round trip");
  require(m::serialize(general)==m::serialize(m::lower(u)),"unstable decision lowering");
  auto at=region(u).children[0]->where;bool located=false;
  for(std::size_t i=1;i<general.functions[0].blocks.size();++i)
   for(const auto& p:general.functions[0].blocks[i].parameters)
    located|=p.where.file==at.file&&p.where.offset==at.offset;
  require(located,"phi lost result source location");
  // A shadowed spelling in an initializer is legal but outside the proof:
  // ordinary storage lowering must preserve its lexical scope and value.
  auto shadow=fixture();rename(shadow,"temporary");
  auto nested=std::make_unique<Expr>();nested->kind=Expr::Kind::Region;
  nested->type=Type::Bool;nested->where=at;
  Stmt bind;bind.kind=Stmt::Kind::Let;bind.name="temporary";bind.where=at;
  bind.value=local("a");nested->body.push_back(std::move(bind));
  nested->children.push_back(local("temporary"));
  region(shadow).body[1].body[0].value=std::move(nested);
  auto fallback=m::lower(shadow);
  require(fallback.functions[0].slots.size()==1,"near-match incorrectly promoted");
  require(m::evaluate(fallback,"pick",{1,1,0}).value==1,"shadowed initializer changed");
  auto extra=fixture();
  Stmt action;action.kind=Stmt::Kind::Expr;action.where=at;action.value=local("b");
  region(extra).body[1].body.push_back(std::move(action));
  require(m::lower(extra).functions[0].slots.size()==1,"extra arm statement was erased");
  // The optional scan's depth cutoff declines promotion without rejecting a
  // valid bounded expression or changing its result.
  auto deep=fixture();auto& operand=region(deep).body[1].body[0].value;
  for(int i=0;i<130;++i){
   auto negate=std::make_unique<Expr>();negate->kind=Expr::Kind::Unary;
   negate->type=Type::Bool;negate->where=at;negate->text="!";
   negate->children.push_back(std::move(operand));operand=std::move(negate);
  }
  auto depth_fallback=m::lower(deep);
  require(depth_fallback.functions[0].slots.size()==1,"unbounded decision shape scan");
  require(m::evaluate(depth_fallback,"pick",{1,1,0}).value==1,"depth fallback changed result");
  // A terminating initializer has no incoming phi argument.
  auto terminal=fixture();rename(terminal,"temporary");
  auto stop=std::make_unique<Expr>();stop->kind=Expr::Kind::Region;
  stop->where=at;stop->type=Type::Void;stop->noreturn=true;
  Stmt ret;ret.kind=Stmt::Kind::Return;ret.where=at;ret.value=local("b");
  stop->body.push_back(std::move(ret));
  auto unit=std::make_unique<Expr>();unit->kind=Expr::Kind::Unit;unit->type=Type::Void;unit->where=at;
  stop->children.push_back(std::move(unit));region(terminal).body[1].body[0].value=std::move(stop);
  auto terminated=m::lower(terminal);
  require(terminated.functions[0].slots.empty(),"terminating arm was not lowered to SSA");
  require(m::evaluate(terminated,"pick",{1,0,1}).value==1,"terminating arm return lost");
   require(m::evaluate(terminated,"pick",{0,1,1}).value==0,"normal arm after terminating arm");
   auto stored=m::lower(terminal,{.decision_ssa=false});
   require(stored.functions[0].slots.size()==1,"disabled decision SSA still eliminated the slot");
   require(m::evaluate(stored,"pick",{1,0,1}).value==1&&
           m::evaluate(stored,"pick",{0,1,1}).value==0,"storage lowering lost a terminating arm");
  // Verify malformed near-matches before considering optimization.
  for(int change=0;change<5;++change){
   auto bad=fixture();auto& e=region(bad);
   if(change==0)e.body[1].body[0].value=local(e.body[0].name);
   if(change==1)e.body[1].otherwise.clear();
   if(change==2)e.children[0]->reference=Expr::Reference::Function;
   if(change==3)e.body[1].body[0].ref_store=true;
   if(change==4)e.body[0].declared=Type::I64;
   rejected([&]{(void)m::lower(bad);},change==3?"reference store":"canonical");
  }
  auto original=emit_c(u,"");
  rejected([&]{(void)m::lower(u,{8,4194304});},"node budget");
  rejected([&]{(void)m::lower(u,{65536,1});},"budget");
  require(original==emit_c(u,""),"budget failure changed checked HIR");
  std::cout<<"canonical bool decision SSA PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

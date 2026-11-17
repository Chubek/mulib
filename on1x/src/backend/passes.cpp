#include <on1x/backend/passes.hpp>
#include <on1x/meta.hpp>
#include "checked.hpp"
#include <algorithm>
#include <deque>
#include <set>
#include <tuple>

namespace on1x::mir::passes {
namespace {
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-MIR-PASS: "+message);}
struct Exhausted {Location where;std::string reason;};
struct Budget {
 std::size_t work,rewrites,used=0,edits=0;
 void tick(const Location& at,std::size_t n=1){
  if(n>work)throw Exhausted{at,"pass work budget exhausted"};
  work-=n;used+=n;
 }
 void edit(const Location& at){
  tick(at);if(!rewrites)throw Exhausted{at,"pass rewrite budget exhausted"};--rewrites;++edits;
 }
};
void charge_copy(const Module& m,Budget& budget){
 for(const auto& f:m.functions){
  budget.tick(f.where,1+f.name.size()+f.slots.size());
  for(const auto& b:f.blocks){
   budget.tick(b.where,1+b.parameters.size());
    for(const auto& i:b.instructions)budget.tick(i.where,1+i.operands.size()+i.where.file.size()+i.primitive.size());
   budget.tick(b.exit.where);for(const auto& e:b.exit.edges)budget.tick(b.exit.where,1+e.arguments.size());
  }
 }
}
using Inputs=std::map<Id,std::vector<Id>>;
Inputs phi_inputs(const Function& f,Budget& budget){
 Inputs inputs;std::map<Id,const Block*> blocks;
 for(const auto& b:f.blocks){budget.tick(b.where);blocks.emplace(b.id,&b);}
 for(const auto& b:f.blocks)for(const auto& e:b.exit.edges){
  budget.tick(b.exit.where);const auto& target=*blocks.at(e.block);
  for(std::size_t n=0;n<e.arguments.size();++n){budget.tick(b.exit.where);inputs[target.parameters[n].id].push_back(e.arguments[n]);}
 }
 return inputs;
}
// Repair every incoming edge using the original parameter positions. This is
// simultaneous even for cyclic parameters and duplicate branch destinations.
void remove_arguments(Function& f,const std::set<Id>& removed,Budget& budget){
 std::map<Id,std::vector<std::size_t>> keep;
 for(const auto& b:f.blocks){budget.tick(b.where);
  for(std::size_t n=0;n<b.parameters.size();++n){budget.tick(b.where);if(!removed.contains(b.parameters[n].id))keep[b.id].push_back(n);}
 }
 for(auto& b:f.blocks)for(auto& e:b.exit.edges){
  budget.tick(b.exit.where);std::vector<Id> arguments;
  for(auto n:keep[e.block]){budget.tick(b.exit.where);arguments.push_back(e.arguments[n]);}
  e.arguments=std::move(arguments);
 }
 for(auto& b:f.blocks)std::erase_if(b.parameters,[&](const Value& p){budget.tick(p.where);return removed.contains(p.id);});
}
std::optional<std::int64_t> fold(Op op,std::int64_t a,std::int64_t b){
 switch(op){
 case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Neg:{
  auto value=detail::arithmetic(op,a,b);if(value.trap)return {};return value.value;
 }
 case Op::Not:return !a;
 case Op::Eq:return a==b;case Op::Ne:return a!=b;
 case Op::Lt:return a<b;case Op::Le:return a<=b;case Op::Gt:return a>b;case Op::Ge:return a>=b;
 default:return {};
 }
}
void constant_fold(Module& m,Budget& budget){
 for(auto& f:m.functions){
  auto phis=phi_inputs(f,budget);Inputs users;
  std::map<Id,const Instruction*> instructions;std::map<Id,Location> locations;
  std::map<Id,std::int64_t> known,folded;std::deque<Id> pending;
  for(const auto& b:f.blocks){
   for(const auto& p:b.parameters){budget.tick(p.where);locations.emplace(p.id,p.where);if(phis.contains(p.id))pending.push_back(p.id);}
   for(const auto& i:b.instructions){
    budget.tick(i.where);instructions.emplace(i.id,&i);locations.emplace(i.id,i.where);
    if(i.op==Op::Constant)known.emplace(i.id,i.immediate);else pending.push_back(i.id);
    for(auto v:i.operands){budget.tick(i.where);users[v].push_back(i.id);}
   }
  }
  for(const auto& [p,args]:phis)for(auto v:args){budget.tick(locations.at(p));users[v].push_back(p);}
  while(!pending.empty()){
   auto id=pending.front();pending.pop_front();const auto& at=locations.at(id);budget.tick(at);
   if(known.contains(id))continue;
   std::optional<std::int64_t> value;
   if(auto p=phis.find(id);p!=phis.end()){
    bool all=!p->second.empty();
    for(auto v:p->second){
     budget.tick(at);auto k=known.find(v);
     if(k==known.end()||(value&&*value!=k->second)){all=false;break;}
     value=k->second;
    }
    if(!all)value.reset();
   }else{
    const auto& i=*instructions.at(id);bool all=!i.operands.empty();
    for(auto v:i.operands){budget.tick(at);if(!known.contains(v)){all=false;break;}}
    if(all)value=fold(i.op,known.at(i.operands[0]),i.operands.size()>1?known.at(i.operands[1]):0);
   }
   if(!value)continue;
   budget.edit(at);known.emplace(id,*value);folded.emplace(id,*value);
   for(auto user:users[id]){budget.tick(at);pending.push_back(user);}
  }
  std::set<Id> removed;
  // All analysis is complete before vectors move or parameter identities change
  // definition kind. The type and source point of each folded value are retained.
  for(auto& b:f.blocks){
   std::vector<Instruction> prefix;
   for(const auto& p:b.parameters){budget.tick(p.where);if(folded.contains(p.id)){
    removed.insert(p.id);prefix.push_back({p.id,p.type,Op::Constant,{},folded.at(p.id),0,0,0,p.where});
   }}
   Id token=b.effect;
   for(auto& i:b.instructions){
    budget.tick(i.where);
    if(folded.contains(i.id)){i.op=Op::Constant;i.immediate=folded.at(i.id);i.operands.clear();i.target=0;i.effect_in=i.effect_out=0;}
    if(effect(i.op)!=Effect::Pure){i.effect_in=token;token=i.effect_out;}
    prefix.push_back(std::move(i));
   }
   b.exit.effect=token;b.instructions=std::move(prefix);
  }
  remove_arguments(f,removed,budget);
 }
}
void simplify_cfg(Module& m,Budget& budget){
 for(auto& f:m.functions){
  std::map<Id,std::int64_t> constants;
  for(const auto& b:f.blocks)for(const auto& i:b.instructions){budget.tick(i.where);if(i.op==Op::Constant)constants.emplace(i.id,i.immediate);}
  std::map<Id,const Block*> blocks;
  for(auto& b:f.blocks){
   budget.tick(b.where);auto& t=b.exit;blocks.emplace(b.id,&b);
   if(t.kind!=Exit::Branch)continue;
   std::optional<std::size_t> selected;
   if(auto c=constants.find(t.value);c!=constants.end())selected=c->second?0:1;
   else{
    budget.tick(t.where,t.edges[0].arguments.size()+t.edges[1].arguments.size());
    if(t.edges[0].block==t.edges[1].block&&t.edges[0].arguments==t.edges[1].arguments)selected=0;
   }
   if(selected){budget.edit(t.where);auto edge=t.edges[*selected];t.kind=Exit::Jump;t.value=0;t.edges={std::move(edge)};}
  }
  std::set<Id> reachable;std::vector<Id> pending{f.blocks.front().id};
  while(!pending.empty()){
   auto id=pending.back();pending.pop_back();budget.tick(f.where);if(!reachable.insert(id).second)continue;
   for(const auto& e:blocks.at(id)->exit.edges){budget.tick(f.where);pending.push_back(e.block);}
  }
  std::erase_if(f.blocks,[&](const Block& b){budget.tick(b.where);if(reachable.contains(b.id))return false;budget.edit(b.where);return true;});
 }
}
void dead_pure(Module& m,Budget& budget){
 for(auto& f:m.functions){
  auto inputs=phi_inputs(f,budget);std::vector<Id> pending;std::set<Id> needed;
  for(const auto& b:f.blocks){
   budget.tick(b.where);if(b.exit.value)pending.push_back(b.exit.value);
   for(const auto& i:b.instructions){
    budget.tick(i.where,1+i.operands.size());inputs.emplace(i.id,i.operands);
    if(effect(i.op)!=Effect::Pure)pending.push_back(i.id);
   }
  }
  while(!pending.empty()){
   auto id=pending.back();pending.pop_back();budget.tick(f.where);if(!needed.insert(id).second)continue;
   if(auto it=inputs.find(id);it!=inputs.end())for(auto v:it->second){budget.tick(f.where);pending.push_back(v);}
  }
  std::set<Id> removed;
  for(auto& b:f.blocks){
   if(&b!=&f.blocks.front())for(const auto& p:b.parameters){budget.tick(p.where);if(!needed.contains(p.id)){budget.edit(p.where);removed.insert(p.id);}}
   std::erase_if(b.instructions,[&](const Instruction& i){
    budget.tick(i.where);if(needed.contains(i.id)||effect(i.op)!=Effect::Pure)return false;budget.edit(i.where);return true;
   });
  }
  remove_arguments(f,removed,budget);
 }
}
void local_slots(Module& m,Budget& budget){
 for(auto& f:m.functions){
  std::map<Id,Id> aliases;std::set<Id> removed;
  for(const auto& b:f.blocks){
   // Only same-block reaching values are evidence. A callee cannot access a
   // caller's slots: MIR has neither slot addresses nor indirect/foreign calls.
   std::map<Id,Id> values;std::map<Id,const Instruction*> stores;
   for(const auto& i:b.instructions){
    budget.tick(i.where);
    if(i.op==Op::Store){
     if(auto old=stores.find(i.target);old!=stores.end()){
      budget.edit(old->second->where);removed.insert(old->second->id);
     }
     stores[i.target]=&i;values[i.target]=i.operands[0];
    }else if(i.op==Op::Load){
     if(auto v=values.find(i.target);v!=values.end()){
      budget.edit(i.where);aliases.emplace(i.id,v->second);removed.insert(i.id);
     }else values[i.target]=i.id;
    }
   }
  }
  // Rewrite all uses after discovery, including dominating definitions used in
  // earlier-listed blocks and phi edges. Alias links follow strict dominance,
  // so cannot cycle; path compression keeps long forwarding chains bounded.
  auto resolve=[&](Id id,const Location& at){
   auto root=id;std::vector<Id> path;
   while(aliases.contains(root)){budget.tick(at);path.push_back(root);root=aliases.at(root);}
   for(auto v:path){budget.tick(at);aliases[v]=root;}return root;
  };
  for(auto& b:f.blocks){
   std::erase_if(b.instructions,[&](const Instruction& i){budget.tick(i.where);return removed.contains(i.id);});
   Id token=b.effect;
   for(auto& i:b.instructions){
    budget.tick(i.where);
    for(auto& v:i.operands){budget.tick(i.where);v=resolve(v,i.where);}
    if(effect(i.op)!=Effect::Pure){i.effect_in=token;token=i.effect_out;}
   }
   b.exit.effect=token;budget.tick(b.exit.where);
   if(b.exit.value)b.exit.value=resolve(b.exit.value,b.exit.where);
   for(auto& e:b.exit.edges)for(auto& v:e.arguments){budget.tick(b.exit.where);v=resolve(v,b.exit.where);}
  }
 }
}
// Exact preserved-analysis views. These are checked claims, not cache hints.
struct Views {
 using Signature=std::tuple<Id,std::string,Type,std::vector<std::pair<Id,Type>>>;
 std::vector<Signature> signatures;
 std::vector<std::tuple<Id,Id,Exit,std::vector<Id>>> cfg;
 std::vector<std::tuple<Id,Id,Id,Id,Type,std::vector<Id>>> calls;
  std::vector<std::tuple<Id,Id,Id,Op,Id,Id,Id,std::vector<Id>,std::string>> effects;
 std::map<Id,Location> locations;
 std::vector<std::tuple<Id,Id,Type>> slots;
  std::vector<std::tuple<Id,Id,Id,Op,Id,Type,std::string>> barriers;
};
Views views(const Module& m){
 Views v;
 for(const auto& f:m.functions){
  std::vector<std::pair<Id,Type>> parameters;for(const auto& p:f.blocks.front().parameters)parameters.emplace_back(p.id,p.type);
  v.signatures.emplace_back(f.id,f.name,f.result,std::move(parameters));v.locations.emplace(f.id,f.where);
  for(const auto& s:f.slots){v.locations.emplace(s.id,s.where);v.slots.emplace_back(f.id,s.id,s.type);}
  for(const auto& b:f.blocks){
   v.locations.emplace(b.id,b.where);v.locations.emplace(b.effect,b.where);
   for(const auto& p:b.parameters)v.locations.emplace(p.id,p.where);
   std::vector<Id> edges;for(const auto& e:b.exit.edges)edges.push_back(e.block);
   v.cfg.emplace_back(f.id,b.id,b.exit.kind,std::move(edges));
   // Boundary records also cover empty effect chains. Op is a fixed sentinel.
    v.effects.emplace_back(f.id,b.id,0,Op::Constant,0,b.effect,b.exit.effect,std::vector<Id>{},std::string{});
   for(const auto& i:b.instructions){
    v.locations.emplace(i.id,i.where);if(i.effect_out)v.locations.emplace(i.effect_out,i.where);
    if(i.op==Op::Call)v.calls.emplace_back(f.id,b.id,i.id,i.target,i.type,i.operands);
     if(effect(i.op)!=Effect::Pure)v.effects.emplace_back(f.id,b.id,i.id,i.op,i.target,i.effect_in,i.effect_out,i.operands,i.primitive);
     if(effect(i.op)!=Effect::Pure&&effect(i.op)!=Effect::Memory)v.barriers.emplace_back(f.id,b.id,i.id,i.op,i.target,i.type,i.primitive);
   }
   v.locations.emplace(b.exit.id,b.exit.where);
  }
 }
 return v;
}
}
const Descriptor& descriptor(Pass pass){
 static const Descriptor descriptors[]={
  {Pass::ConstantFold,"constant-fold",2,{"verified-mir"},{Analysis::Signatures,Analysis::CFG,Analysis::Calls},
    {Analysis::Effects,Analysis::Values,Analysis::Liveness},"remove only proven nontrapping integer arithmetic effects","portable-scalar-v1 / portable-numeric-product-v2 / portable-numeric-v3","shared work/rewrites; bounded rounds","MIR + preservation + retained source locations"},
  {Pass::SimplifyCFG,"simplify-cfg",2,{"verified-mir"},{Analysis::Signatures},
    {Analysis::CFG,Analysis::Calls,Analysis::Effects,Analysis::Values,Analysis::Liveness},"remove only statically unreachable paths","portable-scalar-v1 / portable-numeric-product-v2 / portable-numeric-v3","shared work/rewrites; bounded rounds","MIR + preservation + retained source locations"},
  {Pass::DeadPure,"dead-pure",2,{"verified-mir"},{Analysis::Signatures,Analysis::CFG,Analysis::Calls,Analysis::Effects},
    {Analysis::Values,Analysis::Liveness},"retain all effect roots and control flow","portable-scalar-v1 / portable-numeric-product-v2 / portable-numeric-v3","shared work/rewrites; bounded rounds","MIR + preservation + retained source locations"},
  {Pass::LocalSlots,"local-slots",2,{"verified-mir"},{Analysis::Signatures,Analysis::CFG},
    {Analysis::Calls,Analysis::Effects,Analysis::Values,Analysis::Liveness},"forward local loads; remove same-block overwritten stores","portable-scalar-v1 / portable-numeric-product-v2 / portable-numeric-v3","shared work/rewrites; bounded rounds","MIR + preservation + retained source locations + slots/nonmemory barriers"}
 };
 auto index=static_cast<std::size_t>(pass);if(index>=std::size(descriptors))fail({"<mir-pass>"},"unknown pass");return descriptors[index];
}
std::string_view analysis_name(Analysis analysis){
 switch(analysis){case Analysis::Signatures:return "signatures";case Analysis::CFG:return "cfg";case Analysis::Calls:return "calls";case Analysis::Effects:return "effects";case Analysis::Values:return "values";case Analysis::Liveness:return "liveness";}
 fail({"<mir-pass>"},"unknown analysis");
}
void verify_transition(const Module& before,const Module& after,Pass pass,mir::Limits limits){
 const auto& d=descriptor(pass);verify(before,limits);verify(after,limits);
 if(before.id!=after.id||before.version!=after.version)fail({"<mir-pass>"},"pass changed module identity/version");
 auto a=views(before),b=views(after);
 if(pass==Pass::LocalSlots&&(a.slots!=b.slots||a.barriers!=b.barriers))
  fail({"<mir-pass>"},"local-slots: changed slots/nonmemory barriers");
 for(auto preserved:d.preserves){
  bool equal=false;
  switch(preserved){
  case Analysis::Signatures:equal=a.signatures==b.signatures;break;
  case Analysis::CFG:equal=a.cfg==b.cfg;break;
  case Analysis::Calls:equal=a.calls==b.calls;break;
  case Analysis::Effects:equal=a.effects==b.effects;break;
  case Analysis::Values:case Analysis::Liveness:break; // No built-in pass claims these.
  }
  if(!equal)fail({"<mir-pass>"},std::string(d.name)+": violated preservation of "+std::string(analysis_name(preserved)));
 }
 for(const auto& [id,at]:b.locations){
  auto found=a.locations.find(id);
  if(found==a.locations.end()||std::tie(at.file,at.offset,at.line,at.column)!=std::tie(found->second.file,found->second.offset,found->second.line,found->second.column))
   fail(at,std::string(d.name)+": changed retained identity/source location");
 }
}
Result run(const Module& module,Options options){
  if(options.target!="portable-scalar-v1"&&options.target!="portable-numeric-product-v2"&&options.target!="portable-numeric-v3")fail({"<mir-pass>"},"unsupported pass target: "+options.target);
  if(module.version==format_version)options.target="portable-numeric-product-v2";
  if(module.version==numeric_format_version)options.target="portable-numeric-v3";
 if(options.pipeline.empty()||options.pipeline.size()>16||!options.rounds||options.rounds>64)fail({"<mir-pass>"},"pipeline requires 1..16 passes and 1..64 rounds");
 for(auto pass:options.pipeline)(void)descriptor(pass);
 verify(module,options.verification);Result result(module,serialize(module),options);
 Budget budget{options.work,options.rewrites};std::size_t revision=0;
 for(std::size_t round=0;round<options.rounds;++round){
  bool changed=false;
  for(auto pass:options.pipeline){
   Event event{pass,round,revision,revision,Outcome::Unchanged,0,0,{"<mir-pass>"},{}};
   auto start_work=budget.used,start_edits=budget.edits;
   try{
    charge_copy(result.module_,budget);auto candidate=result.module_;
    switch(pass){case Pass::ConstantFold:constant_fold(candidate,budget);break;case Pass::SimplifyCFG:simplify_cfg(candidate,budget);break;case Pass::DeadPure:dead_pure(candidate,budget);break;case Pass::LocalSlots:local_slots(candidate,budget);break;}
    verify_transition(result.module_,candidate,pass,options.verification);auto wire=serialize(candidate);
    if(wire!=result.output_){
     result.module_=std::move(candidate);result.output_=std::move(wire);changed=true;
     event.outcome=Outcome::Committed;event.output_revision=++revision;
    }
   }catch(const Exhausted& e){
    event.outcome=Outcome::BudgetExhausted;event.where=e.where;event.diagnostic=e.reason;
    result.status_=Status::BudgetExhausted;result.diagnostic_=std::string(descriptor(pass).name)+": "+e.reason;
   }
   event.work=budget.used-start_work;event.attempted_rewrites=budget.edits-start_edits;result.events_.push_back(std::move(event));
   result.work_=budget.used;result.rewrites_=budget.edits;
   if(result.status_==Status::BudgetExhausted)return result;
  }
  if(!changed)return result;
 }
 result.status_=Status::BudgetExhausted;result.diagnostic_="pass round budget exhausted before a fixed point";return result;
}
std::string report(const Result& result){
 using meta::SExpr;using meta::list;using meta::symbol;using meta::string;
 auto num=[](std::size_t n){return symbol(std::to_string(n));};
 auto names=[&](const std::vector<Analysis>& analyses){std::vector<SExpr> out;for(auto a:analyses)out.push_back(symbol(std::string(analysis_name(a))));return list(std::move(out));};
 std::vector<SExpr> catalog,events;
 for(auto pass:result.options().pipeline){const auto& d=descriptor(pass);std::vector<SExpr> requirements;
  for(auto r:d.requirements)requirements.push_back(symbol(std::string(r)));
  catalog.push_back(list({symbol("pass"),string(std::string(d.name)),num(d.version),list(std::move(requirements)),names(d.preserves),names(d.invalidates),string(std::string(d.effects)),string(std::string(d.target)),string(std::string(d.budget)),string(std::string(d.verifier))}));
 }
 for(const auto& e:result.events())events.push_back(list({symbol("event"),string(std::string(descriptor(e.pass).name)),num(e.round),num(e.input_revision),num(e.output_revision),
  symbol(e.outcome==Outcome::Committed?"committed":e.outcome==Outcome::Unchanged?"unchanged":"budget-exhausted"),num(e.work),num(e.attempted_rewrites),
  list({symbol("at"),string(e.where.file),num(e.where.offset),num(e.where.line),num(e.where.column)}),string(e.diagnostic)}));
 const auto& o=result.options();
 return meta::write_sexpr(list({symbol("on1x.mir-passes"),num(pipeline_version),
  list({symbol("target"),string(o.target)}),list({symbol("source"),string(result.source())}),list({symbol("output"),string(result.output())}),
  list({symbol("budgets"),num(o.work),num(o.rewrites),num(o.rounds),num(o.verification.nodes),num(o.verification.work)}),
  list({symbol("status"),symbol(result.status()==Status::Complete?"complete":"budget-exhausted"),string(result.diagnostic())}),
  list({symbol("consumed"),num(result.work()),num(result.attempted_rewrites())}),list(std::move(catalog)),list(std::move(events))}));
}
}

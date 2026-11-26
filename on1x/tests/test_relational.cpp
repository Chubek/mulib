#include <on1x/backend/relational.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>

namespace {
namespace m=on1x::mir;
namespace r=m::relational;
namespace dl=novostone::datalog;
using on1x::Type;
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
template<class Exception=std::exception,class F>void rejected(F operation,const std::string& why){
 try{operation();}catch(const Exception& e){require(std::string(e.what()).find(why)!=std::string::npos,"wrong rejection: "+std::string(e.what()));return;}
 throw std::runtime_error("expected rejection: "+why);
}
m::Module lower(const std::string& text){
 auto unit=on1x::parse(on1x::scan(text,"analysis-test.on1x"));on1x::check(unit);
 auto module=m::lower(unit);
 require(module.version==m::legacy_format_version,"MIR v1 fixture gained directives or an extended type: "+text);
 return module;
}
dl::Tuple pair(m::Id a,m::Id b){return {std::to_string(a),std::to_string(b)};}

// Independent worklist/sets over MIR, without reading projected facts or rules.
void oracle(const r::Snapshot& snapshot){
 const auto& module=snapshot.module();dl::Relation reach,calls,effects,definitions,uses,live_in,live_out;
 std::map<m::Id,std::set<m::Id>> direct;
 std::map<m::Id,std::set<std::string>> local_effects;
 for(const auto& f:module.functions){
  std::map<m::Id,const m::Block*> blocks;
  std::map<m::Id,std::set<m::Id>> defs,used,edge_used,in,out;
  for(const auto& b:f.blocks){
   blocks.emplace(b.id,&b);
   for(const auto& p:b.parameters)defs[b.id].insert(p.id);
   for(const auto& i:b.instructions){
    if(i.type!=Type::Void)defs[b.id].insert(i.id);
    used[b.id].insert(i.operands.begin(),i.operands.end());
    if(i.op==m::Op::Call)direct[f.id].insert(i.target);
    switch(m::effect(i.op)){
    case m::Effect::Pure:break;
    case m::Effect::Trap:local_effects[f.id].insert("trap");break;
    case m::Effect::Memory:local_effects[f.id].insert("memory");break;
    case m::Effect::Call:local_effects[f.id].insert("call");break;
    case m::Effect::IO:local_effects[f.id].insert("io");break;
    }
   }
   if(b.exit.value)used[b.id].insert(b.exit.value);
   for(const auto& e:b.exit.edges)edge_used[b.id].insert(e.arguments.begin(),e.arguments.end());
  }
  std::vector<m::Id> todo{f.blocks.front().id};std::set<m::Id> seen;
  while(!todo.empty()){
   auto b=todo.back();todo.pop_back();if(!seen.insert(b).second)continue;
   reach.insert(pair(f.id,b));for(const auto& e:blocks.at(b)->exit.edges)todo.push_back(e.block);
  }
  bool changed=true;
  while(changed){
   changed=false;
   for(auto it=f.blocks.rbegin();it!=f.blocks.rend();++it){const auto& b=*it;
    auto next_out=edge_used[b.id];
    for(const auto& e:b.exit.edges)next_out.insert(in[e.block].begin(),in[e.block].end());
    auto next_in=used[b.id];next_in.insert(next_out.begin(),next_out.end());
    for(auto v:defs[b.id])next_in.erase(v);
    if(in[b.id]!=next_in||out[b.id]!=next_out)changed=true;
    in[b.id]=std::move(next_in);out[b.id]=std::move(next_out);
   }
  }
  for(const auto& b:f.blocks){
   for(auto v:defs[b.id])definitions.insert(pair(b.id,v));
   for(auto v:used[b.id])uses.insert(pair(b.id,v));
   for(auto v:in[b.id])live_in.insert(pair(b.id,v));
   for(auto v:out[b.id])live_out.insert(pair(b.id,v));
  }
 }
 for(const auto& f:module.functions){
  std::set<m::Id> seen;std::vector<m::Id> todo(direct[f.id].begin(),direct[f.id].end());
  auto all_effects=local_effects[f.id];
  while(!todo.empty()){
   auto callee=todo.back();todo.pop_back();if(!seen.insert(callee).second)continue;
   calls.insert(pair(f.id,callee));all_effects.insert(local_effects[callee].begin(),local_effects[callee].end());
   todo.insert(todo.end(),direct[callee].begin(),direct[callee].end());
  }
  for(const auto& e:all_effects)effects.insert({std::to_string(f.id),e});
 }
 for(const auto& [name,expected]:std::map<std::string,dl::Relation>{{"Reachable",reach},{"Calls",calls},{"MayEffect",effects},
   {"Definition",definitions},{"UseAt",uses},{"LiveIn",live_in},{"LiveOut",live_out}}){
  auto q=snapshot.query(name);snapshot.require_current(q);
  require(q.result().rows==expected,"independent oracle mismatch: "+name);
  require(q.result().revision==snapshot.revision(),"query revision lost");
  auto naive=r::program().evaluate(snapshot.facts(),name,{},dl::Evaluation::Naive);
  require(naive.rows==expected,"naive/seminaive mismatch: "+name);
 }
}
m::Module phis(){
 on1x::Location at{"phi-analysis.on1x"};m::Module module;m::Function f;f.id=2;f.name="main";f.where=at;
 m::Block entry;entry.id=3;entry.effect=4;entry.where=at;
 entry.instructions={{5,Type::I64,m::Op::Constant,{},10,0,0,0,at},{6,Type::I64,m::Op::Constant,{},20,0,0,0,at},{7,Type::Bool,m::Op::Constant,{},1,0,0,0,at}};
 entry.exit={8,m::Exit::Jump,0,4,{{9,{5,6,7}}},at};
 m::Block loop;loop.id=9;loop.effect=10;loop.where=at;loop.parameters={{11,Type::I64,at},{12,Type::I64,at},{13,Type::Bool,at}};
 loop.instructions={{14,Type::Bool,m::Op::Constant,{},0,0,0,0,at}};
 loop.exit={15,m::Exit::Branch,13,10,{{9,{12,11,14}},{16,{11,12}}},at};
 m::Block done;done.id=16;done.effect=17;done.where=at;done.parameters={{18,Type::I64,at},{19,Type::I64,at}};
 // A dominating entry value remains live across the loop independently of phis.
 done.instructions={{20,Type::Void,m::Op::Print,{5},0,0,17,21,at}};
 done.exit={22,m::Exit::Return,0,21,{},at};f.blocks={entry,done,loop};module.functions={f};return module;
}
}
int main(){
 try{
  auto module=lower("fn leaf(x:i64)->i64{return x;}fn other(x:i64)->i64{return x;}"
    "fn relay(x:i64)->i64{return leaf(x);}fn main(){println(relay(1));}");
  r::Store store(module);auto first=store.snapshot();oracle(first);
  auto before=m::serialize(module),dump=r::dump_facts(first);
  auto parsed=dl::parse(dump);r::check_projection(module,parsed);
  require(dl::dump(parsed)==dump,"Datalog source round-trip not canonical");
  require(r::report(first)==r::report(first),"cache changes report bytes");
  require(before==m::serialize(module),"analysis modified MIR");
  auto missing=parsed;missing.facts["Call"].clear();rejected([&]{r::check_projection(module,missing);},"missing, extra");
  auto extra=parsed;extra.facts["Entry"].insert(pair(9999,10000));rejected([&]{r::check_projection(module,extra);},"missing, extra");
  auto mistyped=parsed;mistyped.facts["Constant"].insert({std::string("bad"),std::string("not integer")});
  rejected([&]{r::check_projection(module,mistyped);},"type");
  auto unknown=parsed;unknown.facts["Imaginary"]={};rejected([&]{r::check_projection(module,unknown);},"unknown/derived");
  unknown=parsed;unknown.facts["Calls"]={};rejected([&]{r::check_projection(module,unknown);},"unknown/derived");
  auto rules=parsed;rules.program.declare("Injected",{});rejected([&]{r::check_projection(module,rules);},"catalog mismatch");
  auto wrong_source=module;wrong_source.functions[0].where.column++;
  rejected([&]{r::check_projection(wrong_source,parsed);},"missing, extra");
  r::ImportLimits small;small.facts=0;
  rejected<r::BudgetExceeded>([&]{store.publish(first,module,small);},"fact budget");
  small={};small.bytes=0;rejected<r::BudgetExceeded>([&]{r::project(module,small);},"byte budget");
  small={};small.mir.nodes=0;rejected<on1x::Error>([&]{store.publish(first,module,small);},"node budget");
  rejected<r::BudgetExceeded>([&]{first.query("Calls",{1000000,0});},"budget");
  rejected<r::BudgetExceeded>([&]{first.query("Calls",{0,10000000});},"budget");
  rejected<r::BudgetExceeded>([&]{r::report(first,{1000000,0});},"budget");
  auto reach_work=first.query("Reachable").result().work;
  rejected<r::BudgetExceeded>([&]{r::report(first,{1000000,reach_work});},"budget");
  rejected([&]{first.query("Unknown");},"unknown relation");
  require(store.events().size()==1,"failed import published a revision");

  auto query=first.query("Calls");require(query.result().cached,"query did not cache");
  require(query.result().dependencies==std::set<std::string>{"Call"},"call query dependencies imprecise");
  auto relocated=module;relocated.functions[0].where.line++;
  auto second=store.publish(first,relocated);
  require(second.query("Calls").result().cached,"unrelated source edit invalidated Calls");
  rejected<novostone::ccdb::Conflict>([&]{second.require_current(query);},"stale or foreign");
  rejected<novostone::ccdb::Conflict>([&]{store.publish(first,module);},"stale or foreign");
  r::Store foreign(module);
  require(foreign.snapshot().revision()==first.revision(),"fixture revision mismatch");
  rejected<novostone::ccdb::Conflict>([&]{foreign.snapshot().require_current(query);},"stale or foreign");
  rejected<novostone::ccdb::Conflict>([&]{store.publish(foreign.snapshot(),module);},"stale or foreign");

  auto replacement=module.functions[2];
  auto call=std::find_if(replacement.blocks[0].instructions.begin(),replacement.blocks[0].instructions.end(),[](const auto& i){return i.op==m::Op::Call;});
  require(call!=replacement.blocks[0].instructions.end(),"missing call fixture");call->target=module.functions[1].id;
  auto third=store.replace_function(second,replacement);
  require(!third.query("Calls").result().cached,"changed call graph reused stale cache");oracle(third);
  require(first.query("Calls").result().rows==query.result().rows,"old snapshot changed");
  auto bad=replacement;bad.blocks[0].exit.effect=0;
  rejected<on1x::Error>([&]{store.replace_function(third,bad);},"effects");
  require(store.snapshot().revision()==third.revision()&&store.events().size()==3,"invalid edit escaped transaction");
  call->op=m::Op::Neg;call->target=0;
  auto fourth=store.replace_function(third,replacement);oracle(fourth);
  require(fourth.query("Calls").result().rows.size()==1,"deleted call consequences survived");
  require(store.events().back().changed.contains("Call"),"missing call invalidation audit");
  auto surviving=[&]{r::Store temporary(module);return temporary.snapshot();}();oracle(surviving);

  auto phi=phis();r::Store phi_store(phi);oracle(phi_store.snapshot());
  const auto live=phi_store.snapshot().query("LiveIn").result().rows;
  require(live==dl::Relation{pair(9,5),pair(16,5)},"phi operands treated as join-block uses");
  // Both branch edges target one block, but carry different argument tuples.
  phi.functions[0].blocks[2].exit.edges={ {16,{11,12}}, {16,{12,11}} };
  r::Store parallel_edges(phi);oracle(parallel_edges.snapshot());
  auto projected=r::project(phi);r::check_projection(phi,dl::parse(dl::dump(projected)));
  require(projected.facts.at("Argument").size()==7,"parallel edges merged");

  auto wide=lower("fn main(){println(-9223372036854775808);println(9223372036854775807);}");
  wide.id=std::numeric_limits<m::Id>::max();auto& instructions=wide.functions[0].blocks[0].instructions;
  auto old=instructions[0].id;instructions[0].id=wide.id-1;
  for(auto& i:instructions)for(auto& v:i.operands)if(v==old)v=wide.id-1;
  wide.functions[0].where={"quotes\"/slash\\/newline\n.on1x",std::numeric_limits<std::size_t>::max(),1,1};
  r::Store wide_store(wide);auto wide_facts=dl::parse(r::dump_facts(wide_store.snapshot()));r::check_projection(wide,wide_facts);
  require(wide_facts.facts.at("Module").begin()->at(0)==dl::Value(std::string("18446744073709551615")),"uint64 identity narrowed");
  require(wide_facts.facts.at("Constant").contains({std::to_string(wide.id-1),std::numeric_limits<std::int64_t>::min()}),"i64 minimum narrowed");
  bool max_found=false;for(const auto& row:wide_facts.facts.at("Constant"))max_found|=std::get<std::int64_t>(row[1])==std::numeric_limits<std::int64_t>::max();
  require(max_found,"i64 maximum narrowed");oracle(wide_store.snapshot());

  std::mt19937 rng(728);
  // Built-in optimization publishes only against its exact source snapshot;
  // CCDB retracts call/effect consequences and retains unaffected analyses.
  auto optimizable=lower("fn leaf(x:i64)->i64{return x;}fn main(){if true{println(2+3);}else{println(leaf(1));}}");
  r::Store optimized_store(optimizable);auto opt_source=optimized_store.snapshot();
  auto old_calls=opt_source.query("Calls");require(old_calls.result().rows.size()==1,"optimization fixture lacks call");
  m::passes::Options no_work;no_work.work=0;
  auto unchanged=optimized_store.optimize(opt_source,no_work);
  require(!unchanged.result.changed()&&unchanged.snapshot.revision()==opt_source.revision(),"empty optimization advanced revision");
  unchanged.snapshot.require_current(old_calls);
  r::ImportLimits no_facts;no_facts.facts=0;
  rejected<r::BudgetExceeded>([&]{optimized_store.optimize(opt_source,{},no_facts);},"fact budget");
  require(optimized_store.events().size()==1,"failed optimized projection published");
  auto optimized=optimized_store.optimize(opt_source);
  require(optimized.result.changed()&&optimized.result.status()==m::passes::Status::Complete,"optimization did not complete");
  require(optimized.snapshot.source()==optimized.result.output(),"optimized facts have wrong source");
  auto new_calls=optimized.snapshot.query("Calls");
  require(!new_calls.result().cached&&new_calls.result().rows.empty(),"optimization retained stale call consequences");
  oracle(optimized.snapshot);
  rejected<novostone::ccdb::Conflict>([&]{optimized.snapshot.require_current(old_calls);},"stale or foreign");
  rejected<novostone::ccdb::Conflict>([&]{optimized_store.optimize(opt_source);},"stale or foreign");
  rejected<novostone::ccdb::Conflict>([&]{optimized_store.optimize(foreign.snapshot());},"stale or foreign");
  auto stable=optimized_store.optimize(optimized.snapshot);
  require(!stable.result.changed()&&stable.snapshot.revision()==optimized.snapshot.revision(),"fixed point advanced revision");
  stable.snapshot.require_current(new_calls);
  require(optimized_store.events().back().reason=="on1x.mir-passes/v1 complete","pass provenance missing from audit");
  for(unsigned sample=0;sample<40;++sample){
   std::string source;
   for(unsigned f=0;f<4;++f){
    auto target=rng()%4;source+="fn f"+std::to_string(f)+"(n:i64)->i64{var i=0;while i<n{i=i+1;}";
    if(rng()%2)source+="println(i);";
     source+="if n<=0{return "+std::to_string(rng()%100)+";}return f"+std::to_string(target)+"(n-1);}";
   }
    source+="fn main(){println(f0(3));println(f0(3)>0&&(f1(0)==1||f2(1)==2));}";
   r::Store generated(lower(source));oracle(generated.snapshot());
  }
  std::cout<<"MIR facts, independent graph/liveness oracles, revisions, rollback and budgets PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

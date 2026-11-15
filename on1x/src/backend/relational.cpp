#include <on1x/backend/relational.hpp>
#include <on1x/meta.hpp>
#include "mir_analysis.hpp"
#include <limits>

namespace on1x::mir::relational {
namespace {
namespace dl=novostone::datalog;
[[noreturn]] void fail(const std::string& message){throw Error({"<mir-analysis>"},"ON1X-MIR-ANALYSIS: "+message);}
[[noreturn]] void exhausted(const std::string& message){throw BudgetExceeded({"<mir-analysis>"},"ON1X-MIR-ANALYSIS: "+message);}
std::string id(Id value){return std::to_string(value);}
std::int64_t ordinal(std::size_t value){
 if(value>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))fail("ordinal out of range");
 return static_cast<std::int64_t>(value);
}
std::string effect_name(Effect e){
 switch(e){case Effect::Pure:return "pure";case Effect::Trap:return "trap";case Effect::Memory:return "memory";case Effect::Call:return "call";case Effect::IO:return "io";}
 fail("unknown effect");
}
struct Budget {
 std::size_t facts,bytes;
 void charge(const std::string& name,const dl::Tuple& row){
  if(!facts)exhausted("fact budget exhausted");
  --facts;
  auto take=[&](std::size_t n){if(n>bytes)exhausted("fact byte budget exhausted");bytes-=n;};
  take(name.size());
  for(const auto& v:row)if(auto s=std::get_if<std::string>(&v))take(s->size());else take(sizeof(std::int64_t));
 }
};
dl::Facts normalized(const dl::Facts& facts){
 auto result=facts;
 for(const auto& [name,schema]:program().schemas())if(!schema.derived)result.try_emplace(name);
 return result;
}
}
const novostone::datalog::Program& program(){
 static const auto parsed=dl::parse(analysis_source,"on1x/backend/Analysis.dl");
 return parsed.program;
}
novostone::datalog::Module project(const Module& module,ImportLimits limits){
 verify(module,limits.mir);
 dl::Module out{program(),{}};Budget budget{limits.facts,limits.bytes};
 auto add=[&](const std::string& name,dl::Tuple row){
  budget.charge(name,row);out.program.validate_fact(name,row);out.facts[name].insert(std::move(row));
 };
 auto location=[&](Id entity,const Location& at){
  add("Location",{id(entity),at.file,std::to_string(at.offset),std::to_string(at.line),std::to_string(at.column)});
 };
 add("Module",{id(module.id),std::int64_t(module.version),std::int64_t(mapping_version)});
 for(std::size_t fn=0;fn<module.functions.size();++fn){
  const auto& f=module.functions[fn];
  add("Function",{id(f.id),ordinal(fn),f.name,type_name(f.result)});location(f.id,f.where);
  add("Entry",{id(f.id),id(f.blocks.front().id)});
  for(std::size_t s=0;s<f.slots.size();++s){const auto& slot=f.slots[s];
   add("Slot",{id(slot.id),id(f.id),ordinal(s),type_name(slot.type)});location(slot.id,slot.where);
  }
  std::map<Id,Id> block_effects;for(const auto& b:f.blocks)block_effects.emplace(b.id,b.effect);
  for(std::size_t bn=0;bn<f.blocks.size();++bn){const auto& b=f.blocks[bn];
   add("Block",{id(b.id),id(f.id),ordinal(bn)});location(b.id,b.where);
   add("BlockEffect",{id(b.id),id(b.effect)});
   for(std::size_t p=0;p<b.parameters.size();++p){const auto& v=b.parameters[p];
    add("Parameter",{id(v.id),id(b.id),ordinal(p),type_name(v.type)});location(v.id,v.where);
   }
   for(std::size_t n=0;n<b.instructions.size();++n){const auto& i=b.instructions[n];
    add("Instruction",{id(i.id),id(b.id),ordinal(n),std::string(op_name(i.op)),type_name(i.type)});location(i.id,i.where);
    if(i.op==Op::Constant)add("Constant",{id(i.id),i.immediate});
    for(std::size_t a=0;a<i.operands.size();++a)add("Operand",{id(i.id),ordinal(a),id(i.operands[a])});
    if(i.op==Op::Call)add("Call",{id(i.id),id(f.id),id(i.target)});
    if(i.op==Op::Load||i.op==Op::Store)add("SlotAccess",{id(i.id),id(i.target)});
    if(effect(i.op)!=Effect::Pure)add("EffectStep",{id(i.id),id(i.effect_in),id(i.effect_out),effect_name(effect(i.op))});
   }
   const auto& t=b.exit;
   add("Terminator",{id(t.id),id(b.id),std::string(t.kind==Exit::Return?"return":t.kind==Exit::Jump?"jump":"branch")});location(t.id,t.where);
   if(t.value)add("ExitValue",{id(t.id),id(t.value)});
   add("ExitEffect",{id(t.id),id(t.effect)});
   for(std::size_t e=0;e<t.edges.size();++e){const auto& edge=t.edges[e];
    add("Edge",{id(t.id),ordinal(e),id(b.id),id(edge.block)});
    add("EffectEdge",{id(t.id),ordinal(e),id(t.effect),id(block_effects.at(edge.block))});
    for(std::size_t a=0;a<edge.arguments.size();++a)add("Argument",{id(t.id),ordinal(e),ordinal(a),id(edge.arguments[a])});
   }
  }
 }
 return out;
}
void check_projection(const Module& module,const dl::Module& facts,ImportLimits limits){
 // Check external input budgets before copying/comparing any supplied fact set.
 Budget budget{limits.facts,limits.bytes};
 if(facts.program.signature()!=program().signature())fail("schema/rule catalog mismatch");
 for(const auto& [name,rows]:facts.facts){
  auto schema=program().schemas().find(name);
  if(schema==program().schemas().end()||schema->second.derived)fail("unknown/derived fact relation: "+name);
  for(const auto& row:rows){budget.charge(name,row);program().validate_fact(name,row);}
 }
 auto expected=project(module,limits);
 if(normalized(expected.facts)!=normalized(facts.facts))fail("facts do not match verified MIR (missing, extra or altered facts)");
}
Query Snapshot::query(const std::string& relation,dl::Limits limits) const {
 if(!program().schemas().contains(relation))fail("unknown relation: "+relation);
 try{return Query(*this,relation,data_.query(relation,limits));}
 catch(const dl::BudgetExceeded& e){exhausted(e.what());}
}
void Snapshot::require_current(const Query& query) const {
 if(!data_.same_revision(query.source_.data_))throw novostone::ccdb::Conflict("ON1X-MIR-ANALYSIS: stale or foreign query revision");
}
Store::Store(const Module& module,ImportLimits limits)
 : database_(program()),head_(database_.snapshot(),nullptr,nullptr){publish(head_,module,limits);}
Snapshot Store::publish(const Snapshot& expected,const Module& module,ImportLimits limits,std::string reason){
 auto tx=database_.begin(expected.data_); // Identity check precedes any analysis.
 auto projected=project(module,limits);auto facts=normalized(projected.facts);
 auto owned=std::make_shared<const Module>(module);
 auto source=std::make_shared<const std::string>(serialize(module));
 Snapshot candidate(expected.data_,std::move(owned),std::move(source));
 for(const auto& [name,rows]:expected.facts())for(const auto& row:rows)
  if(!facts.at(name).contains(row))tx.erase(name,row);
 for(const auto& [name,rows]:facts)for(const auto& row:rows)
  if(!expected.facts().at(name).contains(row))tx.insert(name,row);
 candidate.data_=tx.commit(std::move(reason),[&](const auto& staged){
  if(staged.facts()!=facts)fail("transaction projection mismatch");
 });
 head_=candidate;return candidate;
}
Snapshot Store::replace_function(const Snapshot& expected,const Function& function,ImportLimits limits){
 // Reject stale/foreign input before copying a previous module.
 auto guard=database_.begin(expected.data_);guard.abort();
 auto module=expected.module();bool found=false;
 for(auto& f:module.functions)if(f.id==function.id){f=function;found=true;break;}
 if(!found)fail("replacement function identity not found");
 return publish(expected,module,limits);
}
Optimization Store::optimize(const Snapshot& expected,passes::Options options,ImportLimits limits){
 auto guard=database_.begin(expected.data_);guard.abort();
 auto result=passes::run(expected.module(),std::move(options));
 if(result.source()!=expected.source())fail("pass source identity mismatch");
 auto next=result.changed()?publish(expected,result.module(),limits,
  result.status()==passes::Status::Complete?"on1x.mir-passes/v1 complete":"on1x.mir-passes/v1 verified budget prefix"):expected;
 return {std::move(next),std::move(result)};
}
std::string dump_facts(const Snapshot& snapshot){return dl::dump({program(),snapshot.facts()});}
std::string report(const Snapshot& snapshot,dl::Limits limits){
 using meta::list;using meta::symbol;using meta::string;using meta::SExpr;
 std::vector<SExpr> queries;
 std::size_t nodes=meta::max_wire_nodes-13;
 auto charge=[&](std::size_t n){if(n>nodes)exhausted("report node budget exhausted");nodes-=n;};
 for(const auto* name:{"Reachable","Calls","MayEffect","LiveIn","LiveOut"}){
  auto query=snapshot.query(name,limits);snapshot.require_current(query);const auto& r=query.result();
  if(r.work>limits.work)exhausted("cumulative query budget exhausted");
  limits.work-=r.work;
  charge(5+r.dependencies.size());
  std::vector<SExpr> dependencies,rows;
  for(const auto& d:r.dependencies)dependencies.push_back(string(d));
  for(const auto& row:r.rows){charge(1+row.size());std::vector<SExpr> values;
   for(const auto& v:row)if(auto s=std::get_if<std::string>(&v))values.push_back(string(*s));else values.push_back(symbol(std::to_string(std::get<std::int64_t>(v))));
   rows.push_back(list(std::move(values)));
  }
  queries.push_back(list({symbol("query"),string(name),list(std::move(dependencies)),list(std::move(rows))}));
 }
 auto tree=list({symbol("on1x.mir-analysis"),symbol(std::to_string(mapping_version)),
  list({symbol("revision"),symbol(std::to_string(snapshot.revision()))}),
  list({symbol("source"),string(snapshot.source())}),list({symbol("rules"),string(program().signature())}),list(std::move(queries))});
 try{return meta::write_sexpr(tree);}catch(const Error& e){exhausted(e.what());}
}
}

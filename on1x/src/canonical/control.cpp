#include <on1x/canonical/control.hpp>
#include <on1x/clone.hpp>
#include <algorithm>
#include <set>

namespace on1x::canonical {
namespace {
using S=Stmt::Kind;using E=Expr::Kind;
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-CONTROL: "+message,"ON1X-CONTROL");}
struct Budget {
 std::size_t nodes=262144;
 // The parser bounds statement nesting and expression nesting separately. A
 // lambda body can sit below both; do not reduce historically accepted budgets.
 void tick(const Location& at,std::size_t depth){if(depth>2*max_syntax_depth+8||!nodes--)fail(at,"canonical control depth/node budget exhausted");}
};
ExprPtr atom(E kind,const std::string& text,const Location& at){auto e=std::make_unique<Expr>();e->kind=kind;e->text=text;e->where=at;return e;}
ExprPtr id(const std::string& text,const Location& at){return atom(E::Variable,text,at);}
ExprPtr binary(const std::string& op,ExprPtr a,ExprPtr b,const Location& at){auto e=atom(E::Binary,op,at);e->children.push_back(std::move(a));e->children.push_back(std::move(b));return e;}
Stmt binding(const std::string& name,Type type,ExprPtr value,bool mut,const Location& at){Stmt s;s.kind=S::Let;s.name=name;s.where=at;s.declared=type;s.value=std::move(value);s.mutable_binding=mut;return s;}
class Lower {
 Unit& unit;Budget budget;std::size_t serial=0;
 struct Loop {std::string label;std::size_t context,target;};
 std::vector<Loop> loops;
 std::vector<std::set<std::string>> names;
 std::set<std::string> globals;
 bool visible(const std::string& name){if(globals.contains(name))return true;for(const auto& scope:names)if(scope.contains(name))return true;return false;}
 void pattern(const Pattern& p){pattern_bindings(p,[&](const Pattern& name){names.back().insert(name.name);});}
 void parameters(const std::vector<Param>& ps){for(const auto& p:ps){if(p.pattern)pattern(*p.pattern);else names.back().insert(p.name);}}
 Stmt range(Stmt source){
  auto at=source.where;
  if(!source.value||source.value->kind!=E::Tuple||source.value->children.size()!=2||!source.value->children[0]||!source.value->children[1]||source.name.empty()||source.pattern||!source.otherwise.empty())fail(at,"malformed source range");
  // Unlexable private names are introduced only after macros/PPX; never part of
  // the source wire ABI. Bounds are evaluated left-to-right, once, before binding.
  auto prefix="@range_"+std::to_string(++serial);auto low=prefix+"_low",high=prefix+"_high",cursor=prefix+"_cursor",done=prefix+"_done";
  Stmt scope;scope.kind=S::Block;scope.where=at;
  scope.body.push_back(binding(low,Type::I64,std::move(source.value->children[0]),false,at));
  scope.body.push_back(binding(high,Type::I64,std::move(source.value->children[1]),false,at));
  scope.body.push_back(binding(cursor,Type::I64,id(low,at),true,at));
  scope.body.push_back(binding(done,Type::Bool,atom(E::Boolean,"false",at),true,at));
  Stmt loop;loop.kind=S::While;loop.where=at;loop.label=source.label;loop.label_context=source.label_context;
  loop.value=binary("&&",binary("==",id(done,at),atom(E::Boolean,"false",at),at),binary(source.inclusive?"<=":"<",id(cursor,at),id(high,at),at),at);
  loop.body.push_back(binding(source.name,Type::I64,id(cursor,at),false,at));
  // Keep the implicit binder outside the user's block, like for-C's initializer.
  Stmt body;body.kind=S::Block;body.where=at;body.body=std::move(source.body);loop.body.push_back(std::move(body));
  Stmt step;step.kind=S::Assign;step.where=at;step.name=cursor;step.value=binary("+",id(cursor,at),atom(E::Integer,"1",at),at);
  if(source.inclusive){
   // Never form MAX+1. Completion is tested before increment, including continue.
   Stmt end;end.kind=S::If;end.where=at;end.value=binary("==",id(cursor,at),id(high,at),at);
   Stmt stop;stop.kind=S::Assign;stop.where=at;stop.name=done;stop.value=atom(E::Boolean,"true",at);
   end.body.push_back(std::move(stop));end.otherwise.push_back(std::move(step));loop.otherwise.push_back(std::move(end));
  }else loop.otherwise.push_back(std::move(step));
  scope.body.push_back(std::move(loop));return scope;
 }
 void expression(Expr& e,std::size_t depth){
  budget.tick(e.where,depth);
  if(e.kind==E::PatternMatch){expression(*e.children.at(0),depth+1);names.emplace_back();if(!e.text.empty())names.back().insert(e.text);for(std::size_t i=1;i<e.children.size();++i)expression(*e.children[i],depth+1);names.pop_back();return;}
  if(e.kind==E::Case){names.emplace_back();for(const auto& p:e.patterns)pattern(p);expression(*e.children.at(0),depth+1);names.emplace_back();body(e.body,depth+1,false);expression(*e.children.at(1),depth+1);names.pop_back();names.pop_back();return;}
  if(e.callee)expression(*e.callee,depth+1);
  for(auto& c:e.children){if(!c)fail(e.where,"missing expression child");expression(*c,depth+1);}
  if(e.kind==E::Lambda){auto outer=std::move(loops);loops.clear();names.emplace_back();parameters(e.parameters);body(e.body,depth+1);names.pop_back();loops=std::move(outer);}
  else if(e.kind==E::Region)body(e.body,depth+1);
  else if(!e.body.empty())fail(e.where,"unlowered expression body");
 }
 void body(std::vector<Stmt>& statements,std::size_t depth,bool scope=true){
  if(scope)names.emplace_back();
  for(auto& s:statements){
   budget.tick(s.where,depth);if(s.prototype)continue;
   if(s.kind==S::Range)s=range(std::move(s));
   s.control_target=0;
   if(s.kind==S::Break||s.kind==S::Continue){
   if(s.value||s.pattern||!s.name.empty()||!s.body.empty()||!s.otherwise.empty())fail(s.where,"malformed loop exit");
    auto name=s.kind==S::Break?"break":"continue";
    // Preserve formerly valid identifier expression statements. Labels provide
    // unambiguous control syntax when an ordinary binding has this spelling.
    if(s.label.empty()&&!s.label_context&&visible(name)){s.kind=S::Expr;s.value=id(name,s.where);}
    else{
     auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const Loop& l){return s.label_context==l.context&&(s.label.empty()||s.label==l.label);});
     if(target==loops.rend())fail(s.where,"loop exit has no lexical target"+(s.label.empty()?std::string{}:": "+s.label));
     s.control_target=target->target;continue;
    }
   }
   if(s.value&&s.kind!=S::Template)expression(*s.value,depth+1);
   if(s.kind==S::Let){if(s.pattern)pattern(*s.pattern);else names.back().insert(s.name);}
   if(s.kind==S::While){
    if(!s.value)fail(s.where,"loop condition missing");
    if(!s.label.empty()&&std::any_of(loops.begin(),loops.end(),[&](const auto& l){return l.label==s.label&&l.context==s.label_context;}))fail(s.where,"duplicate active loop label: "+s.label);
    s.control_target=++serial;loops.push_back({s.label,s.label_context,s.control_target});body(s.body,depth+1);
    // No jumps from the latch back into the same latch. Source steps are simple
    // clauses; outer exits in a transformed latch remain lexically meaningful.
    loops.pop_back();body(s.otherwise,depth+1);
   }else{body(s.body,depth+1);body(s.otherwise,depth+1);}
  }
  if(scope)names.pop_back();
 }
public:
 explicit Lower(Unit& u):unit(u){for(const auto& f:u.functions)globals.insert(f.name);}
 void run(){for(auto& f:unit.functions){if(f.prototype)continue;loops.clear();names.emplace_back();parameters(f.params);body(f.body,0);names.pop_back();}}
};
class Verify {
 Budget budget;std::set<std::size_t> ids;std::vector<std::size_t> targets,regions;
 void expression(const Expr& e,std::size_t depth){
  budget.tick(e.where,depth);
  if(e.callee)expression(*e.callee,depth+1);
  for(const auto& c:e.children){if(!c)fail(e.where,"missing expression child");expression(*c,depth+1);}
  if(e.kind==E::Lambda){auto saved=std::move(targets),saved_regions=std::move(regions);targets.clear();regions.clear();body(e.body,depth+1);targets=std::move(saved);regions=std::move(saved_regions);}
  else if(e.kind==E::Case||e.kind==E::Region)body(e.body,depth+1);
  else if(!e.body.empty())fail(e.where,"noncanonical expression body");
 }
 void body(const std::vector<Stmt>& statements,std::size_t depth){for(const auto& s:statements){
  budget.tick(s.where,depth);if(s.prototype)continue;
   if(s.kind==S::Range)fail(s.where,"surface range reached canonical verification");
   if(s.kind==S::While||s.kind==S::Break||s.kind==S::Continue){
    if(s.where.file.empty()||s.where.file.size()>4096||!s.where.line||!s.where.column||s.where.offset>max_source_bytes||s.where.line>max_source_bytes+1||s.where.column>max_source_bytes+1)fail(s.where,"invalid canonical control source point");
   }
  if(s.kind==S::While){
   if(!s.value||!s.control_target||!ids.insert(s.control_target).second)fail(s.where,"missing/duplicate canonical loop identity");
   expression(*s.value,depth+1);targets.push_back(s.control_target);body(s.body,depth+1);targets.pop_back();body(s.otherwise,depth+1);
  }else if(s.kind==S::Region){
   if(!s.control_target||!ids.insert(s.control_target).second||s.value||!s.otherwise.empty())fail(s.where,"invalid canonical region");
   regions.push_back(s.control_target);body(s.body,depth+1);regions.pop_back();
  }else if(s.kind==S::Leave){
   if(!s.control_target||s.value||s.pattern||!s.name.empty()||!s.body.empty()||!s.otherwise.empty()||std::find(regions.begin(),regions.end(),s.control_target)==regions.end())fail(s.where,"invalid canonical region exit");
  }else if(s.kind==S::Break||s.kind==S::Continue){
    if(s.value||s.pattern||!s.name.empty()||!s.body.empty()||!s.otherwise.empty()||!s.control_target||std::find(targets.begin(),targets.end(),s.control_target)==targets.end())fail(s.where,"invalid canonical loop exit target/shape");
  }else{
   if(s.control_target)fail(s.where,"control identity on non-control statement");
   if(s.value&&s.kind!=S::Template)expression(*s.value,depth+1);
   body(s.body,depth+1);body(s.otherwise,depth+1);
  }
 }}
public:
 void run(const Unit& u){for(const auto& f:u.functions){if(f.prototype)continue;targets.clear();body(f.body,0);}}
};
}
void verify_control(const Unit& unit){Verify().run(unit);}
bool needs_control(const Unit& unit){
 Budget budget;std::set<std::size_t> ids;bool needs=false;
 auto expression=[&](auto&& self,const Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);if(e.callee)self(self,*e.callee,depth+1);for(const auto& c:e.children){if(!c)fail(e.where,"missing expression child");self(self,*c,depth+1);}
  auto body=[&](auto&& walk,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.prototype)continue;
   if(s.kind==S::Range||((s.kind==S::While||s.kind==S::Break||s.kind==S::Continue)&&!s.control_target))needs=true;
   if(s.kind==S::While&&s.control_target&&!ids.insert(s.control_target).second)needs=true;
   if(s.value&&s.kind!=S::Template)self(self,*s.value,d+1);
   walk(walk,s.body,d+1);walk(walk,s.otherwise,d+1);
  }};body(body,e.body,depth+1);
 };
 for(const auto& f:unit.functions){if(f.prototype)continue;auto body=[&](auto&& walk,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.prototype)continue;
  if(s.kind==S::Range||((s.kind==S::While||s.kind==S::Break||s.kind==S::Continue)&&!s.control_target))needs=true;
  if(s.kind==S::While&&s.control_target&&!ids.insert(s.control_target).second)needs=true;
  if(s.value&&s.kind!=S::Template)expression(expression,*s.value,d+1);
  walk(walk,s.body,d+1);walk(walk,s.otherwise,d+1);
 }};body(body,f.body,0);}
 return needs;
}
void lower_control(Unit& unit){if(!needs_control(unit)){verify_control(unit);return;}std::size_t budget=262144;auto working=clone_unit(unit,budget);Lower(working).run();verify_control(working);unit=std::move(working);}
}

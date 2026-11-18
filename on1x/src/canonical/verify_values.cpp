#include <on1x/canonical/values.hpp>
#include <algorithm>
#include <map>
#include <iterator>
#include <optional>
#include <set>

namespace on1x::canonical {
namespace {
using E=Expr::Kind;using S=Stmt::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-VALUES-VERIFY: "+text,"ON1X-VALUES-VERIFY");}
struct State {
  std::set<const Stmt*> initialized;
  std::set<const Stmt*> maybe_initialized;
 std::set<std::pair<std::size_t,std::string>> tags;
};
using Flow=std::optional<State>;
State meet(const State& a,const State& b){
  State out;std::set_intersection(a.initialized.begin(),a.initialized.end(),b.initialized.begin(),b.initialized.end(),std::inserter(out.initialized,out.initialized.end()));
  std::set_union(a.maybe_initialized.begin(),a.maybe_initialized.end(),b.maybe_initialized.begin(),b.maybe_initialized.end(),std::inserter(out.maybe_initialized,out.maybe_initialized.end()));
 std::set_intersection(a.tags.begin(),a.tags.end(),b.tags.begin(),b.tags.end(),std::inserter(out.tags,out.tags.end()));return out;
}
Flow merge(Flow a,const Flow& b){if(!a)return b;if(!b)return a;return meet(*a,*b);}
 class Verify {
  bool unsafe_context=false;
 struct Local {Type type;bool mut=false;const Stmt* storage=nullptr;std::size_t id=0;};
 struct Target {std::size_t id;std::vector<State> exits,continues;};
 std::vector<std::map<std::string,Local>> scopes;
  std::vector<Target> regions,loops;
  Type returns=Type::Void;
 std::size_t serial=0,budget=262144,state_work=4*1024*1024;
 void tick(const Location& at,std::size_t depth){if(depth>520||!budget--)throw Error(at,"canonical value verification budget exhausted","ON1X-VALUES-LIMIT");}
  void charge(const State& state,const Location& at){auto size=state.initialized.size()+state.maybe_initialized.size()+state.tags.size();if(size>state_work)throw Error(at,"canonical value flow-state budget exhausted","ON1X-VALUES-LIMIT");state_work-=size;}
 void source(const Location& at){if(at.file.empty()||at.file.size()>4096||!at.line||!at.column||at.offset>max_source_bytes||at.line>max_source_bytes+1||at.column>max_source_bytes+1)fail(at,"invalid canonical value source point");}
 const Local* find(const std::string& name)const{for(auto it=scopes.rbegin();it!=scopes.rend();++it)if(auto found=it->find(name);found!=it->end())return &found->second;return nullptr;}
 void bind(const std::string& name,Type type,bool mut,const Stmt* storage=nullptr){if(name.empty()||scopes.back().contains(name))fail(storage?storage->where:Location{"<canonical-values>"},"empty/duplicate canonical local");scopes.back()[name]={type,mut,storage,++serial};}
  void pattern(const Pattern& p,const Type& t){
   if(p.is_tuple()){
    if(!t.is_tuple()||p.elements.size()!=t.elements().size())fail(p.where,"canonical tuple binding pattern/type mismatch");
    for(std::size_t i=0;i<p.elements.size();++i)pattern(p.elements[i],t.elements()[i]);
   }else bind(p.name,t,false);
  }
 void params(const std::vector<Param>& ps){for(const auto& p:ps)if(p.pattern)pattern(*p.pattern,p.type);else bind(p.name,p.type,false);}
 std::pair<std::size_t,std::string> tag(const Expr& e){
  if(e.children.size()!=1||!e.children[0]||e.children[0]->kind!=E::Variable)fail(e.where,"sum decision operand must be a named value");
  const auto& root=*e.children[0];auto binding=find(root.text);if(!binding||binding->mut||binding->type!=root.type||root.type.kind()!=Type::Sum)fail(e.where,"sum decision requires an immutable closed-sum binding");
  auto found=std::find(root.type.labels().begin(),root.type.labels().end(),e.text);if(found==root.type.labels().end()||e.projection!=static_cast<std::size_t>(found-root.type.labels().begin()))fail(e.where,"sum decision tag/projection mismatch");
  return {binding->id,e.text};
 }
 bool expression(const Expr& e,State& state,std::size_t depth){
  tick(e.where,depth);if(e.prototype)return true;
     if(e.kind==E::PatternMatch||e.kind==E::Match||e.kind==E::Case||e.kind==E::Loop||e.kind==E::Propagate||e.kind==E::UnsafeBlock)fail(e.where,"surface value construct reached the canonical verifier");
   if(e.unsafe_region&&e.kind!=E::Region)fail(e.where,"unsafe marker on a non-region expression");
   if(e.reference==Expr::Reference::UnsafeForeign)
    fail(e.where,"unsafe foreign reference must be the direct callee of an unsafe call");
  if(e.noreturn&&e.kind!=E::Region)fail(e.where,"non-returning metadata on an ordinary expression");
  if(e.kind==E::Region||e.kind==E::TagTest||e.kind==E::Payload||e.kind==E::Project||e.kind==E::Unit){source(e.where);if(e.callee||!e.parameters.empty()||!e.patterns.empty()||!e.labels.empty())fail(e.where,"extraneous canonical expression fields");}
  if(e.kind==E::Variable){auto binding=find(e.text);if(binding||e.reference==Expr::Reference::Local){if(!binding||e.reference!=Expr::Reference::Local||binding->type!=e.type)fail(e.where,"canonical local binding/type mismatch: "+e.text);if(binding->storage&&!state.initialized.contains(binding->storage))fail(e.where,"read of uninitialized canonical storage: "+e.text);}return true;}
  if(e.kind==E::Lambda){
   for(const auto& capture:e.captures){auto b=find(capture.name);if(!b||b->mut||b->type!=capture.type||capture.type==Type::Json||capture.type==Type::JsonView||(b->storage&&!state.initialized.contains(b->storage)))fail(e.where,"uninitialized/invalid closure capture");}
   auto saved_scopes=std::move(scopes);auto saved_regions=std::move(regions),saved_loops=std::move(loops);scopes.clear();regions.clear();loops.clear();scopes.emplace_back();
   for(const auto& capture:e.captures)bind(capture.name,capture.type,false);
    auto saved_returns=returns;returns=e.result;params(e.parameters);
    auto saved_unsafe=unsafe_context;unsafe_context=false;
    (void)body(e.body,State{},depth+1,false);returns=saved_returns;unsafe_context=saved_unsafe;
   scopes=std::move(saved_scopes);regions=std::move(saved_regions);loops=std::move(saved_loops);return true;
  }
   if(e.kind==E::Region){
    if(e.children.size()!=1||!e.children[0])fail(e.where,"value region requires one result");
    auto saved_unsafe=unsafe_context;unsafe_context=unsafe_context||e.unsafe_region;
   scopes.emplace_back();auto after=body(e.body,state,depth+1,false);
   if(e.noreturn==after.has_value())fail(e.where,"incorrect value-region completion metadata");
   if(after){if(!expression(*e.children[0],*after,depth+1)||e.children[0]->type!=e.type)fail(e.where,"value-region result mismatch");state=*after;}
   else if(e.type!=Type::Void||e.children[0]->kind!=E::Unit)fail(e.where,"non-returning region must have no value");
    scopes.pop_back();unsafe_context=saved_unsafe;return after.has_value();
  }
  if(e.kind==E::Binary&&(e.text=="&&"||e.text=="||")){
   if(e.children.size()!=2)fail(e.where,"malformed logical expression");
   if(!expression(*e.children[0],state,depth+1))return false;
   charge(state,e.where);auto right=state;bool live=expression(*e.children[1],right,depth+1);if(live)state=meet(state,right);return true;
  }
   if(e.callee&&e.callee->reference==Expr::Reference::UnsafeForeign){
    tick(e.callee->where,depth+1);
    if(!unsafe_context||e.kind!=E::Call||e.callee->kind!=E::Variable||
       e.callee->callee||!e.callee->children.empty()||!e.callee->body.empty())
     fail(e.callee->where,"unsafe foreign call outside its checked lexical scope");
   }else if(e.callee&&!expression(*e.callee,state,depth+1))return false;
  for(const auto& child:e.children){if(!child)fail(e.where,"missing expression child");if(!expression(*child,state,depth+1))return false;}
  if(e.kind==E::TagTest){(void)tag(e);if(e.type!=Type::Bool)fail(e.where,"tag test result is not bool");}
  if(e.kind==E::Payload){auto fact=tag(e);if(!state.tags.contains(fact))fail(e.where,"payload projection lacks a dominating tag test");if(e.type!=e.children[0]->type.elements().at(e.projection))fail(e.where,"payload result type mismatch");}
   if(e.kind==E::Project){if(e.children.size()!=1||!e.children[0]->type.is_tuple()||e.projection>=e.children[0]->type.elements().size()||e.type!=e.children[0]->type.elements()[e.projection])fail(e.where,"invalid canonical tuple projection");}
   if(e.kind==E::Variant){if(e.type.kind()!=Type::Sum||e.result!=e.type||e.children.size()!=1||e.projection>=e.type.elements().size()||e.text!=e.type.labels()[e.projection]||e.children[0]->type!=e.type.elements()[e.projection])fail(e.where,"invalid variant construction");}
  if(e.kind==E::Unit&&(e.type!=Type::Void||!e.children.empty()||!e.body.empty()))fail(e.where,"invalid unit value");
  if(!e.body.empty())fail(e.where,"unexpected canonical expression body");
  return true;
 }
 Flow body(const std::vector<Stmt>& statements,State initial,std::size_t depth,bool scoped=true){
  if(scoped)scopes.emplace_back();
  Flow state=std::move(initial);
  for(const auto& s:statements){
    tick(s.where,depth);if(s.prototype)continue;if(!state)fail(s.where,"unreachable canonical statement");
    if(s.unsafe_block&&s.kind!=S::Block)fail(s.where,"unsafe marker on a non-block statement");
   charge(*state,s.where);
    if(s.kind==S::Storage||s.kind==S::Initialize||s.kind==S::Region||s.kind==S::Leave||s.kind==S::Trap||s.kind==S::PushDefer){
    source(s.where);if(s.pattern||s.mutable_binding||!s.label.empty()||s.label_context||s.context||s.inclusive||!s.otherwise.empty())fail(s.where,"extraneous canonical statement fields");
    if(s.kind!=S::Region&&!s.body.empty())fail(s.where,"body on a canonical non-region statement");
    if((s.kind==S::Region||s.kind==S::Leave||s.kind==S::Trap)&&(!s.name.empty()||s.value||s.declared!=Type::Invalid))fail(s.where,"invalid canonical transfer/region shape");
   }
    if(s.kind==S::Template){state=body(s.body,*state,depth+1,false);continue;}
    auto header_entry=s.kind==S::While?state:Flow{};
   if(s.value&&!expression(*s.value,*state,depth+1)){state.reset();continue;}
     switch(s.kind){
     case S::TaskGroup:case S::CspScope:state=body(s.body,*state,depth+1);break;
    case S::Defer:fail(s.where,"unlowered defer");
    case S::PushDefer:if(!s.name.empty()||s.declared!=Type::Invalid||!s.value||s.value->kind!=E::Lambda||s.value->type!=Type::function({},Type::Void)||!s.value->parameters.empty()||s.value->result!=Type::Void)fail(s.where,"invalid deferred thunk");break;
   case S::Let:if(!s.value)fail(s.where,"missing binding value");if(s.pattern)pattern(*s.pattern,s.value->type);else bind(s.name,s.value->type,s.mutable_binding);break;
   case S::Storage:
    if(s.value||s.pattern||s.declared==Type::Invalid||s.declared==Type::Void||s.declared==Type::Json||s.declared==Type::JsonView||s.declared.has_holes()||s.declared.has_generics())fail(s.where,"invalid canonical storage declaration");
     bind(s.name,s.declared,false,&s);state->initialized.erase(&s);state->maybe_initialized.erase(&s);break;
    case S::Initialize:{auto b=find(s.name);if(!b||!b->storage||!s.value||b->type!=s.value->type||state->maybe_initialized.contains(b->storage))fail(s.where,"invalid or repeated canonical initialization");state->initialized.insert(b->storage);state->maybe_initialized.insert(b->storage);break;}
   case S::Assign:{auto b=find(s.name);if(!b||!b->mut)fail(s.where,"canonical assignment to missing/immutable binding");for(auto it=state->tags.begin();it!=state->tags.end();)if(it->first==b->id)it=state->tags.erase(it);else ++it;break;}
   case S::Expr:break;
   case S::Return:if((s.value?s.value->type:Type{Type::Void})!=returns)fail(s.where,"canonical return type mismatch");state.reset();break;
   case S::Trap:state.reset();break;
    case S::Block:{auto saved=unsafe_context;unsafe_context=unsafe_context||s.unsafe_block;
     state=body(s.body,*state,depth+1);unsafe_context=saved;break;}
   case S::If:{
    if(!s.value||s.value->type!=Type::Bool)fail(s.where,"invalid canonical branch condition");
    auto yes=*state;if(s.value->kind==E::TagTest)yes.tags.insert(tag(*s.value));
    auto a=body(s.body,std::move(yes),depth+1),b=body(s.otherwise,*state,depth+1);state=merge(std::move(a),b);break;
   }
   case S::Region:{
    regions.push_back({s.control_target,{},{}});auto after=body(s.body,*state,depth+1);auto exits=std::move(regions.back().exits);regions.pop_back();for(const auto& exit:exits)after=merge(std::move(after),exit);state=std::move(after);break;
   }
   case S::Leave:{auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==regions.rend())fail(s.where,"region exit target missing");target->exits.push_back(*state);state.reset();break;}
    case S::While:{
     std::set<const Stmt*> enclosing;for(const auto& scope:scopes)for(const auto& [name,local]:scope){(void)name;if(local.storage)enclosing.insert(local.storage);}
     loops.push_back({s.control_target,{},{}});auto normal=body(s.body,*state,depth+1);for(const auto& c:loops.back().continues)normal=merge(std::move(normal),c);
     if(normal){auto backedge=body(s.otherwise,*normal,depth+1);if(backedge)for(auto storage:enclosing)if(!header_entry->maybe_initialized.contains(storage)&&backedge->maybe_initialized.contains(storage))fail(s.where,"canonical storage initialized on a repeatable loop backedge");}
    auto exits=std::move(loops.back().exits);loops.pop_back();for(const auto& exit:exits)state=merge(std::move(state),exit);state->tags.clear();break;
   }
    case S::Break:case S::Continue:{if(s.value)fail(s.where,"surface value exit reached the canonical verifier");auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==loops.rend())fail(s.where,"loop target missing");(s.kind==S::Break?target->exits:target->continues).push_back(*state);state.reset();break;}
   case S::Range:fail(s.where,"surface range in canonical values");
   case S::Template:break;
   }
  }
  if(scoped)scopes.pop_back();
  return state;
 }
public:
 void run(const Unit& unit){for(const auto& f:unit.functions){if(f.prototype||f.external)continue;returns=f.result;scopes.emplace_back();params(f.params);(void)body(f.body,State{},0,false);scopes.pop_back();}}
};
}
void verify_values(const Unit& unit){Verify().run(unit);}
}

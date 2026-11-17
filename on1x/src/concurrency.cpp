#include <on1x/concurrency.hpp>
#include <on1x/clone.hpp>
#include <on1x/numeric.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <type_traits>

namespace on1x::csp {
Type send_result(){return Type::data(Type::Sum,{Type::I64,Type::data(Type::Record,{})},{"Err","Ok"});}
Type receive_result(){return Type::data(Type::Sum,{Type::data(Type::Record,{}),Type::I64},{"None","Some"});}
bool contains(const Type& t){
 if(t.is_csp())return true;
 if(t.is_function()){if(contains(t.result()))return true;for(const auto& p:t.parameters())if(contains(p))return true;}
 if(t.is_tuple()||t.is_data())for(const auto& p:t.elements())if(contains(p))return true;
 if(t.is_code()&&contains(t.code_result()))return true;
 if(t.is_forall()&&contains(t.body()))return true;
 for(const auto& a:t.arguments())if(contains(a))return true;
 return false;
}
namespace {
using E=Expr::Kind;using S=Stmt::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-CSP: "+text,"ON1X-CSP");}
struct Budget {
 std::size_t nodes=262144,states=4*1024*1024;
 void tick(const Location& at,std::size_t depth){if(depth>520||!nodes--)throw Error(at,"concurrency verification depth/node budget exhausted","ON1X-CSP-LIMIT");}
 void state(std::size_t n,const Location& at){if(n>states)throw Error(at,"concurrency flow-state budget exhausted","ON1X-CSP-LIMIT");states-=n;}
};
bool operation(const Expr& e){return e.kind==E::Csp||e.kind==E::SourceCsp;}
// A bitset, not a boolean: after a branch, a capability may have been consumed.
// Such a value is neither usable nor (for a fiber) safely droppable.
constexpr unsigned live=1,dead=2;
using State=std::map<std::size_t,unsigned>;
using Flow=std::optional<State>;
class Verify {
 struct Local {Type type;bool mut,capture;std::size_t id;};
 struct Target {std::size_t id,keep;std::vector<State> exits,continues;};
  bool canonical,ready_cancel=false;Budget budget;std::size_t serial=0,groups=0;
 std::vector<std::map<std::string,Local>> scopes;
 std::vector<Target> loops,regions;
 std::set<std::string> capture_uses;
 void source(const Location& at){
  if(at.file.empty()||at.file.size()>4096||at.file.find('\0')!=std::string::npos||!at.line||!at.column||
     at.offset>max_source_bytes||at.line>max_source_bytes+1||at.column>max_source_bytes+1)fail(at,"invalid concurrency source point");
 }
 Flow merge(Flow a,const Flow& b,const Location& at){
  if(!a)return b;
  if(!b)return a;
  budget.state(a->size()+b->size(),at);
  for(const auto& [id,status]:*b)(*a)[id]|=status;
  return a;
 }
 const Local& local(const std::string& name,const Location& at){
  for(auto s=scopes.rbegin();s!=scopes.rend();++s)if(auto f=s->find(name);f!=s->end()){
   if(f->second.capture)capture_uses.insert(name);
   return f->second;
  }
  fail(at,"missing local/capture: "+name);
 }
 void bind(const std::string& name,Type type,bool mut,State& state,const Location& at,bool capture=false){
  if(name.empty()||scopes.back().contains(name))fail(at,"empty/duplicate concurrency local");
  auto id=++serial;scopes.back().emplace(name,Local{type,mut,capture,id});
  if(type.is_csp())state[id]=live;
 }
 void pattern(const Pattern& p,Type t,State& state){
  if(p.is_tuple()){
   if(!t.is_tuple()||p.elements.size()!=t.elements().size())fail(p.where,"binding pattern/type mismatch");
   for(std::size_t i=0;i<p.elements.size();++i)pattern(p.elements[i],t.elements()[i],state);
  }else bind(p.name,t,false,state,p.where);
 }
 State project(State state,std::size_t keep,const Location& at){
  budget.state(state.size(),at);
  for(std::size_t i=keep;i<scopes.size();++i)for(const auto& [name,b]:scopes[i]){
    if((b.type==Type::Fiber||b.type==Type::ExternalFiber)&&(state[b.id]&live))fail(at,"fiber must be joined or cancelled on every normal exit: "+name);
   state.erase(b.id);
  }
  return state;
 }
 void capability(const Expr& e,Type expected,State& state,bool consume){
  if(e.kind!=E::Variable||e.reference!=Expr::Reference::Local||e.type!=expected)fail(e.where,"operation requires a named affine "+type_name(expected));
  const auto& b=local(e.text,e.where);
  if(b.type!=expected||b.mut||!state.contains(b.id)||state.at(b.id)!=live)fail(e.where,"moved, conditionally consumed or invalid capability: "+e.text);
  if(consume)state[b.id]=dead;
 }
 void lambda(const Expr& e,State& parent,bool launched,std::size_t depth){
  if(!e.type.is_function()||e.result!=e.type.result()||e.parameters.size()!=e.type.parameters().size()||contains(e.type))fail(e.where,"invalid concurrency closure signature");
  std::set<std::string> names;
  for(const auto& c:e.captures){
   if(!names.insert(c.name).second)fail(e.where,"duplicate closure capture");
   const auto& b=local(c.name,e.where);
   if(b.type!=c.type||b.mut)fail(e.where,"invalid/mutable launch or closure capture");
   if(launched&&c.type!=Type::I64&&c.type!=Type::Sender&&c.type!=Type::Receiver)fail(e.where,"launch captures support only immutable i64 or scoped endpoints");
   if(contains(c.type)){
    if(!launched||(c.type!=Type::Sender&&c.type!=Type::Receiver)||parent[b.id]!=live)fail(e.where,"invalid or moved endpoint capture: "+c.name);
    parent[b.id]=dead;
   }
  }
  auto outer=std::move(scopes);auto outer_loops=std::move(loops),outer_regions=std::move(regions);
  auto used=std::move(capture_uses);auto saved_groups=groups;
  scopes.clear();loops.clear();regions.clear();capture_uses.clear();groups=launched?1:0;scopes.emplace_back();State state;
  for(const auto& c:e.captures)bind(c.name,c.type,false,state,e.where,true);
  for(std::size_t i=0;i<e.parameters.size();++i){
   const auto& p=e.parameters[i];if(p.type!=e.type.parameters()[i])fail(p.where,"closure parameter type mismatch");
   if(p.pattern)pattern(*p.pattern,p.type,state);else bind(p.name,p.type,false,state,p.where);
  }
  auto after=body(e.body,state,depth+1,false);
  if(after)(void)project(*after,0,e.where);
  if(names!=capture_uses)fail(e.where,"closure capture list does not match its body");
  scopes=std::move(outer);loops=std::move(outer_loops);regions=std::move(outer_regions);
  capture_uses=std::move(used);groups=saved_groups;
 }
  void shape(const Expr& e){
  source(e.where);
  if((canonical&&e.kind!=E::Csp)||e.callee||!e.body.empty()||!e.parameters.empty()||!e.labels.empty()||
     !e.patterns.empty()||!e.captures.empty()||!e.proofs.empty()||e.syntax_tail||!e.syntax_definitions.empty()||
     e.projection||e.control_target||e.noreturn||e.statement_match||e.reference!=Expr::Reference::Unresolved)
   fail(e.where,"invalid/private concurrency expression shape");
    auto count=e.text=="channel"?3:e.text=="send"||e.text=="offer_send"?2:1;
    if(e.text=="poll")count=static_cast<int>(e.children.size());
    if(e.text=="launch_exec")count=static_cast<int>(e.children.size());
   if(e.children.size()!=static_cast<std::size_t>(count))fail(e.where,"concurrency operand arity mismatch");
  for(const auto& c:e.children)if(!c)fail(e.where,"missing concurrency operand");
  if(e.result!=(e.text=="channel"?Type{Type::I64}:Type{Type::Void}))fail(e.where,"invalid concurrency payload annotation");
    Type result=e.text=="poll"?e.type:e.text=="offer_recv"||e.text=="offer_send"?Type{Type::Void}:e.text=="channel"?Type{Type::Channel}:e.text=="launch"?Type{Type::Fiber}:e.text=="launch_exec"?Type{Type::ExternalFiber}:
    e.text=="join"?Type{Type::I64}:e.text=="split"?Type::tuple({Type::Sender,Type::Receiver}):
    e.text=="close"||e.text=="cancel"?Type{Type::Bool}:e.text=="send"?send_result():e.text=="recv"?receive_result():Type{Type::Invalid};
   if(result==Type::Invalid||result!=e.type)fail(e.where,"unknown concurrency operation or result type");
  }
 bool expression(const Expr& e,State& state,std::size_t depth,bool acquire=false){
  budget.tick(e.where,depth);if(e.prototype)return true;
   if(operation(e)){
    shape(e);
    if(e.text=="poll"){
     if(e.children.empty()||e.children.size()>17)fail(e.where,"poll offer limit/arity mismatch");
     std::vector<Type> fields;std::vector<std::string> labels;std::set<std::size_t> seen;
     for(std::size_t i=0;i<e.children.size();++i){const auto& offer=*e.children[i];
      if(offer.kind==E::Boolean&&offer.text=="true"&&i+1==e.children.size()){
       fields.push_back(Type::data(Type::Record,{}));labels.push_back("Default");break;
      }
      if(!operation(offer)||(offer.text!="offer_recv"&&offer.text!="offer_send"))fail(offer.where,"invalid poll offer");
      shape(offer);
      if(offer.children.empty()||offer.children.size()!=(offer.text=="offer_recv"?1u:2u))fail(offer.where,"poll operand arity mismatch");
      const auto& endpoint=*offer.children[0];
      capability(endpoint,offer.text=="offer_recv"?Type::Receiver:Type::Sender,state,false);
      const auto& binding=local(endpoint.text,endpoint.where);
      if(!seen.insert(binding.id).second)fail(offer.where,"duplicate poll endpoint");
      if(offer.text=="offer_send"&&(!expression(*offer.children[1],state,depth+1)||offer.children[1]->type!=Type::I64))fail(offer.where,"poll send requires a completing i64 payload");
      for(const auto id:seen)if(state.at(id)!=live)fail(offer.where,"poll payload moved an offered endpoint");
      fields.push_back(offer.text=="offer_recv"?Type{Type::I64}:Type::data(Type::Record,{}));
      labels.push_back((offer.text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
      fields.push_back(Type::data(Type::Record,{}));labels.push_back("Closed"+std::to_string(i));
     }
     if(fields.size()<2||Type::data(Type::Sum,std::move(fields),std::move(labels))!=e.type)fail(e.where,"poll result/offer type mismatch");
     return true;
    }
     if(e.text=="offer_recv"||e.text=="offer_send")fail(e.where,"poll offer outside a checked poll");
     if(e.text=="launch_exec"){
      if(!acquire||!groups||e.children.empty()||e.children.size()>9)fail(e.where,"external launch must bind an xfiber inside a taskgroup (1..9 literal argv items)");
      for(const auto& a:e.children)if(a->kind!=E::String||a->type!=Type::String||a->text.size()>4098||a->text.find('\0')!=std::string::npos)fail(a->where,"external argv must be bounded constant strings");
      if(e.children[0]->text.size()<3||e.children[0]->text[1]!='/')fail(e.children[0]->where,"external executable path must be absolute");
      return true;
     }
    const auto& operand=*e.children[0];
   if(e.text=="launch"){
    if(!acquire||!groups||operand.kind!=E::Lambda||!operand.parameters.empty()||operand.result!=Type::I64)fail(e.where,"launch must bind an ifiber<i64> inside a taskgroup");
    lambda(operand,state,true,depth+1);
   }else if(e.text=="channel"){
    if(!acquire||!groups)fail(e.where,"channel must bind a descriptor inside a taskgroup");
    for(std::size_t i=0;i<3;++i){
     const auto& c=*e.children[i];
     if(c.kind!=E::Integer||c.type!=Type::I64||!c.children.empty())fail(c.where,"channel cardinality/capacity must be checked i64 literals");
     auto n=integer_literal_bits(c.text,c.where);
     if(i<2?n!=1:n<0||n>256)fail(c.where,"channel requires (1,1) cardinality and capacity in 0..256");
    }
   }else if(e.text=="split"){
    if(!acquire)fail(e.where,"split must bind exactly (sender,receiver)");
    capability(operand,Type::Channel,state,true);
   }else{
     auto t=e.text=="join"||e.text=="cancel"?operand.type:e.text=="recv"?Type::Receiver:Type::Sender;
     if(e.text=="join"&&t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"join requires a fiber handle");
     if(e.text=="cancel"&&t==Type::Fiber&&!ready_cancel)fail(e.where,"ON1X-CSP-UNSUPPORTED: internal cancellation requires the immediately preceding launch and no scheduling point");
     if(e.text=="cancel"&&t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"cancel requires a fiber handle");
     capability(operand,t,state,e.text=="join"||e.text=="cancel");
    if(e.text=="send"){
     if(!expression(*e.children[1],state,depth+1)||e.children[1]->type!=Type::I64)fail(e.where,"send requires a completing i64 operand");
     if(state.at(local(operand.text,operand.where).id)!=live)fail(e.where,"send payload moved its endpoint");
    }
   }
   return true;
  }
  if(contains(e.type))fail(e.where,"concurrency capabilities cannot be copied, aggregated or exported");
  if(e.kind==E::Variable&&e.reference==Expr::Reference::Local){
   const auto& b=local(e.text,e.where);if(b.type!=e.type)fail(e.where,"local type mismatch");return true;
  }
  if(e.kind==E::Variable&&e.reference==Expr::Reference::Intrinsic&&(e.text=="io_stdin"||e.text=="tool_read_file"))
   fail(e.where,"ON1X-CSP-UNSUPPORTED: blocking input/file intrinsics are unavailable in concurrent units");
  if(e.kind==E::Lambda){lambda(e,state,false,depth);return true;}
  if(e.kind==E::Region){
   if(e.children.size()!=1||!e.children[0])fail(e.where,"malformed value region");
   scopes.emplace_back();auto after=body(e.body,state,depth+1,false);
   if(after&&expression(*e.children[0],*after,depth+1))state=project(*after,scopes.size()-1,e.where);
   else after.reset();
   scopes.pop_back();return after.has_value();
  }
  if(e.kind==E::Binary&&(e.text=="&&"||e.text=="||")){
   if(e.children.size()!=2||!e.children[0]||!e.children[1])fail(e.where,"malformed short-circuit expression");
   if(!expression(*e.children[0],state,depth+1))return false;
   auto right=state;if(expression(*e.children[1],right,depth+1))state=*merge(state,right,e.where);return true;
  }
  if(e.kind==E::PatternMatch||e.kind==E::Match||e.kind==E::Loop||e.kind==E::Propagate)fail(e.where,"value lowering must precede concurrency admission");
  if(e.callee&&!expression(*e.callee,state,depth+1))return false;
  for(const auto& c:e.children){if(!c)fail(e.where,"missing operand");if(!expression(*c,state,depth+1))return false;}
  if(!e.body.empty())fail(e.where,"unexpected expression body in concurrency admission");
  return true;
 }
  Flow body(const std::vector<Stmt>& b,State initial,std::size_t depth,bool scoped=true){
   if(scoped)scopes.emplace_back();
   Flow state=std::move(initial);
   std::optional<std::size_t> ready;
   for(const auto& s:b){
    budget.tick(s.where,depth);if(s.prototype)continue;if(!state)fail(s.where,"unreachable statement in concurrency admission");
    budget.state(state->size(),s.where);
    bool immediate_cancel=s.kind==S::Expr&&s.value&&operation(*s.value)&&s.value->text=="cancel"&&
      s.value->children.size()==1&&s.value->children[0]&&s.value->children[0]->type==Type::Fiber;
    if(immediate_cancel){
     const auto& handle=*s.value->children[0];
     if(!ready||handle.kind!=E::Variable||local(handle.text,handle.where).id!=*ready)fail(s.where,"ON1X-CSP-UNSUPPORTED: internal cancellation requires the immediately preceding launch and no scheduling point");
    }
    ready.reset();
   if(s.kind==S::TaskGroup||s.kind==S::CspScope){
    source(s.where);
    if((canonical&&s.kind!=S::CspScope)||s.value||s.pattern||!s.name.empty()||s.declared!=Type::Invalid||
       s.mutable_binding||!s.otherwise.empty()||s.control_target||!s.label.empty()||s.label_context||s.context||s.inclusive)fail(s.where,"invalid taskgroup shape");
    ++groups;state=body(s.body,*state,depth+1);--groups;continue;
   }
   if(s.kind==S::Let&&s.value&&contains(s.value->type)){
    const auto& e=*s.value;
    if(s.mutable_binding||!operation(e))fail(s.where,"affine bindings must originate from launch/channel/split, never an alias");
    if(s.declared!=Type::Invalid&&s.declared!=e.type)fail(s.where,"affine binding annotation mismatch");
    if(e.text=="split"){
     if(!s.pattern||!s.pattern->is_tuple()||s.pattern->elements.size()!=2)fail(s.where,"split requires exactly (sender,receiver)");
     for(const auto& p:s.pattern->elements)if(p.kind!=Pattern::Kind::Name||!p.elements.empty())fail(p.where,"split endpoints require two simple names");
     }else if((e.text!="channel"&&e.text!="launch"&&e.text!="launch_exec")||s.pattern)fail(s.where,"invalid affine acquisition binding");
    (void)expression(e,*state,depth+1,true);
     if(s.pattern)pattern(*s.pattern,e.type,*state);else {bind(s.name,e.type,false,*state,s.where);if(e.text=="launch")ready=scopes.back().at(s.name).id;}
    continue;
   }
   if(contains(s.declared))fail(s.where,"concurrency storage/assignment/annotation is outside an affine acquisition");
   if(s.kind==S::Expr&&s.value&&operation(*s.value)&&s.value->text=="send")fail(s.where,"send outcome must be handled explicitly (match or let); statement error effects are not yet supported");
   auto header=state;
    ready_cancel=immediate_cancel;
    if(s.value&&s.kind!=S::Template&&!expression(*s.value,*state,depth+1)){ready_cancel=false;state.reset();continue;}
    ready_cancel=false;
   switch(s.kind){
   case S::Let:if(!s.value)fail(s.where,"missing binding value");if(s.pattern)pattern(*s.pattern,s.value->type,*state);else bind(s.name,s.value->type,s.mutable_binding,*state,s.where);break;
   case S::Storage:bind(s.name,s.declared,false,*state,s.where);break;
   case S::Assign:case S::Initialize:if(contains(local(s.name,s.where).type))fail(s.where,"affine reassignment is unsupported");break;
   case S::Return:(void)project(*state,0,s.where);state.reset();break;
   case S::Trap:state.reset();break;
   case S::If:{auto a=body(s.body,*state,depth+1),n=body(s.otherwise,*state,depth+1);state=merge(a,n,s.where);break;}
   case S::Block:state=body(s.body,*state,depth+1);break;
   case S::Template:state=body(s.body,*state,depth+1,false);break;
   case S::Region:{
    regions.push_back({s.control_target,scopes.size(),{},{}});
    auto after=body(s.body,*state,depth+1);auto exits=std::move(regions.back().exits);regions.pop_back();
    for(const auto& exit:exits)after=merge(after,exit,s.where);
    state=std::move(after);break;
   }
   case S::While:{
    loops.push_back({s.control_target,scopes.size(),{},{}});
    auto after=body(s.body,*state,depth+1);
    for(const auto& c:loops.back().continues)after=merge(after,c,s.where);
    if(after){
     after=body(s.otherwise,*after,depth+1);
     if(after&&*after!=*header)fail(s.where,"repeatable loop backedge changes an enclosing affine capability");
    }
    auto exits=std::move(loops.back().exits);loops.pop_back();
    if(s.value&&s.value->kind==E::Boolean&&s.value->text=="true")state.reset();
    for(const auto& exit:exits)state=merge(state,exit,s.where);
    break;
   }
   case S::Break:case S::Continue:case S::Leave:{
    auto& targets=s.kind==S::Leave?regions:loops;
    auto t=std::find_if(targets.rbegin(),targets.rend(),[&](const auto& p){return p.id==s.control_target;});
    if(t==targets.rend())fail(s.where,"missing affine flow target");
    auto exit=project(*state,t->keep,s.where);
    (s.kind==S::Continue?t->continues:t->exits).push_back(std::move(exit));state.reset();break;
   }
   case S::Range:fail(s.where,"unlowered range in concurrency admission");
   case S::Defer:fail(s.where,"defer lowering must precede concurrency admission");
   case S::Expr:case S::PushDefer:break;
   case S::TaskGroup:case S::CspScope:break;
   }
  }
  if(scoped){if(state)state=project(*state,scopes.size()-1,b.empty()?Location{"<taskgroup>"}:b.back().where);scopes.pop_back();}
  return state;
 }
public:
 explicit Verify(bool c):canonical(c){}
 void run(const Unit& u){
  for(const auto& f:u.functions){
   if(f.prototype)continue;
   if(f.external)fail(f.where,"ON1X-CSP-UNSUPPORTED: raw foreign effects are unavailable in concurrent units");
   if(contains(f.result))fail(f.where,"concurrency handles cannot escape through function results");
   scopes.emplace_back();State state;
   for(const auto& p:f.params){
    if(contains(p.type))fail(p.where,"concurrency handles cannot cross ordinary function signatures");
    if(p.pattern)pattern(*p.pattern,p.type,state);else bind(p.name,p.type,false,state,p.where);
   }
   auto after=body(f.body,state,0,false);if(after)(void)project(*after,0,f.where);
   scopes.pop_back();
  }
 }
};
template<class F>void walk(Unit& u,F apply){
 Budget budget;
 auto expr=[&](auto&& self,Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);apply(e);
  if(e.callee)self(self,*e.callee,depth+1);
  for(auto& c:e.children){if(!c)fail(e.where,"missing expression child");self(self,*c,depth+1);}
  auto body=[&](auto&& visit,std::vector<Stmt>& b,std::size_t d)->void{for(auto& s:b){budget.tick(s.where,d);apply(s);if(s.value&&s.kind!=S::Template)self(self,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};body(body,e.body,depth+1);
 };
 auto body=[&](auto&& visit,std::vector<Stmt>& b,std::size_t d)->void{for(auto& s:b){budget.tick(s.where,d);apply(s);if(s.value&&s.kind!=S::Template)expr(expr,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};
 for(auto& f:u.functions)if(!f.prototype)body(body,f.body,0);
}
}
bool present(const Unit& u){
 Budget budget;bool found=false;
 auto expr=[&](auto&& self,const Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);if(operation(e)||contains(e.type)||contains(e.result))found=true;
  if(e.callee)self(self,*e.callee,depth+1);
  for(const auto& c:e.children){if(!c)fail(e.where,"missing expression child");self(self,*c,depth+1);}
  auto body=[&](auto&& visit,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.kind==S::TaskGroup||s.kind==S::CspScope||contains(s.declared))found=true;if(s.value&&s.kind!=S::Template)self(self,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};body(body,e.body,depth+1);
 };
 auto body=[&](auto&& visit,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.kind==S::TaskGroup||s.kind==S::CspScope||contains(s.declared))found=true;if(s.value&&s.kind!=S::Template)expr(expr,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};
 for(const auto& f:u.functions)if(!f.prototype){if(contains(f.result))found=true;for(const auto& p:f.params)if(contains(p.type))found=true;body(body,f.body,0);}
 return found;
}
void verify(const Unit& u,bool canonical){if(present(u))Verify(canonical).run(u);}
void lower(Unit& u){
 if(!present(u))return;
 verify(u,false);std::size_t budget=262144;auto copy=clone_unit(u,budget);
 walk(copy,[]<class T>(T& node){
  if constexpr(std::is_same_v<T,Expr>){if(node.kind==E::SourceCsp)node.kind=E::Csp;}
  else if(node.kind==S::TaskGroup)node.kind=S::CspScope;
 });
 verify(copy,true);u=std::move(copy);
}
}

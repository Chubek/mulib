#include <on1x/canonical/values.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/clone.hpp>
#include <on1x/patterns.hpp>
#include <algorithm>
#include <functional>
#include <iterator>
#include <set>

namespace on1x::canonical {
namespace {
using E=Expr::Kind;using S=Stmt::Kind;using P=Pattern::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-VALUES: "+text,"ON1X-VALUES");}
ExprPtr value(E k,Type t,const Location& at,std::string text={}){auto e=std::make_unique<Expr>();e->kind=k;e->type=t;e->where=at;e->text=std::move(text);return e;}
ExprPtr local(const std::string& name,Type t,const Location& at){auto e=value(E::Variable,t,at,name);e->reference=Expr::Reference::Local;return e;}
ExprPtr integer(std::size_t n,const Location& at){return value(E::Integer,Type::I64,at,std::to_string(n));}
ExprPtr equal(ExprPtr a,ExprPtr b,const Location& at){auto e=value(E::Binary,Type::Bool,at,"==");e->children.push_back(std::move(a));e->children.push_back(std::move(b));return e;}
Stmt leave(std::size_t target,const Location& at){Stmt s;s.kind=S::Leave;s.control_target=target;s.where=at;return s;}
class Lower {
  Unit& unit;std::size_t serial=0,budget=262144,copy_budget=262144,bytes=8*1024*1024;
  struct LoopValue {std::size_t target,done;std::string result;Type type;};
  std::vector<LoopValue> loops;
 void tick(const Location& at){if(!budget--)throw Error(at,"canonical value expansion budget exhausted","ON1X-VALUES-LIMIT");}
 void charge(const Pattern& p){auto size=p.name.size()+p.where.file.size();for(const auto& label:p.labels){if(label.size()>bytes)throw Error(p.where,"pattern plan byte budget exhausted","ON1X-VALUES-LIMIT");bytes-=label.size();}if(size>bytes)throw Error(p.where,"pattern plan byte budget exhausted","ON1X-VALUES-LIMIT");bytes-=size;}
 Pattern copy_pattern(const Pattern& p){auto account=[&](auto&& self,const Pattern& part)->void{tick(part.where);charge(part);for(const auto& c:part.elements)self(self,c);};account(account,p);return clone_pattern(p,copy_budget);}
 std::string fresh(){return "@decision_"+std::to_string(++serial);}
 Stmt bind(const std::string& name,ExprPtr e,const Location& at){Stmt s;s.kind=S::Let;s.where=at;s.name=name;s.declared=e->type;s.value=std::move(e);return s;}
 Stmt store(const std::string& name,ExprPtr e,const Location& at){Stmt s;s.kind=S::Initialize;s.name=name;s.where=at;s.value=std::move(e);return s;}
 std::vector<Pattern> alternatives(const Pattern& p){
  tick(p.where);charge(p);if(p.kind==P::Or){auto out=alternatives(p.elements.at(0)),right=alternatives(p.elements.at(1));if(right.size()>4096-out.size())throw Error(p.where,"pattern plan alternative budget exhausted","ON1X-VALUES-LIMIT");out.insert(out.end(),std::make_move_iterator(right.begin()),std::make_move_iterator(right.end()));return out;}
  Pattern base;base.kind=p.kind;base.where=p.where;base.name=p.name;base.context=p.context;base.labels=p.labels;base.open=p.open;base.type=p.type;std::vector<Pattern> out;out.push_back(std::move(base));
  for(const auto& field:p.elements){auto options=alternatives(field);std::vector<Pattern> next;for(const auto& prefix:out)for(const auto& option:options){tick(p.where);auto n=copy_pattern(prefix);n.elements.push_back(copy_pattern(option));next.push_back(std::move(n));if(next.size()>4096)throw Error(p.where,"pattern plan alternative budget exhausted","ON1X-VALUES-LIMIT");}out=std::move(next);}return out;
 }
 struct Binding {std::string name,root;Type type;};
 using Bindings=std::vector<Binding>;using Next=std::function<void(std::vector<Stmt>&,Bindings)>;
 void test(const Pattern& p,const std::string& root,std::vector<Stmt>& out,Bindings bindings,const Next& next){
  tick(p.where);charge(p);auto t=p.type;auto at=p.where;
  if(p.kind==P::As){bindings.push_back({p.name,root,t});test(p.elements.at(0),root,out,std::move(bindings),next);return;}
  if(p.kind==P::Name&&!p.is_tuple()){bindings.push_back({p.name,root,t});next(out,std::move(bindings));return;}
  if(p.kind==P::Wildcard||p.kind==P::Rest){next(out,std::move(bindings));return;}
  if(p.kind==P::Literal){Stmt branch;branch.kind=S::If;branch.where=at;branch.value=equal(local(root,t,at),std::make_unique<Expr>(patterns::literal(p)),at);next(branch.body,std::move(bindings));out.push_back(std::move(branch));return;}
  if(p.kind==P::Variant){
   auto tag=std::find(t.labels().begin(),t.labels().end(),p.name);auto index=static_cast<std::size_t>(tag-t.labels().begin());auto payload=t.elements().at(index);
   Stmt branch;branch.kind=S::If;branch.where=at;branch.value=value(E::TagTest,Type::Bool,at,p.name);branch.value->projection=index;branch.value->children.push_back(local(root,t,at));
   auto name=fresh();auto get=value(E::Payload,payload,at,p.name);get->projection=index;get->children.push_back(local(root,t,at));branch.body.push_back(bind(name,std::move(get),at));
   if(p.elements.size()==1)test(p.elements[0],name,branch.body,std::move(bindings),next);
   else{Pattern tuple;tuple.kind=P::Tuple;tuple.where=at;tuple.type=payload;tuple.elements=p.elements;test(tuple,name,branch.body,std::move(bindings),next);}
   out.push_back(std::move(branch));return;
  }
  std::vector<Stmt>* destination=&out;Stmt length_branch;
  if(p.kind==P::Array&&t.kind()==Type::Slice){
   length_branch.kind=S::If;length_branch.where=at;auto length=value(E::Length,Type::I64,at);length->children.push_back(local(root,t,at));length_branch.value=equal(std::move(length),integer(p.elements.size(),at),at);destination=&length_branch.body;
  }
  std::function<void(std::size_t,std::vector<Stmt>&,Bindings)> fields;
  fields=[&](std::size_t i,std::vector<Stmt>& into,Bindings bound){
   if(i==p.elements.size()){next(into,std::move(bound));return;}
   const auto& child=p.elements[i];ExprPtr get;
   if(p.is_tuple()){get=value(E::Project,t.elements().at(i),child.where);get->projection=i;get->children.push_back(local(root,t,at));}
   else if(p.kind==P::Record){auto tag=std::find(t.labels().begin(),t.labels().end(),p.labels.at(i));auto index=static_cast<std::size_t>(tag-t.labels().begin());get=value(E::Field,t.elements().at(index),child.where,p.labels[i]);get->projection=index;get->children.push_back(local(root,t,at));}
   else if(p.kind==P::Array){get=value(E::Index,t.elements().at(0),child.where);get->children.push_back(local(root,t,at));get->children.push_back(integer(i,child.where));}
   else fail(p.where,"unhandled structural decision");
   auto name=fresh();into.push_back(bind(name,std::move(get),child.where));test(child,name,into,std::move(bound),[&](std::vector<Stmt>& continuation,Bindings b){fields(i+1,continuation,std::move(b));});
  };
  fields(0,*destination,std::move(bindings));if(destination!=&out)out.push_back(std::move(length_branch));
 }
 void old_match(Expr& e){
  if(e.children.empty()||!e.children[0]||e.children[0]->type.kind()!=Type::Sum||e.children.size()!=e.labels.size()+1)fail(e.where,"legacy match requires checked closed-sum arms");
  auto schema=e.children.at(0)->type;std::vector<ExprPtr> arms;arms.push_back(std::move(e.children[0]));
  for(std::size_t i=0;i<e.labels.size();++i){auto ctor=e.labels[i];auto tag=std::find(schema.labels().begin(),schema.labels().end(),ctor);auto t=schema.elements().at(static_cast<std::size_t>(tag-schema.labels().begin()));
   auto name=fresh();auto arm=value(E::Case,Type::Void,e.where);Pattern p;p.kind=P::Variant;p.name=ctor;p.type=schema;p.where=e.where;Pattern binding;binding.name=name;binding.type=t;binding.where=e.where;p.elements.push_back(std::move(binding));arm->patterns.push_back(std::move(p));
   arm->children.push_back(value(E::Boolean,Type::Bool,e.where,"true"));auto call=value(E::Call,e.type,e.where);call->callee=std::move(e.children[i+1]);call->children.push_back(local(name,t,e.where));arm->children.push_back(std::move(call));arms.push_back(std::move(arm));
  }
  e.kind=E::PatternMatch;e.children=std::move(arms);e.labels.clear();e.statement_match=false;
 }
  void expression(Expr& e){
   tick(e.where);if(e.prototype)return;
   if(e.kind==E::Propagate){
    expression(*e.children.at(0));auto at=e.where;auto input=e.children[0]->type;auto root=fresh();auto result=fresh();
    Expr region;region.kind=E::Region;region.type=e.type;region.where=at;
    region.body.push_back(bind(root,std::move(e.children[0]),at));
    Stmt slot;slot.kind=S::Storage;slot.name=result;slot.declared=e.type;slot.where=at;region.body.push_back(std::move(slot));
    Stmt branch;branch.kind=S::If;branch.where=at;branch.value=value(E::TagTest,Type::Bool,at,"Ok");branch.value->projection=1;branch.value->children.push_back(local(root,input,at));
    auto ok=value(E::Payload,e.type,at,"Ok");ok->projection=1;ok->children.push_back(local(root,input,at));branch.body.push_back(store(result,std::move(ok),at));
    Stmt err;err.kind=S::If;err.where=at;err.value=value(E::TagTest,Type::Bool,at,"Err");err.value->projection=0;err.value->children.push_back(local(root,input,at));
    auto payload=value(E::Payload,input.arguments()[1],at,"Err");payload->projection=0;payload->children.push_back(local(root,input,at));
    Stmt ret;ret.kind=S::Return;ret.where=at;ret.value=value(E::Variant,e.result,at,"Err");ret.value->result=e.result;ret.value->projection=0;ret.value->children.push_back(std::move(payload));err.body.push_back(std::move(ret));
    Stmt trap;trap.kind=S::Trap;trap.where=at;err.otherwise.push_back(std::move(trap));branch.otherwise.push_back(std::move(err));region.body.push_back(std::move(branch));
    region.children.push_back(local(result,e.type,at));e=std::move(region);return;
   }
   if(e.kind==E::Loop){
    if(!e.control_target||e.type==Type::Invalid||e.type.has_holes()||e.type.has_generics()||e.type==Type::Json||e.type==Type::JsonView)fail(e.where,"loop requires a checked concrete result type");
    Expr region;region.kind=E::Region;region.type=e.type;region.where=e.where;
    auto result=e.type==Type::Void?std::string{}:fresh();auto done=result.empty()?0:++serial;
    if(!result.empty()){Stmt slot;slot.kind=S::Storage;slot.where=e.where;slot.name=result;slot.declared=e.type;region.body.push_back(std::move(slot));}
    loops.push_back({e.control_target,done,result,e.type});body(e.body);loops.pop_back();
    Stmt loop;loop.kind=S::While;loop.where=e.where;loop.label=e.text;loop.label_context=e.context;loop.control_target=e.control_target;loop.value=value(E::Boolean,Type::Bool,e.where,"true");loop.body=std::move(e.body);
    if(done){
     Stmt exit;exit.kind=S::Region;exit.where=e.where;exit.control_target=done;exit.body.push_back(std::move(loop));
     // The condition is literal true. Keep the impossible normal edge explicit
     // and fatal so no emitter/verifier can read an uninitialized result slot.
     Stmt unreachable;unreachable.kind=S::Trap;unreachable.where=e.where;exit.body.push_back(std::move(unreachable));region.body.push_back(std::move(exit));
    }else region.body.push_back(std::move(loop));
    region.children.push_back(result.empty()?value(E::Unit,Type::Void,e.where):local(result,e.type,e.where));e=std::move(region);return;
   }
  if(e.kind==E::Match)old_match(e);
  if(e.kind==E::PatternMatch){
   if(e.children.size()<2||e.children[0]->type==Type::Invalid||e.children[0]->type==Type::Void||e.type==Type::Invalid||e.type.has_holes()||e.type.has_generics())fail(e.where,"match requires checked concrete value/result types");
   for(std::size_t i=1;i<e.children.size();++i){auto& arm=*e.children[i];if(arm.kind!=E::Case||arm.patterns.size()!=1||arm.children.size()!=2||arm.children[0]->type!=Type::Bool)fail(arm.where,"malformed checked match arm");(void)patterns::check(arm.patterns[0],e.children[0]->type);}
   expression(*e.children.at(0));for(std::size_t i=1;i<e.children.size();++i){auto& arm=*e.children[i];expression(*arm.children.at(0));body(arm.body);expression(*arm.children.at(1));}
   Expr region;region.kind=E::Region;region.type=e.type;region.where=e.where;region.noreturn=e.noreturn;
   auto root=e.text.empty()?fresh():e.text;auto input_type=e.children[0]->type;region.body.push_back(bind(root,std::move(e.children[0]),e.where));
   auto result=fresh();if(e.type!=Type::Void){Stmt slot;slot.kind=S::Storage;slot.where=e.where;slot.name=result;slot.declared=e.type;region.body.push_back(std::move(slot));}
   Stmt decision;decision.kind=S::Region;decision.control_target=++serial;decision.where=e.where;auto done=decision.control_target;
   for(std::size_t i=1;i<e.children.size();++i){auto& arm=*e.children[i];auto& pattern=arm.patterns.at(0);Stmt skip;skip.kind=S::Region;skip.control_target=++serial;skip.where=arm.where;
    pattern_bindings(pattern,[&](const Pattern& p){Stmt slot;slot.kind=S::Storage;slot.where=p.where;slot.name=p.name;slot.declared=p.type;skip.body.push_back(std::move(slot));});
    Stmt choose;choose.kind=S::Region;choose.control_target=++serial;choose.where=arm.where;auto selected=choose.control_target;
    for(const auto& alternative:alternatives(pattern)){
     if(patterns::terminates(choose.body))break;
     test(alternative,root,choose.body,{},[&](std::vector<Stmt>& into,Bindings bindings){for(const auto& b:bindings)into.push_back(store(b.name,local(b.root,b.type,arm.where),arm.where));into.push_back(leave(selected,arm.where));});
    }
    if(!patterns::terminates(choose.body))choose.body.push_back(leave(skip.control_target,arm.where));
    skip.body.push_back(std::move(choose));
    Stmt guard;guard.kind=S::If;guard.where=arm.where;guard.value=std::move(arm.children[0]);Stmt block;block.kind=S::Block;block.where=arm.where;block.body=std::move(arm.body);
    if(!patterns::terminates(block.body)){
     if(e.statement_match||e.type==Type::Void){Stmt action;action.kind=S::Expr;action.where=arm.where;action.value=std::move(arm.children[1]);block.body.push_back(std::move(action));}
     else block.body.push_back(store(result,std::move(arm.children[1]),arm.where));
     if(!patterns::terminates(block.body))block.body.push_back(leave(done,arm.where));
    }
    guard.body.push_back(std::move(block));skip.body.push_back(std::move(guard));decision.body.push_back(std::move(skip));
   }
   Stmt unreachable;unreachable.kind=S::Trap;unreachable.where=e.where;decision.body.push_back(std::move(unreachable));region.body.push_back(std::move(decision));
   region.children.push_back(e.type==Type::Void?value(E::Unit,Type::Void,e.where):local(result,e.type,e.where));e=std::move(region);return;
  }
  if(e.callee)expression(*e.callee);
  for(auto& c:e.children)expression(*c);
  body(e.body);
 }
  void body(std::vector<Stmt>& b){for(auto& s:b){
   tick(s.where);if(s.prototype)continue;
   if(s.kind==S::Defer){
    auto at=s.where;auto call=std::move(s.value);Stmt scope;scope.kind=S::Block;scope.where=at;
    if(call->callee->reference!=Expr::Reference::Intrinsic&&call->callee->reference!=Expr::Reference::Extern){auto t=call->callee->type;auto n=fresh();scope.body.push_back(bind(n,std::move(call->callee),at));call->callee=local(n,t,at);}
    for(auto& a:call->children){auto t=a->type;auto n=fresh();scope.body.push_back(bind(n,std::move(a),at));a=local(n,t,at);}
    auto thunk=value(E::Lambda,Type::function({},Type::Void),at);thunk->result=Type::Void;
    Stmt invoke;invoke.kind=S::Expr;invoke.where=at;invoke.value=std::move(call);thunk->body.push_back(std::move(invoke));
    Stmt push;push.kind=S::PushDefer;push.where=at;push.value=std::move(thunk);scope.body.push_back(std::move(push));s=std::move(scope);
   }
   if(s.kind==S::Break){
    auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const auto& l){return l.target==s.control_target;});
    if(target!=loops.rend()&&(s.value||target->done)){
     auto plan=*target;if(!s.value)fail(s.where,"bare break in a value-producing loop");expression(*s.value);
     Stmt block;block.kind=S::Block;block.where=s.where;
     if(plan.type==Type::Void||s.value->noreturn){Stmt action;action.kind=S::Expr;action.where=s.where;action.value=std::move(s.value);block.body.push_back(std::move(action));}
     else{if(s.value->type!=plan.type)fail(s.where,"loop result initialization type mismatch");block.body.push_back(store(plan.result,std::move(s.value),s.where));}
     if(!patterns::terminates(block.body)){if(plan.done)block.body.push_back(leave(plan.done,s.where));else block.body.push_back(std::move(s));}
     s=std::move(block);continue;
    }
    if(s.value)fail(s.where,"value exit missing its lexical loop plan");
   }
   if(s.value&&s.kind!=S::Template)expression(*s.value);
   body(s.body);body(s.otherwise);
  }}
public:
 explicit Lower(Unit& u):unit(u){
   auto scan=[&](auto&& self,const Expr& e)->void{serial=std::max(serial,e.control_target);if(e.callee)self(self,*e.callee);for(const auto& c:e.children)self(self,*c);auto visit=[&](auto&& walk,const std::vector<Stmt>& b)->void{for(const auto& s:b){serial=std::max(serial,s.control_target);if(s.value)self(self,*s.value);walk(walk,s.body);walk(walk,s.otherwise);}};visit(visit,e.body);};
  for(const auto& f:u.functions){auto visit=[&](auto&& walk,const std::vector<Stmt>& b)->void{for(const auto& s:b){serial=std::max(serial,s.control_target);if(s.value)scan(scan,*s.value);walk(walk,s.body);walk(walk,s.otherwise);}};visit(visit,f.body);}
 }
 void run(){for(auto& f:unit.functions)if(!f.prototype)body(f.body);}
};
}
bool needs_values(const Unit& unit){
  auto expression=[&](auto&& self,const Expr& e)->bool{if(e.kind==E::PatternMatch||e.kind==E::Match||e.kind==E::Loop)return true;if(e.callee&&self(self,*e.callee))return true;for(const auto& c:e.children)if(self(self,*c))return true;auto body=[&](auto&& walk,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if(!s.prototype&&((s.value&&s.kind!=S::Template&&self(self,*s.value))||walk(walk,s.body)||walk(walk,s.otherwise)))return true;return false;};return body(body,e.body);};
 for(const auto& f:unit.functions)if(!f.prototype){auto body=[&](auto&& walk,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if(!s.prototype&&((s.value&&s.kind!=S::Template&&expression(expression,*s.value))||walk(walk,s.body)||walk(walk,s.otherwise)))return true;return false;};if(body(body,f.body))return true;}
 return false;
}
std::set<std::size_t> exit_targets(const Function& function,Stmt::Kind kind){
 std::set<std::size_t> out;
 auto expr=[&](auto&& self,const Expr& e)->void{if(e.prototype)return;if(e.callee)self(self,*e.callee);for(const auto& c:e.children)self(self,*c);auto visit=[&](auto&& walk,const std::vector<Stmt>& b)->void{for(const auto& s:b){if(s.prototype)continue;if(s.kind==kind)out.insert(s.control_target);if(s.value&&s.kind!=S::Template)self(self,*s.value);walk(walk,s.body);walk(walk,s.otherwise);}};visit(visit,e.body);};
 auto visit=[&](auto&& walk,const std::vector<Stmt>& b)->void{for(const auto& s:b){if(s.prototype)continue;if(s.kind==kind)out.insert(s.control_target);if(s.value&&s.kind!=S::Template)expr(expr,*s.value);walk(walk,s.body);walk(walk,s.otherwise);}};visit(visit,function.body);return out;
}
void lower_values(Unit& unit){verify_control(unit);std::size_t budget=262144;auto working=clone_unit(unit,budget);Lower(working).run();verify_control(working);unit=std::move(working);}
}

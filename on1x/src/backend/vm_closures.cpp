#include <on1x/backend/vm_closures.hpp>
#include <on1x/clone.hpp>
#include <map>
#include <set>

namespace on1x::mir {
namespace {
using K=Expr::Kind;
using R=Expr::Reference;
using S=Stmt::Kind;
[[noreturn]] void unsupported(const Location& at,std::string reason){
 throw Error(at,"ON1X-MIR-UNSUPPORTED: VM closure conversion: "+reason,"ON1X-MIR-UNSUPPORTED");
}
class Conversion{
 std::size_t left,serial=0;
 struct Alternative {std::string function;std::vector<Capture> captures;std::vector<std::size_t> slots;};
 struct Group {std::vector<Alternative> choices;std::vector<Type> fields;Type rep;std::string dispatcher;Location at;};
 std::map<Type,Group> groups;
 std::map<const Expr*,std::pair<Type,std::size_t>> values;
 std::map<const Expr*,std::string> lambdas;
 std::vector<on1x::Function> lifted;
 std::set<std::string> names;
 bool found=false;
 void tick(const Location& at){if(!left)unsupported(at,"node budget exhausted");--left;}
 std::string fresh(){
  std::string s;do{s="@vm_closure_"+std::to_string(++serial);}while(!names.insert(s).second);return s;
 }
 static bool direct(const Expr& e){return e.kind==K::Variable&&e.reference!=R::Local;}
 static bool capture_type(const Type& t){
  if(t.is_numeric()||t==Type::Bool)return true;
  if(!t.is_tuple()&&!t.is_data())return false;
  if(t.contains_unique()||t.has_functions())return false;
  for(const auto& field:t.elements())if(!capture_type(field))return false;
  return true;
 }
 static bool eligible(const Type& t){
  for(const auto& p:t.parameters())if(!capture_type(p))return false;
  return t.result()==Type::Void||capture_type(t.result());
 }
 // Reserve user and compiler-private spellings *before* creating any helper.
 // Skip cloning altogether in the common case with no runtime function value.
 void reserve(const Expr& e,bool callee=false){
  tick(e.where);names.insert(e.text);if(e.prototype)return;
  found|=e.kind==K::Lambda||(e.kind==K::Variable&&e.reference==R::Function&&!callee);
  for(const auto& cap:e.captures)names.insert(cap.name);
  for(const auto& p:e.parameters)reserve(p);
  if(e.callee)reserve(*e.callee,true);
  for(const auto& c:e.children)reserve(*c);
  reserve(e.body);
 }
 void reserve(const Param& p){names.insert(p.name);if(p.pattern)
  pattern_bindings(*p.pattern,[&](const Pattern& binding){names.insert(binding.name);});}
 void reserve(const std::vector<Stmt>& stmts){
  for(const auto& s:stmts){
   if(s.prototype||s.kind==S::Template)continue;
   tick(s.where);names.insert(s.name);
   if(s.pattern)pattern_bindings(*s.pattern,[&](const Pattern& binding){names.insert(binding.name);});
   if(s.value)reserve(*s.value);
   reserve(s.body);reserve(s.otherwise);
  }
 }
 void alt(const Expr& e,std::string fn,std::vector<Capture> captures){
  if(!eligible(e.type))unsupported(e.where,"only closed first-order callback signatures are represented");
  auto& g=groups[e.type];g.at=e.where;
  for(std::size_t i=0;i<g.choices.size();++i)if(g.choices[i].function==fn){
   values[&e]={e.type,i};return;
  }
  if(g.choices.size()>=128)unsupported(e.where,"alternative limit");
  values[&e]={e.type,g.choices.size()};g.choices.push_back({std::move(fn),std::move(captures),{}});
 }
 void inspect(const Expr& e,bool callee=false){
  tick(e.where);names.insert(e.text);
  for(const auto& p:e.parameters)names.insert(p.name);
  for(const auto& c:e.captures)names.insert(c.name);
  if(e.prototype)return;
  if(e.kind==K::Lambda){
   if(!e.type.is_function()||!e.type_parameters.empty()||!e.constraints.empty())unsupported(e.where,"non-monomorphic lambda");
   for(const auto& c:e.captures)if(!capture_type(c.type))unsupported(e.where,"capture requires a managed, higher-order or ownership representation");
   auto name=fresh();lambdas.emplace(&e,name);
   if(!callee)alt(e,name,e.captures);
  }else if(!callee&&e.kind==K::Variable&&e.reference==R::Function)alt(e,e.text,{});
  if(e.callee)inspect(*e.callee,true);
  for(const auto& child:e.children)inspect(*child);
  inspect(e.body);
 }
 void inspect(const std::vector<Stmt>& body){
  for(const auto& s:body){tick(s.where);names.insert(s.name);if(s.prototype||s.kind==S::Template)continue;
   if(s.value)inspect(*s.value);
   inspect(s.body);inspect(s.otherwise);}
 }
 Type rep(const Type& t,const Location& at) const{
  if(t.is_function()){
   auto it=groups.find(t);if(it==groups.end())unsupported(at,"callback signature has no closed executable targets");
   return it->second.rep;
  }
  if(t.has_functions())unsupported(at,"nested function-bearing aggregate or polymorphic value");
  return t;
 }
 ExprPtr node(K k,Type t,const Location& at,std::string text={}){
  tick(at);auto e=std::make_unique<Expr>();e->kind=k;e->type=t;e->where=at;e->text=std::move(text);return e;
 }
 ExprPtr var(std::string name,Type t,const Location& at,R ref=R::Local){
  auto e=node(K::Variable,t,at,std::move(name));e->reference=ref;return e;
 }
 ExprPtr integer(std::size_t n,const Location& at){return node(K::Integer,Type::I64,at,std::to_string(n));}
 ExprPtr zero(const Type& t,const Location& at){
  if(t==Type::Bool)return node(K::Boolean,t,at,"false");
  if(t.is_float())return node(K::Float,t,at,t==Type::F32?"0.0f32":"0.0");
  if(t.is_numeric())return node(K::Integer,t,at,t==Type::I64?"0":"0"+type_name(t));
  if(t.is_tuple()){auto e=node(K::Tuple,t,at);for(const auto& p:t.elements())e->children.push_back(zero(p,at));return e;}
  unsupported(at,"heterogeneous closure alternatives require a defaultable capture layout");
 }
 ExprPtr call(std::string name,std::vector<ExprPtr> args,Type result,const Location& at){
  std::vector<Type> types;for(const auto& arg:args)types.push_back(arg->type);
  auto e=node(K::Call,result,at);e->callee=var(std::move(name),Type::function(std::move(types),result),at,R::Function);
  e->children=std::move(args);return e;
 }
 void layout(){
  for(auto& [t,g]:groups){
   // A field of the same type is shared between alternatives, never between
   // distinct captures of the same lambda. No uninitialized union storage.
   for(auto& a:g.choices){
    std::set<std::size_t> used;
    for(const auto& c:a.captures){
     std::size_t s=0;while(s<g.fields.size()&&(g.fields[s]!=c.type||used.contains(s)))++s;
     if(s==g.fields.size()){if(s>=256)unsupported(g.at,"capture layout limit");g.fields.push_back(c.type);}
     a.slots.push_back(s);used.insert(s);
    }
   }
   std::vector<Type> tuple{Type::I64};tuple.insert(tuple.end(),g.fields.begin(),g.fields.end());
   g.rep=tuple.size()==1?Type{Type::I64}:Type::tuple(std::move(tuple));g.dispatcher=fresh();
  }
 }
 ExprPtr pack(const Expr& e){
  const auto& [t,tag]=values.at(&e);const auto& g=groups.at(t);const auto& a=g.choices[tag];
  if(g.fields.empty())return integer(tag,e.where);
  auto p=node(K::Tuple,g.rep,e.where);p->children.push_back(integer(tag,e.where));
  std::map<std::size_t,const Capture*> bound;
  for(std::size_t i=0;i<a.slots.size();++i)bound[a.slots[i]]=&a.captures[i];
  for(std::size_t i=0;i<g.fields.size();++i){
   auto c=bound.find(i);p->children.push_back(c==bound.end()?zero(g.fields[i],e.where):var(c->second->name,c->second->type,e.where));
  }
  return p;
 }
 void lift(Expr& e){
  tick(e.where);on1x::Function f;f.name=lambdas.at(&e);f.where=e.where;f.result=rep(e.result,e.where);
  for(const auto& c:e.captures){Param p;p.name=c.name;p.type=c.type;p.where=e.where;f.params.push_back(std::move(p));}
  for(auto p:e.parameters){p.type=rep(p.type,p.where);f.params.push_back(std::move(p));}
  f.body=std::move(e.body);rewrite(f.body);lifted.push_back(std::move(f));
 }
 void rewrite(ExprPtr& owner){
  auto& e=*owner;tick(e.where);if(e.prototype)return;
  if(e.kind==K::Lambda){auto value=pack(e);lift(e);owner=std::move(value);return;}
  if(values.contains(&e)){owner=pack(e);return;}
  if(e.kind==K::Call){
   auto& c=*e.callee;auto type=c.type;
   if(c.kind==K::Lambda||!direct(c)){
    std::string name;std::vector<ExprPtr> args;
    if(c.kind==K::Lambda){
     name=lambdas.at(&c);
     for(const auto& cap:c.captures)args.push_back(var(cap.name,cap.type,c.where));
     lift(c);
    }else{
     auto hit=groups.find(type);if(hit==groups.end())unsupported(c.where,"unknown indirect target family");
     name=hit->second.dispatcher;rewrite(e.callee);args.push_back(std::move(e.callee));
    }
    for(auto& arg:e.children){rewrite(arg);args.push_back(std::move(arg));}
    owner=call(std::move(name),std::move(args),rep(e.type,e.where),e.where);return;
   }
   if(c.reference==R::Function){std::vector<Type> params;for(const auto& p:type.parameters())params.push_back(rep(p,c.where));
    c.type=Type::function(std::move(params),rep(type.result(),c.where));}
  }else if(e.callee)rewrite(e.callee);
  for(auto& arg:e.children)rewrite(arg);
  rewrite(e.body);
  e.type=rep(e.type,e.where);
 }
 void rewrite(std::vector<Stmt>& body){
  for(auto& s:body){if(s.prototype||s.kind==S::Template)continue;tick(s.where);
   s.declared=rep(s.declared,s.where);if(s.value)rewrite(s.value);rewrite(s.body);rewrite(s.otherwise);}
 }
 on1x::Function dispatch(const Type& signature,const Group& g){
  on1x::Function f;f.name=g.dispatcher;f.where=g.at;f.result=signature.result();
  auto param=[&](Type t){Param p;p.type=t;p.name=fresh();p.where=g.at;f.params.push_back(std::move(p));};
  param(g.rep);for(const auto& p:signature.parameters())param(p);
  auto field=[&](std::size_t n){
   auto v=var(f.params.front().name,g.rep,g.at);
   if(g.fields.empty())return v;
   auto out=node(K::Project,g.rep.elements().at(n),g.at);out->projection=n;out->children.push_back(std::move(v));return out;
  };
  for(std::size_t i=0;i<g.choices.size();++i){
   Stmt branch;branch.kind=S::If;branch.where=g.at;branch.value=node(K::Binary,Type::Bool,g.at,"==");
   branch.value->children.push_back(field(0));branch.value->children.push_back(integer(i,g.at));
   const auto& a=g.choices[i];std::vector<ExprPtr> args;
   for(auto slot:a.slots)args.push_back(field(slot+1));
   for(std::size_t p=1;p<f.params.size();++p)args.push_back(var(f.params[p].name,f.params[p].type,g.at));
   Stmt ret;ret.kind=S::Return;ret.where=g.at;
   auto fn=call(a.function,std::move(args),f.result,g.at);
   if(f.result==Type::Void){Stmt action;action.kind=S::Expr;action.where=g.at;action.value=std::move(fn);branch.body.push_back(std::move(action));}
   else ret.value=std::move(fn);
   branch.body.push_back(std::move(ret));f.body.push_back(std::move(branch));
  }
  Stmt invalid;invalid.kind=S::Trap;invalid.where=g.at;f.body.push_back(std::move(invalid));return f;
 }
public:
 explicit Conversion(std::size_t budget):left(budget){}
 std::optional<Unit> run(const Unit& input){
  for(const auto& f:input.functions){
   names.insert(f.name);if(f.prototype)continue;found|=f.result.is_function();
   for(const auto& p:f.params){reserve(p);found|=p.type.is_function();}
   reserve(f.body);
  }
  if(!found)return {};
  auto count=left;auto u=clone_unit(input,count);
  for(const auto& f:u.functions)if(!f.prototype){
   inspect(f.body);
  }
  layout();
  for(auto& f:u.functions)if(!f.prototype){
   f.result=rep(f.result,f.where);
   for(auto& p:f.params)p.type=rep(p.type,p.where);
   rewrite(f.body);
  }
  for(auto& f:lifted)u.functions.push_back(std::move(f));
  for(const auto& [type,g]:groups)u.functions.push_back(dispatch(type,g));
  return u;
 }
};
}
std::optional<Unit> vm_closures(const Unit& input,Limits limits){return Conversion(limits.nodes).run(input);}
}

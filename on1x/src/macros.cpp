#include <on1x/meta.hpp>
#include <on1x/clone.hpp>
#include "macro_rules.hpp"
#include <algorithm>
#include <set>
namespace on1x {
namespace {
using K=Expr::Kind;
using Key=std::pair<std::string,std::size_t>;
using Captures=meta::MacroCaptures;
class Expander {
 Unit& unit;meta::Schema schema=meta::base_schema();
 meta::MacroRules rules;
 std::map<std::string,const Macro*> macros;
 std::vector<std::set<Key>> scopes;
 std::set<std::string> used;
 std::size_t budget=262144,bytes=8*1024*1024,serial=0,expansions=0,active=0;
 [[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"macro: "+text);}
 void charge(const std::string& text,const Location& at){if(text.size()>bytes)fail(at,"expanded syntax byte limit exceeded");bytes-=text.size();}
 void account_pattern(const Pattern& p){charge(p.name,p.where);charge(p.where.file,p.where);for(const auto& c:p.elements)account_pattern(c);}
 void account_body(const std::vector<Stmt>& body){for(const auto& s:body){charge(s.name,s.where);charge(s.where.file,s.where);if(s.pattern)account_pattern(*s.pattern);if(s.value)account(*s.value);account_body(s.body);account_body(s.otherwise);}}
 void account(const Expr& e){charge(e.text,e.where);charge(e.where.file,e.where);for(const auto& p:e.parameters){charge(p.name,p.where);charge(p.where.file,p.where);if(p.pattern)account_pattern(*p.pattern);}if(e.callee)account(*e.callee);if(e.syntax_tail)account(*e.syntax_tail);for(const auto& c:e.children)account(*c);account_body(e.body);}
 ExprPtr copy(const Expr& e){auto out=clone_expression(e,budget);account(*out);return out;}
 std::vector<Stmt> copy(const std::vector<Stmt>& b){std::vector<Stmt> out;for(const auto& s:b)out.push_back(clone_statement(s,budget));account_body(out);return out;}
 bool bound(const Key& key)const{for(auto it=scopes.rbegin();it!=scopes.rend();++it)if(it->contains(key))return true;return false;}
 void bind(const std::string& name,std::size_t context){scopes.back().insert({name,context});}
 void bind(const Pattern& p){if(!p.is_tuple())bind(p.name,p.context);else for(const auto& c:p.elements)bind(c);}
 const MacroRule& match(const Expr& call,Captures& captures){
  auto found=macros.find(call.text);if(found==macros.end())fail(call.where,"undefined macro "+call.text);
  for(const auto& r:found->second->rules)if(rules.match(r,call,captures))return r;
  std::string actual;for(const auto& c:call.children){if(!actual.empty())actual+=", ";actual+=rules.category(*c);}
  fail(call.where,"no typed rule for "+call.text+"("+actual+")");
 }
 struct ExpansionGuard {Expander& owner;ExpansionGuard(Expander& e,const Location& at):owner(e){if(++e.active>64||++e.expansions>1024)e.fail(at,"expansion nesting/count limit exceeded");}~ExpansionGuard(){--owner.active;}};
 void expression(Expr& e,unsigned depth){
  if(depth>=max_syntax_depth)fail(e.where,"expanded syntax depth limit exceeded");
  if(e.kind==K::MacroCall){
   ExpansionGuard guard(*this,e.where);Captures captures;const auto& r=match(e,captures);
   if(r.block)fail(e.where,"block macro used in expression position");
   auto replacement=copy(*r.expression);rules.substitute(*replacement,captures,++serial);expression(*replacement,depth+1);e=std::move(*replacement);return;
  }
  if(e.kind==K::SyntaxHole)fail(e.where,"unexpanded template variable");
  if(e.callee)expression(*e.callee,depth+1);
  if(e.syntax_tail)expression(*e.syntax_tail,depth+1);
   for(auto& child:e.children)expression(*child,depth+1);
   if(e.kind==K::Tuple&&e.children.size()<2)fail(e.where,"expanded tuple requires at least two elements");
  if(e.kind==K::Lambda){scopes.emplace_back();for(const auto& p:e.parameters){if(p.pattern)bind(*p.pattern);else bind(p.name,p.context);}body(e.body,false,depth+1);scopes.pop_back();}
  else if(e.kind==K::SyntaxBlock)body(e.body,true,depth+1);
 }
 void body(std::vector<Stmt>& b,bool scope,unsigned depth){
  if(depth>=max_syntax_depth)fail(b.empty()?Location{}:b.front().where,"expanded syntax depth limit exceeded");
  if(scope)scopes.emplace_back();
  std::vector<Stmt> out;
  for(auto& s:b){
   if(s.kind==Stmt::Kind::Expr&&s.value->kind==K::MacroCall){
    Captures captured;const auto& r=match(*s.value,captured);
    if(r.block){ExpansionGuard guard(*this,s.where);auto expanded=copy(r.body);rules.substitute_body(expanded,captured,++serial);body(expanded,false,depth+1);for(auto& child:expanded)out.push_back(std::move(child));continue;}
   }
   if(s.value)expression(*s.value,depth+1);
   if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(*s.pattern);else bind(s.name,s.context);}
   body(s.body,true,depth+1);body(s.otherwise,true,depth+1);out.push_back(std::move(s));
  }b=std::move(out);if(scope)scopes.pop_back();
 }
 void resolve(){
  // Syntax contexts are marks, not textual gensyms: only identifiers from the
  // same expansion can bind each other. Caller syntax keeps its original mark.
  std::vector<std::map<Key,std::string>> names;
  auto remember=[&](const std::string& s){used.insert(s);};
  auto collect_expr=[&](auto&& self,const Expr& e)->void{remember(e.text);for(const auto& p:e.parameters)remember(p.name);if(e.callee)self(self,*e.callee);for(const auto& c:e.children)self(self,*c);auto collect=[&](auto&& visit,const std::vector<Stmt>& b)->void{for(const auto& s:b){remember(s.name);if(s.value)self(self,*s.value);visit(visit,s.body);visit(visit,s.otherwise);}};collect(collect,e.body);};
  for(const auto& f:unit.functions){remember(f.name);for(const auto& p:f.params)remember(p.name);Expr holder;holder.kind=K::SyntaxBlock;holder.body=copy(f.body);collect_expr(collect_expr,holder);}
  auto read=[&](std::string& n,std::size_t& context){for(auto it=names.rbegin();it!=names.rend();++it){auto hit=it->find({n,context});if(hit!=it->end()){n=hit->second;break;}}context=0;};
  auto define=[&](std::string& n,std::size_t& context,const Location& at){
   Key key{n,context};if(names.back().contains(key))throw Error(at,"duplicate local: "+n);
   std::string fresh;do{fresh="on1x_hygiene_"+std::to_string(++serial);}while(used.contains(fresh));used.insert(fresh);charge(fresh,at);names.back().emplace(std::move(key),fresh);n=std::move(fresh);context=0;
  };
  auto pattern=[&](auto&& self,Pattern& p)->void{if(p.is_tuple())for(auto& c:p.elements)self(self,c);else define(p.name,p.context,p.where);};
  auto expr=[&](auto&& self,Expr& e)->void{
   if(e.kind==K::Variable)read(e.text,e.context);
   if(e.callee)self(self,*e.callee);
   for(auto& c:e.children)self(self,*c);
   auto walk=[&](auto&& visit,std::vector<Stmt>& b,bool push)->void{if(push)names.emplace_back();for(auto& s:b){if(s.value)self(self,*s.value);if(s.kind==Stmt::Kind::Let){if(s.pattern)pattern(pattern,*s.pattern);else define(s.name,s.context,s.where);}else if(s.kind==Stmt::Kind::Assign)read(s.name,s.context);visit(visit,s.body,true);visit(visit,s.otherwise,true);}if(push)names.pop_back();};
   if(e.kind==K::Lambda){names.emplace_back();for(auto& p:e.parameters){if(p.pattern)pattern(pattern,*p.pattern);else define(p.name,p.context,p.where);}walk(walk,e.body,false);names.pop_back();}
   else if(e.kind==K::SyntaxBlock)walk(walk,e.body,true);
  };
  for(auto& f:unit.functions){names.emplace_back();for(auto& p:f.params){if(p.pattern)pattern(pattern,*p.pattern);else define(p.name,p.context,p.where);}
   auto walk=[&](auto&& self,std::vector<Stmt>& b,bool push)->void{if(push)names.emplace_back();for(auto& s:b){if(s.value)expr(expr,*s.value);if(s.kind==Stmt::Kind::Let){if(s.pattern)pattern(pattern,*s.pattern);else define(s.name,s.context,s.where);}else if(s.kind==Stmt::Kind::Assign)read(s.name,s.context);self(self,s.body,true);self(self,s.otherwise,true);}if(push)names.pop_back();};walk(walk,f.body,false);for(auto& a:f.attributes)expr(expr,*a);names.pop_back();
  }
 }
public:
 explicit Expander(Unit& u):unit(u),rules(schema,[this](const Expr& e){return bound({e.text,e.context});},[this](const Expr& e){return copy(e);},[this](const std::vector<Stmt>& b){return copy(b);}){}
 void run(){
  for(const auto& m:unit.macros){if(!macros.emplace(m.name,&m).second)fail(m.where,"duplicate macro "+m.name);for(const auto& r:m.rules)rules.validate(r);}
  for(auto& f:unit.functions){scopes.emplace_back();for(const auto& p:f.params){if(p.pattern)bind(*p.pattern);else bind(p.name,p.context);}body(f.body,false,0);for(auto& a:f.attributes)expression(*a,0);scopes.pop_back();}
  resolve();unit.macros.clear();
 }
};
}
void expand_macros(Unit& unit){if(unit.macros.empty())return;std::size_t budget=262144;auto working=clone_unit(unit,budget);Expander(working).run();unit=std::move(working);}
}

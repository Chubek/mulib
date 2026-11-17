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
using Values=std::map<Key,std::string>;
using Syntax=std::map<Key,std::size_t>;
class Expander {
 struct Definition {
  Macro macro;
  std::shared_ptr<const Values> values;
  std::shared_ptr<const Syntax> syntax;
 };
 struct Match {std::size_t definition;const MacroRule* rule;Captures captures;};
 Unit& unit;meta::Schema schema=meta::base_schema();
 meta::MacroRules rules;
 // Environments contain pool indices, not shared_ptr<Definition>. Recursive
 // syntax groups therefore do not create compiler-side reference cycles.
 std::vector<std::unique_ptr<Definition>> definitions;
 std::shared_ptr<const Syntax> syntax=std::make_shared<const Syntax>();
 std::vector<Values> scopes;
 std::map<std::size_t,std::size_t> marks;
 std::set<std::string> used,globals;
 std::size_t budget=262144,bytes=8*1024*1024,serial=0,expansions=0,active=0,matching=0;
 [[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"macro: "+text);}
 void charge(std::size_t size,const Location& at){if(size>bytes)fail(at,"expanded syntax byte limit exceeded");bytes-=size;}
 void charge(const std::string& text,const Location& at){charge(text.size(),at);}
 void account_pattern(const Pattern& p){charge(p.name,p.where);charge(p.where.file,p.where);for(const auto& label:p.labels)charge(label,p.where);for(const auto& c:p.elements)account_pattern(c);}
 void account_macro(const Macro& m){
  charge(m.name,m.where);charge(m.where.file,m.where);charge(m.ellipsis,m.where);for(const auto& literal:m.literals)charge(literal,m.where);
  auto pattern=[&](auto&& self,const MacroParameter& p)->void{charge(p.name,p.where);charge(p.category,p.where);charge(p.where.file,p.where);if(p.literal)account(*p.literal);for(const auto& c:p.elements)self(self,c);if(p.tail)self(self,*p.tail);};
  for(const auto& r:m.rules){charge(r.where.file,r.where);for(const auto& p:r.parameters)pattern(pattern,p);if(r.tail)pattern(pattern,*r.tail);if(r.expression)account(*r.expression);account_body(r.body);}
 }
 void account_body(const std::vector<Stmt>& body){for(const auto& s:body){charge(s.name,s.where);charge(s.label,s.where);charge(s.where.file,s.where);if(s.pattern)account_pattern(*s.pattern);if(s.value)account(*s.value);account_body(s.body);account_body(s.otherwise);}}
 void account(const Expr& e){
   charge(e.text,e.where);charge(e.where.file,e.where);
   for(const auto& p:e.patterns)account_pattern(p);
  for(const auto& p:e.parameters){charge(p.name,p.where);charge(p.where.file,p.where);if(p.pattern)account_pattern(*p.pattern);}
  for(const auto& m:e.syntax_definitions)account_macro(m);
  if(e.callee)account(*e.callee);
  if(e.syntax_tail)account(*e.syntax_tail);
  for(const auto& c:e.children)account(*c);
  account_body(e.body);
 }
 ExprPtr copy(const Expr& e){auto out=clone_expression(e,budget);account(*out);return out;}
 std::vector<Stmt> copy(const std::vector<Stmt>& b){std::vector<Stmt> out;for(const auto& s:b)out.push_back(clone_statement(s,budget));account_body(out);return out;}
 void remember(const std::string& name,const Location& at){if(used.insert(name).second)charge(name.size()+32,at);}
 void collect_pattern(const Pattern& p){remember(p.name,p.where);for(const auto& c:p.elements)collect_pattern(c);}
 void collect_body(const std::vector<Stmt>& b){for(const auto& s:b){remember(s.name,s.where);if(s.pattern)collect_pattern(*s.pattern);if(s.value)collect(*s.value);collect_body(s.body);collect_body(s.otherwise);}}
 void collect_macro(const Macro& m){
  remember(m.name,m.where);remember(m.ellipsis,m.where);for(const auto& n:m.literals)remember(n,m.where);
  auto pattern=[&](auto&& self,const MacroParameter& p)->void{if(p.literal)collect(*p.literal);for(const auto& c:p.elements)self(self,c);if(p.tail)self(self,*p.tail);};
  for(const auto& r:m.rules){for(const auto& p:r.parameters)pattern(pattern,p);if(r.tail)pattern(pattern,*r.tail);if(r.expression)collect(*r.expression);collect_body(r.body);}
 }
  void collect(const Expr& e){
   for(const auto& p:e.patterns)collect_pattern(p);
    if(e.kind==K::PatternMatch||e.kind==K::Loop)remember(e.text,e.where);
  if(e.kind==K::Variable||e.kind==K::MacroCall)remember(e.text,e.where);
  for(const auto& p:e.parameters){remember(p.name,p.where);if(p.pattern)collect_pattern(*p.pattern);}
  for(const auto& m:e.syntax_definitions)collect_macro(m);
  if(e.callee)collect(*e.callee);
  if(e.syntax_tail)collect(*e.syntax_tail);
  for(const auto& c:e.children)collect(*c);
  collect_body(e.body);
 }
 std::optional<std::string> marked_value(const std::string& name,std::size_t context)const{
  auto mark=marks.find(context);if(mark==marks.end())return {};
  const auto& values=*definitions.at(mark->second)->values;
  auto found=values.find({name,0});return found==values.end()?std::nullopt:std::optional<std::string>{found->second};
 }
 std::optional<std::string> value(const std::string& name,std::size_t context)const{
  for(auto scope=scopes.rbegin();scope!=scopes.rend();++scope){auto found=scope->find({name,context});if(found!=scope->end())return found->second;}
  return marked_value(name,context);
 }
 std::optional<std::string> value(const Expr& e,const Values& environment)const{
  auto found=environment.find({e.text,e.context});return found==environment.end()?marked_value(e.text,e.context):std::optional<std::string>{found->second};
 }
 std::optional<std::size_t> macro(const Expr& call,const Syntax& environment)const{
  if(auto found=environment.find({call.text,call.context});found!=environment.end())return found->second;
  auto mark=marks.find(call.context);if(mark==marks.end())return {};
  const auto& saved=*definitions.at(mark->second)->syntax;
  auto found=saved.find({call.text,0});return found==saved.end()?std::nullopt:std::optional<std::size_t>{found->second};
 }
 std::string identity(const Expr& e,const std::optional<std::string>& binding,const Syntax& environment)const{
  if(binding)return "value:"+*binding;
  if(!globals.contains(e.text)&&!is_intrinsic_name(e.text))if(auto definition=macro(e,environment))return "syntax:"+std::to_string(*definition);
  return "global:"+e.text;
 }
 bool same_binding(const Expr& pattern,const Expr& actual)const{
  const auto& definition=*definitions.at(matching);
  return identity(pattern,value(pattern,*definition.values),*definition.syntax)==identity(actual,value(actual.text,actual.context),*syntax);
 }
 void read(std::string& name,std::size_t& context){if(auto binding=value(name,context))name=*binding;context=0;}
 void bind(std::string& name,std::size_t& context,const Location& at){
  Key key{name,context};if(scopes.back().contains(key))throw Error(at,"duplicate local: "+name);
  std::string fresh;do{fresh="on1x_hygiene_"+std::to_string(++serial);}while(used.contains(fresh));remember(fresh,at);
  charge(name.size()+fresh.size()+64,at);scopes.back().emplace(std::move(key),fresh);name=std::move(fresh);context=0;
 }
 void bind(Pattern& p){
  pattern_bindings(p,[&](Pattern& name){bind(name.name,name.context,name.where);});
  auto mirror=[&](auto&& self,Pattern& part)->void{if(part.kind==Pattern::Kind::Or&&part.elements.size()==2){pattern_bindings(part.elements[1],[&](Pattern& name){read(name.name,name.context);});}for(auto& c:part.elements)self(self,c);};mirror(mirror,p);
 }
 std::shared_ptr<const Values> snapshot(const Location& at){
  auto out=std::make_shared<Values>();
  for(const auto& scope:scopes)for(const auto& [key,name]:scope){if(!out->contains(key))charge(key.first.size()+name.size()+64,at);(*out)[key]=name;}
  return out;
 }
 std::shared_ptr<const Syntax> group(std::vector<Macro>& macros,bool recursive,const Location& at){
  if(macros.empty())return syntax;
  std::set<std::string> names;for(const auto& m:macros)if(!names.insert(m.name).second)fail(m.where,"duplicate macro "+m.name);
  auto environment=std::make_shared<Syntax>(*syntax);
  for(const auto& [key,id]:*syntax){(void)id;charge(key.first.size()+64,at);}
  auto values=snapshot(at);auto first=definitions.size();
  for(auto& m:macros){
   if(definitions.size()>=4096)fail(m.where,"lexical macro definition limit exceeded");
   charge(m.name.size()+64,m.where);auto definition=std::make_unique<Definition>();definition->macro=std::move(m);definition->values=values;
   (*environment)[{definition->macro.name,0}]=definitions.size();definitions.push_back(std::move(definition));
  }
  for(std::size_t i=first;i<definitions.size();++i){auto& definition=*definitions[i];definition.syntax=recursive?environment:syntax;for(const auto& r:definition.macro.rules)rules.validate(r);}
  macros.clear();return environment;
 }
 Match match(const Expr& call){
  auto found=macro(call,*syntax);if(!found)fail(call.where,"undefined macro "+call.text);
  const auto& definition=*definitions.at(*found);matching=*found;
  for(const auto& r:definition.macro.rules){Captures captures;if(rules.match(r,call,captures))return {*found,&r,std::move(captures)};}
  std::string actual;for(const auto& c:call.children){if(!actual.empty())actual+=", ";actual+=rules.category(*c);}
  fail(call.where,"no typed rule for "+call.text+"("+actual+")");
 }
 std::size_t mark(std::size_t definition){auto identity=++serial;marks.emplace(identity,definition);return identity;}
 struct ExpansionGuard {Expander& owner;ExpansionGuard(Expander& e,const Location& at):owner(e){if(++e.active>64||++e.expansions>1024)e.fail(at,"expansion nesting/count limit exceeded");}~ExpansionGuard(){--owner.active;}};
 void call(Expr& e,Match selected,unsigned depth){
  ExpansionGuard guard(*this,e.where);const auto& rule=*selected.rule;
  if(rule.block)fail(e.where,"block macro used in expression position");
  auto replacement=copy(*rule.expression);rules.substitute(*replacement,selected.captures,mark(selected.definition));expression(*replacement,depth+1);e=std::move(*replacement);
 }
 void local(Expr& e,unsigned depth,bool block_position){
  if(e.children.size()!=1)fail(e.where,"invalid local syntax body");
  bool block=e.children[0]->kind==K::SyntaxBlock;
  if(block&&!block_position)fail(e.where,"block local syntax used in expression position");
  auto saved=syntax;syntax=group(e.syntax_definitions,e.text=="letrec_syntax",e.where);
  auto replacement=std::move(e.children[0]);expression(*replacement,depth+1);syntax=std::move(saved);e=std::move(*replacement);
 }
  void expression(Expr& e,unsigned depth){
  if(depth>=max_syntax_depth)fail(e.where,"expanded syntax depth limit exceeded");
  if(e.kind==K::MacroCall){call(e,match(e),depth);return;}
  if(e.kind==K::SyntaxLet){local(e,depth,false);return;}
  if(e.kind==K::SyntaxHole)fail(e.where,"unexpanded template variable");
   if(e.kind==K::Variable)read(e.text,e.context);
   if(e.kind==K::PatternMatch){expression(*e.children.at(0),depth+1);scopes.emplace_back();if(!e.text.empty())bind(e.text,e.context,e.where);for(std::size_t i=1;i<e.children.size();++i)expression(*e.children[i],depth+1);scopes.pop_back();return;}
   if(e.kind==K::Case){scopes.emplace_back();for(auto& p:e.patterns)bind(p);expression(*e.children.at(0),depth+1);scopes.emplace_back();body(e.body,false,depth+1);expression(*e.children.at(1),depth+1);scopes.pop_back();scopes.pop_back();return;}
  if(e.callee)expression(*e.callee,depth+1);
  if(e.syntax_tail)expression(*e.syntax_tail,depth+1);
  for(auto& child:e.children)expression(*child,depth+1);
  if(e.kind==K::Tuple&&e.children.size()<2)fail(e.where,"expanded tuple requires at least two elements");
  if(e.kind==K::Lambda){scopes.emplace_back();for(auto& p:e.parameters){if(p.pattern)bind(*p.pattern);else bind(p.name,p.context,p.where);}body(e.body,false,depth+1);scopes.pop_back();}
   else if(e.kind==K::SyntaxBlock||e.kind==K::Loop)body(e.body,true,depth+1);
 }
 void body(std::vector<Stmt>& b,bool scope,unsigned depth){
  if(depth>=max_syntax_depth)fail(b.empty()?Location{}:b.front().where,"expanded syntax depth limit exceeded");
  if(scope)scopes.emplace_back();
  std::vector<Stmt> out;
  for(auto& s:b){
   if(s.kind==Stmt::Kind::Expr&&s.value->kind==K::SyntaxLet&&s.value->children.at(0)->kind==K::SyntaxBlock){
    local(*s.value,depth+1,true);s.kind=Stmt::Kind::Block;s.body=std::move(s.value->body);s.value.reset();out.push_back(std::move(s));continue;
   }
   if(s.kind==Stmt::Kind::Expr&&s.value->kind==K::MacroCall){
    auto selected=match(*s.value);const auto& rule=*selected.rule;
    if(rule.block){ExpansionGuard guard(*this,s.where);auto expanded=copy(rule.body);rules.substitute_body(expanded,selected.captures,mark(selected.definition));body(expanded,false,depth+1);for(auto& child:expanded)out.push_back(std::move(child));continue;}
    call(*s.value,std::move(selected),depth+1);
   }else if(s.value)expression(*s.value,depth+1);
   if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(*s.pattern);else bind(s.name,s.context,s.where);}
    else if(s.kind==Stmt::Kind::Assign)read(s.name,s.context);
     if((s.kind==Stmt::Kind::Break||s.kind==Stmt::Kind::Continue)&&!s.value&&s.label.empty()){
     auto name=s.kind==Stmt::Kind::Break?"break":"continue";auto binding=value(name,s.label_context);
     if(binding||globals.contains(name)){
      auto e=std::make_unique<Expr>();e->kind=K::Variable;e->text=binding?*binding:name;e->where=s.where;
      s.kind=Stmt::Kind::Expr;s.value=std::move(e);s.label_context=0;
     }
    }
    if(s.kind==Stmt::Kind::Range){scopes.emplace_back();bind(s.name,s.context,s.where);body(s.body,true,depth+1);scopes.pop_back();}
    else body(s.body,true,depth+1);
    body(s.otherwise,true,depth+1);out.push_back(std::move(s));
  }
  b=std::move(out);if(scope)scopes.pop_back();
 }
public:
 explicit Expander(Unit& u):unit(u),rules(schema,[this](const Expr& a,const Expr& b){return same_binding(a,b);},[this](const Expr& e){return copy(e);},[this](const std::vector<Stmt>& b){return copy(b);}){}
 void run(){
  for(const auto& m:unit.macros)collect_macro(m);
  for(const auto& f:unit.functions){globals.insert(f.name);remember(f.name,f.where);for(const auto& p:f.params){remember(p.name,p.where);if(p.pattern)collect_pattern(*p.pattern);}collect_body(f.body);for(const auto& a:f.attributes)collect(*a);}
  syntax=group(unit.macros,true,unit.macros.empty()?Location{}:unit.macros.front().where);
  for(auto& f:unit.functions){scopes.emplace_back();for(auto& p:f.params){if(p.pattern)bind(*p.pattern);else bind(p.name,p.context,p.where);}body(f.body,false,0);for(auto& a:f.attributes)expression(*a,0);scopes.pop_back();}
 }
};
bool local_syntax(const Unit& unit){
 auto expression=[&](auto&& self,const Expr& e)->bool{
  if(e.kind==K::SyntaxLet||!e.syntax_definitions.empty())return true;
  if(e.callee&&self(self,*e.callee))return true;
  for(const auto& c:e.children)if(self(self,*c))return true;
  auto body=[&](auto&& visit,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if((s.value&&self(self,*s.value))||visit(visit,s.body)||visit(visit,s.otherwise))return true;return false;};return body(body,e.body);
 };
 for(const auto& f:unit.functions){auto body=[&](auto&& self,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if((s.value&&expression(expression,*s.value))||self(self,s.body)||self(self,s.otherwise))return true;return false;};if(body(body,f.body))return true;for(const auto& a:f.attributes)if(expression(expression,*a))return true;}
 return false;
}
}
bool needs_macro_expansion(const Unit& unit){return !unit.macros.empty()||local_syntax(unit);}
void expand_macros(Unit& unit){if(!needs_macro_expansion(unit))return;std::size_t budget=262144;auto working=clone_unit(unit,budget);Expander(working).run();unit=std::move(working);}
}

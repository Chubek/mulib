#include "macro_rules.hpp"
#include <algorithm>
#include <optional>
namespace on1x::meta {
namespace {
using K=Expr::Kind;using P=MacroParameter::Kind;
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"macro: "+message);}
struct Sequence {std::vector<const Expr*> items;const Expr* tail=nullptr;};
Sequence sequence(const Expr& e){
 Sequence out;const Expr* current=&e;
 for(std::size_t depth=0;current;++depth){
  if(depth>=256)fail(e.where,"syntax list depth limit exceeded");
  for(const auto& c:current->children)out.items.push_back(c.get());
  current=current->syntax_tail.get();
  if(current&&current->kind!=K::SyntaxList){out.tail=current;break;}
 }return out;
}
}
void MacroRules::tick(const Location& at,std::size_t amount){if(amount>4*1024*1024-work)fail(at,"pattern matching work limit exceeded");work+=amount;}
void MacroRules::charge(const Location& at,std::size_t amount){if(amount>metadata)fail(at,"capture metadata limit exceeded");metadata-=amount;}
MacroCaptures MacroRules::duplicate(const MacroCaptures& source,const Location& at){
 tick(at,source.owned.size());charge(at,source.owned.size()*sizeof(std::shared_ptr<Expr>));
 for(const auto& [name,v]:source.variables){
  charge(at,name.size()+64);tick(at);
  for(const auto& [path,value]:v.values){(void)value;charge(at,64+path.size()*sizeof(std::size_t));tick(at);}
  for(const auto& [path,width]:v.widths){(void)width;charge(at,64+path.size()*sizeof(std::size_t));tick(at);}
 }return source;
}
std::string MacroRules::category(const Expr& e)const{
 switch(e.kind){
 case K::Integer:return e.text.starts_with("0x")||e.text.starts_with("0X")?"HexLiteral":"DecimalLiteral";
 case K::String:return "StringLiteral";case K::Boolean:return "BooleanLiteral";case K::Variable:return "Identifier";
 case K::Unary:return "UnaryExpression";case K::Binary:return "BinaryExpression";case K::Call:return "CallExpression";
 case K::Tuple:return "TupleExpression";case K::Lambda:return "LambdaExpression";case K::SyntaxBlock:return "Block";
 case K::SyntaxList:return "ListSyntax";case K::SyntaxVector:return "VectorSyntax";case K::SyntaxKeyword:return "KeywordSyntax";
 case K::Extension:return "ExtensionExpression";case K::MacroCall:return "Expression";
 case K::Quote:case K::Splice:case K::Lift:case K::Run:return "StagedExpression";
 default:return "Syntax";
 }
}
void MacroRules::validate(const MacroRule& rule){
 MacroCaptures declarations_for_rule;
 auto declaration=[&](auto&& self,const MacroParameter& p,std::size_t rank)->void{
  tick(p.where);rank+=p.repeated?1:0;
  if(rank>256)fail(p.where,"pattern repetition depth limit exceeded");
  if(p.kind==P::Capture||(!p.category.empty()&&p.kind==P::Wildcard)){
   if(!schema.parents.contains(p.category)||(!schema.subtype(p.category,"Expression")&&p.category!="Syntax"&&p.category!="Block"&&p.category!="ListSyntax"&&p.category!="VectorSyntax"&&p.category!="KeywordSyntax"))fail(p.where,"unsupported syntax category "+p.category);
  }
  if(p.kind==P::Capture){
   charge(p.where,p.name.size()+64);
   if(!declarations_for_rule.variables.emplace(p.name,MacroVariable{&p,rank,{},{}}).second)fail(p.where,"duplicate pattern variable "+p.name);
  }
  for(const auto& c:p.elements)self(self,c,rank);
  if(p.tail){if(p.tail->repeated)fail(p.tail->where,"a dotted tail cannot itself repeat");self(self,*p.tail,rank);}
 };
 for(const auto& p:rule.parameters)declaration(declaration,p,0);
 if(rule.tail)declaration(declaration,*rule.tail,0);
 auto binding=[&](const std::string& name,std::size_t depth,const Location& at){
  if(!name.starts_with('$'))return;
  auto found=declarations_for_rule.variables.find(name);
  if(found==declarations_for_rule.variables.end()||found->second.declaration->category!="Identifier"||found->second.rank>depth)fail(at,"binding/name hole requires one Identifier at the current repetition depth: "+name);
 };
 auto pat=[&](auto&& self,const Pattern& p,std::size_t depth)->void{if(p.is_tuple())for(const auto& c:p.elements)self(self,c,depth);else binding(p.name,depth,p.where);};
 auto driver=[&](auto&& self,const Expr& e,std::size_t depth)->bool{
  if(e.kind==K::SyntaxHole){auto found=declarations_for_rule.variables.find(e.text);return found!=declarations_for_rule.variables.end()&&found->second.rank>depth;}
  if(e.callee&&self(self,*e.callee,depth))return true;
  if(e.syntax_tail&&self(self,*e.syntax_tail,depth))return true;
  for(const auto& c:e.children)if(self(self,*c,depth))return true;
  auto name_driver=[&](const std::string& name){auto found=declarations_for_rule.variables.find(name);return found!=declarations_for_rule.variables.end()&&found->second.rank>depth;};
  auto pattern_driver=[&](auto&& visit,const Pattern& p)->bool{if(name_driver(p.name))return true;for(const auto& c:p.elements)if(visit(visit,c))return true;return false;};
  for(const auto& p:e.parameters)if(name_driver(p.name)||(p.pattern&&pattern_driver(pattern_driver,*p.pattern)))return true;
  auto body=[&](auto&& visit,const std::vector<Stmt>& b)->bool{for(const auto& s:b){if(s.value&&self(self,*s.value,depth))return true;if(name_driver(s.name)||(s.pattern&&pattern_driver(pattern_driver,*s.pattern)))return true;if(visit(visit,s.body)||visit(visit,s.otherwise))return true;}return false;};
  return body(body,e.body);
 };
 auto expression=[&](auto&& self,const Expr& e,std::size_t depth,const std::string& expected,bool list_position)->void{
  tick(e.where);
  for(std::size_t i=0;i<e.syntax_repeat;++i)if(!driver(driver,e,depth+i))fail(e.where,"ellipsis requires a repeated pattern variable at each repetition depth");
  if(e.syntax_repeat&&!list_position)fail(e.where,"ellipsis is only valid in argument/tuple/statement lists");
  depth+=e.syntax_repeat;
  if(e.kind==K::SyntaxHole){
   auto found=declarations_for_rule.variables.find(e.text);if(found==declarations_for_rule.variables.end())fail(e.where,"unknown template variable "+e.text);
   if(found->second.rank>depth)fail(e.where,"repeated pattern variable requires ellipsis at depth "+std::to_string(found->second.rank)+": "+e.text);
   if(!schema.subtype(found->second.declaration->category,expected))fail(e.where,"template variable "+e.text+" has category "+found->second.declaration->category+", expected "+expected);
  }else if(!schema.subtype(category(e),expected))fail(e.where,"template construct has category "+category(e)+", expected "+expected);
  for(const auto& p:e.parameters){if(p.pattern)pat(pat,*p.pattern,depth);else binding(p.name,depth,p.where);}
  if(e.callee)self(self,*e.callee,depth,"Expression",false);
  bool list=e.kind==K::Tuple||e.kind==K::Call||e.kind==K::MacroCall||e.kind==K::Extension||e.kind==K::SyntaxList||e.kind==K::SyntaxVector;
  bool syntax=e.kind==K::MacroCall||e.kind==K::Extension||e.kind==K::SyntaxList||e.kind==K::SyntaxVector;
  for(const auto& c:e.children)self(self,*c,depth,syntax?"Syntax":"Expression",list);
  if(e.syntax_tail)self(self,*e.syntax_tail,depth,"Syntax",false);
  auto body=[&](auto&& visit,const std::vector<Stmt>& b)->void{for(const auto& s:b){binding(s.name,depth,s.where);if(s.pattern)pat(pat,*s.pattern,depth);if(s.value)self(self,*s.value,depth,s.kind==Stmt::Kind::Expr?"Syntax":"Expression",s.kind==Stmt::Kind::Expr);visit(visit,s.body);visit(visit,s.otherwise);}};body(body,e.body);
 };
 if(rule.expression)expression(expression,*rule.expression,0,"Expression",false);
 Expr holder;holder.kind=K::SyntaxBlock;holder.where=rule.where;holder.body=clone_body(rule.body);expression(expression,holder,0,"Syntax",false);
 declarations.emplace(&rule,std::move(declarations_for_rule));
}
bool MacroRules::literal(const Expr& pattern,const Expr& actual){
 auto number=[&](const Expr& e)->std::optional<std::string>{
  const Expr* p=&e;bool negative=false;
  if(p->kind==K::Unary&&p->text=="-"&&p->children.size()==1){negative=true;p=p->children[0].get();}
  if(p->kind!=K::Integer)return {};
  std::string digits=p->text;if(digits.starts_with('-')){negative=!negative;digits.erase(0,1);}
  bool hexadecimal=digits.starts_with("0x")||digits.starts_with("0X");if(hexadecimal)digits.erase(0,2);
  std::vector<unsigned char> decimal(1,0);
  for(char c:digits){tick(e.where);unsigned value=c>='0'&&c<='9'?static_cast<unsigned>(c-'0'):c>='a'&&c<='f'?static_cast<unsigned>(c-'a'+10):c>='A'&&c<='F'?static_cast<unsigned>(c-'A'+10):99;
   if(value>=(hexadecimal?16u:10u))return {};
   unsigned carry=value;for(auto& digit:decimal){tick(e.where);carry+=digit*(hexadecimal?16u:10u);digit=static_cast<unsigned char>(carry%10);carry/=10;}while(carry){decimal.push_back(static_cast<unsigned char>(carry%10));carry/=10;}
  }
  while(decimal.size()>1&&decimal.back()==0)decimal.pop_back();
  std::string result;
  if(negative&&(decimal.size()!=1||decimal[0]))result+='-';
  for(auto it=decimal.rbegin();it!=decimal.rend();++it)result+=static_cast<char>('0'+*it);
  return result;
 };
 if(auto a=number(pattern)){auto b=number(actual);return b&&*a==*b;}
 if(pattern.kind!=actual.kind)return false;
 if(pattern.kind==K::Variable)return pattern.text==actual.text&&!locally_bound(actual);
 if(pattern.kind==K::String)return literal_value(pattern.text)==literal_value(actual.text);
 return pattern.text==actual.text;
}
void MacroRules::widths(const MacroParameter& p,MacroCaptures& captures,const RepetitionPath& path,std::size_t count){
 tick(p.where);
 if(p.kind==P::Capture){auto& widths=captures.variables.at(p.name).widths;if(!widths.contains(path))charge(p.where,64+path.size()*sizeof(std::size_t));widths[path]=count;}
 for(const auto& c:p.elements)widths(c,captures,path,count);
 if(p.tail)widths(*p.tail,captures,path,count);
}
bool MacroRules::match_one(const MacroParameter& p,const Expr& e,MacroCaptures& captures,RepetitionPath& path){
 tick(p.where);
 if(p.kind==P::Wildcard)return p.category.empty()||schema.subtype(category(e),p.category);
 if(p.kind==P::Literal)return literal(*p.literal,e);
 if(p.kind==P::Capture){
  if(!schema.subtype(category(e),p.category))return false;
  charge(p.where,64+path.size()*sizeof(std::size_t));captures.variables.at(p.name).values[path]=&e;return true;
 }
 bool okay=p.kind==P::List?e.kind==K::SyntaxList:p.kind==P::Vector?e.kind==K::SyntaxVector:p.kind==P::Tuple?e.kind==K::Tuple:p.kind==P::Call?e.kind==K::Call:p.kind==P::Unary?e.kind==K::Unary:e.kind==K::Binary;
 if(!okay)return false;
 auto view=sequence(e);
 if(p.kind==P::Call)view.items.insert(view.items.begin(),e.callee.get());
 if(p.kind==P::Unary||p.kind==P::Binary){
  charge(e.where,sizeof(Expr)+e.text.size());
  auto op=std::make_shared<Expr>();op->kind=K::String;op->where=e.where;op->text=source_literal(e.text);view.items.insert(view.items.begin(),op.get());captures.owned.push_back(std::move(op));
 }
 return match_sequence(p.elements,p.tail.get(),view.items,view.tail,0,0,captures,path,p.where);
}
bool MacroRules::match_sequence(const std::vector<MacroParameter>& patterns,const MacroParameter* tail,const std::vector<const Expr*>& items,const Expr* dotted,std::size_t pattern_index,std::size_t index,MacroCaptures& captures,RepetitionPath& path,const Location& at){
 struct Guard {std::size_t& depth;explicit Guard(std::size_t& d,const Location& at):depth(d){if(depth>=256)fail(at,"pattern matching depth limit exceeded");++depth;}~Guard(){--depth;}} guard(active,at);
 tick(at);
 while(pattern_index<patterns.size()&&!patterns[pattern_index].repeated){
  if(index==items.size()||!match_one(patterns[pattern_index],*items[index],captures,path))return false;
  ++pattern_index;++index;tick(at);
 }
 if(pattern_index==patterns.size()){
  if(!tail)return index==items.size()&&!dotted;
  if(index==items.size()&&dotted)return match_one(*tail,*dotted,captures,path);
  auto rest=std::make_shared<Expr>();rest->kind=K::SyntaxList;rest->where=at;
  for(std::size_t i=index;i<items.size();++i)rest->children.push_back(clone(*items[i]));
  if(dotted)rest->syntax_tail=clone(*dotted);
  auto pointer=rest.get();captures.owned.push_back(std::move(rest));return match_one(*tail,*pointer,captures,path);
 }
 const auto& pattern=patterns[pattern_index];
 for(std::size_t n=items.size()-index+1;n>0;--n){
  auto count=n-1;auto candidate=duplicate(captures,at);widths(pattern,candidate,path,count);bool okay=true;
  for(std::size_t i=0;i<count;++i){path.push_back(i);okay=match_one(pattern,*items[index+i],candidate,path);path.pop_back();if(!okay)break;}
  if(okay&&match_sequence(patterns,tail,items,dotted,pattern_index+1,index+count,candidate,path,at)){captures=std::move(candidate);return true;}
 }return false;
}
bool MacroRules::match(const MacroRule& rule,const Expr& call,MacroCaptures& captures){
 captures=duplicate(declarations.at(&rule),call.where);auto input=sequence(call);RepetitionPath path;
 return match_sequence(rule.parameters,rule.tail.get(),input.items,input.tail,0,0,captures,path,call.where);
}
const Expr& MacroRules::value(const std::string& name,const MacroCaptures& captures,const RepetitionPath& path,const Location& at){
 const auto& variable=captures.variables.at(name);if(variable.rank>path.size())fail(at,"insufficient template ellipses for "+name);
 RepetitionPath key(path.begin(),path.begin()+variable.rank);auto found=variable.values.find(key);
 if(found==variable.values.end())fail(at,"missing repetition value for "+name);
 return *found->second;
}
std::size_t MacroRules::extent(const Expr& e,const MacroCaptures& captures,const RepetitionPath& path){
 std::optional<std::size_t> count;
 auto use=[&](const std::string& name,const Location& at){auto found=captures.variables.find(name);if(found==captures.variables.end()||found->second.rank<=path.size())return;auto width=found->second.widths.find(path);if(width==found->second.widths.end())fail(at,"missing repetition shape for "+name);if(count&&*count!=width->second)fail(at,"incompatible repetition lengths in template");count=width->second;};
 auto pattern=[&](auto&& self,const Pattern& p)->void{use(p.name,p.where);for(const auto& c:p.elements)self(self,c);};
 auto walk=[&](auto&& self,const Expr& n)->void{
  tick(n.where);if(n.kind==K::SyntaxHole)use(n.text,n.where);for(const auto& p:n.parameters){use(p.name,p.where);if(p.pattern)pattern(pattern,*p.pattern);}
  if(n.callee)self(self,*n.callee);
  if(n.syntax_tail)self(self,*n.syntax_tail);
  for(const auto& c:n.children)self(self,*c);
  auto body=[&](auto&& visit,const std::vector<Stmt>& b)->void{for(const auto& s:b){use(s.name,s.where);if(s.pattern)pattern(pattern,*s.pattern);if(s.value)self(self,*s.value);visit(visit,s.body);visit(visit,s.otherwise);}};body(body,n.body);
 };walk(walk,e);if(!count)fail(e.where,"template ellipsis has no repeated driver");return *count;
}
void MacroRules::name(std::string& n,std::size_t& context,const MacroCaptures& captures,const RepetitionPath& path,std::size_t mark,const Location& at){
 if(n.empty())return;
 if(n.starts_with('$')){const auto& captured=value(n,captures,path,at);if(captured.kind!=K::Variable)fail(at,"name position requires one Identifier");n=captured.text;context=captured.context;}else context=mark;
}
void MacroRules::pattern(Pattern& p,const MacroCaptures& captures,const RepetitionPath& path,std::size_t mark){if(p.is_tuple())for(auto& c:p.elements)pattern(c,captures,path,mark);else name(p.name,p.context,captures,path,mark,p.where);}
void MacroRules::element(const Expr& t,const MacroCaptures& captures,RepetitionPath& path,std::size_t mark,std::size_t repeat,std::vector<ExprPtr>& out){
 tick(t.where);if(!repeat){auto copy=clone(t);copy->syntax_repeat=0;substitute(*copy,captures,mark,path);out.push_back(std::move(copy));return;}
 auto n=extent(t,captures,path);for(std::size_t i=0;i<n;++i){path.push_back(i);element(t,captures,path,mark,repeat-1,out);path.pop_back();}
}
void MacroRules::substitute(Expr& e,const MacroCaptures& captures,std::size_t mark,RepetitionPath path){
 tick(e.where);
 if(e.kind==K::SyntaxHole){auto replacement=clone(value(e.text,captures,path,e.where));e=std::move(*replacement);return;}
 if(e.kind==K::Variable)name(e.text,e.context,captures,path,mark,e.where);
 for(auto& p:e.parameters){if(p.pattern)pattern(*p.pattern,captures,path,mark);else name(p.name,p.context,captures,path,mark,p.where);}
 if(e.callee)substitute(*e.callee,captures,mark,path);
 std::vector<ExprPtr> children;
 for(auto& c:e.children)element(*c,captures,path,mark,c->syntax_repeat,children);
 e.children=std::move(children);
 if(e.syntax_tail){
  substitute(*e.syntax_tail,captures,mark,path);
  for(unsigned depth=0;e.syntax_tail&&e.syntax_tail->kind==K::SyntaxList;++depth){if(depth>=256)fail(e.where,"syntax list depth limit exceeded");auto tail=std::move(e.syntax_tail);for(auto& c:tail->children)e.children.push_back(std::move(c));e.syntax_tail=std::move(tail->syntax_tail);}
  if(e.kind!=K::SyntaxList&&e.kind!=K::MacroCall&&e.syntax_tail)fail(e.where,"dotted template tail must lower to a proper syntax list");
 }
 substitute_body(e.body,captures,mark,path);
}
void MacroRules::substitute_body(std::vector<Stmt>& body,const MacroCaptures& captures,std::size_t mark,RepetitionPath path){
 std::vector<Stmt> out;
 for(auto& s:body){
  if(s.kind==Stmt::Kind::Expr){
   std::vector<ExprPtr> values;element(*s.value,captures,path,mark,s.value->syntax_repeat,values);
   for(auto& e:values){if(e->kind==K::SyntaxBlock){for(auto& child:e->body)out.push_back(std::move(child));}else{Stmt statement;statement.kind=Stmt::Kind::Expr;statement.where=s.where;statement.value=std::move(e);out.push_back(std::move(statement));}}continue;
  }
  name(s.name,s.context,captures,path,mark,s.where);if(s.pattern)pattern(*s.pattern,captures,path,mark);
  if(s.value)substitute(*s.value,captures,mark,path);
  substitute_body(s.body,captures,mark,path);substitute_body(s.otherwise,captures,mark,path);out.push_back(std::move(s));
 }body=std::move(out);
}
}

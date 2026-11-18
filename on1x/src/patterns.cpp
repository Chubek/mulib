#include <on1x/patterns.hpp>
#include <on1x/numeric.hpp>
#include <on1x/literals.hpp>
#include <on1x/meta.hpp>
#include <algorithm>
#include <bit>
#include <charconv>
#include <set>

namespace on1x::patterns {
namespace {
using P=Pattern::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-PATTERN: "+text,"ON1X-PATTERN");}
struct Budget {
 std::size_t work=262144,bytes=8*1024*1024;
 void spend(const Location& at,std::size_t amount){if(amount>work)throw Error(at,"pattern analysis budget exhausted","ON1X-PATTERN-LIMIT");work-=amount;}
 void tick(const Location& at,std::size_t depth=0){if(depth>256)throw Error(at,"pattern analysis depth exhausted","ON1X-PATTERN-LIMIT");spend(at,1);}
 void text(const Location& at,std::size_t size){if(size>bytes)throw Error(at,"pattern analysis byte budget exhausted","ON1X-PATTERN-LIMIT");bytes-=size;}
};
Type literal_type(const Pattern& p){return p.name=="true"||p.name=="false"?Type::Bool:p.name.starts_with('"')?Type::String:p.name.starts_with("#\\")?Type::Char:numeric_literal_type(p.name.starts_with('-')?std::string_view(p.name).substr(1):p.name);}
Bindings inspect(Pattern& p,const Type& type,Budget& budget,std::size_t depth,bool root){
 budget.tick(p.where,depth);p.type=type;Bindings out;
 if(p.elements.size()>256||p.labels.size()>256)fail(p.where,"pattern width exceeds 256");
 if(p.kind!=P::Record&&(!p.labels.empty()||p.open))fail(p.where,"record-only metadata on another pattern");
 if(p.kind!=P::Name&&p.kind!=P::As&&p.kind!=P::Literal&&p.kind!=P::Variant&&!p.name.empty())fail(p.where,"unexpected pattern name/literal");
 auto bind=[&](const std::string& name,const Type& t){if(name.empty()||!out.emplace(name,t).second)fail(p.where,"duplicate or empty pattern binder: "+name);};
 auto merge=[&](Bindings b){for(const auto& [name,t]:b)bind(name,t);};
 auto child=[&](std::size_t i,const Type& t){if(i>=p.elements.size())fail(p.where,"missing pattern child");merge(inspect(p.elements[i],t,budget,depth+1,false));};
 if(p.is_tuple()){
  if(p.elements.size()<2||!p.name.empty())fail(p.where,"tuple pattern needs at least two unnamed elements");
  if(!type.is_tuple()||p.elements.size()!=type.elements().size())fail(p.where,"tuple pattern arity/type mismatch");
  for(std::size_t i=0;i<p.elements.size();++i)child(i,type.elements()[i]);
  return out;
 }
 switch(p.kind){
 case P::Name:if(!p.elements.empty())fail(p.where,"invalid binder shape");bind(p.name,type);break;
 case P::Wildcard:if(!p.elements.empty()||!p.name.empty())fail(p.where,"invalid wildcard shape");break;
 case P::Rest:if(!root||!p.elements.empty())fail(p.where,"rest must be the complete final-arm pattern");break;
 case P::Literal:{
  if(!p.elements.empty()||literal_type(p)!=type)fail(p.where,"literal pattern type mismatch: expected "+type_name(type));
  auto value=literal(p);(void)value;break;
 }
 case P::Variant:{
  if(type.kind()!=Type::Sum)fail(p.where,"constructor pattern requires a closed sum");
  auto tag=std::find(type.labels().begin(),type.labels().end(),p.name);if(tag==type.labels().end())fail(p.where,"unknown constructor: "+p.name);
  auto payload=type.elements()[static_cast<std::size_t>(tag-type.labels().begin())];
  if(p.elements.empty()&&payload.kind()==Type::Record&&payload.elements().empty())break;
  if(p.elements.size()==1)child(0,payload);
  else if(payload.is_tuple()&&payload.elements().size()==p.elements.size()){for(std::size_t i=0;i<p.elements.size();++i)child(i,payload.elements()[i]);}
  else fail(p.where,"constructor payload arity mismatch");
  break;
 }
 case P::Array:
  if(type.kind()!=Type::Array&&type.kind()!=Type::Slice)fail(p.where,"exact array pattern requires an array or snapshot slice; lists are not implemented");
  if((type.kind()==Type::Array&&p.elements.size()!=type.extent())||p.elements.size()>type.extent())fail(p.where,"array pattern extent mismatch");
  for(std::size_t i=0;i<p.elements.size();++i)child(i,type.elements()[0]);
  break;
 case P::Record:{
  if(type.kind()!=Type::Record||p.labels.size()!=p.elements.size())fail(p.where,"record pattern shape/type mismatch");
  if(!p.open&&p.labels.size()!=type.labels().size())fail(p.where,"record pattern requires every field or '..'");
  std::set<std::string> seen;
  for(std::size_t i=0;i<p.labels.size();++i){auto f=std::find(type.labels().begin(),type.labels().end(),p.labels[i]);if(f==type.labels().end()||!seen.insert(p.labels[i]).second)fail(p.where,"unknown/duplicate record field: "+p.labels[i]);child(i,type.elements()[static_cast<std::size_t>(f-type.labels().begin())]);}break;
 }
 case P::As:if(p.elements.size()!=1)fail(p.where,"as-pattern requires one child");child(0,type);bind(p.name,type);break;
 case P::Or:{
  if(p.elements.size()!=2)fail(p.where,"or-pattern requires two alternatives");
  auto a=inspect(p.elements[0],type,budget,depth+1,false),b=inspect(p.elements[1],type,budget,depth+1,false);
  if(a!=b)fail(p.where,"or-pattern alternatives must bind the same names with identical types");
  out=std::move(a);break;
 }
 case P::Tuple:fail(p.where,"invalid tuple pattern");
 default:fail(p.where,"unknown structural pattern kind");
 }
 return out;
}
// Matrix rows share immutable constructor children; specializing a row must not
// repeatedly deep-copy the same expanded pattern tree.
struct Node {bool wild=true;std::string key;std::shared_ptr<const std::vector<Node>> args;};
using Row=std::vector<Node>;using Matrix=std::vector<Row>;
void copy_keys(const Row& row,Budget& budget,const Location& at){for(const auto& n:row)budget.text(at,n.key.size());}
struct Constructor {std::string key;std::vector<Type> args;};
std::vector<Constructor> signature(const Type& t,Budget& budget,const Location& at){
 if(t.is_tuple()||t.is_data())budget.spend(at,t.elements().size()+1);
 if(t==Type::Bool)return {{"b:false",{}},{"b:true",{}}};
 if(t.is_tuple())return {{"tuple",t.elements()}};
 if(t.kind()==Type::Record)return {{"record",t.elements()}};
 if(t.kind()==Type::Array){budget.spend(at,t.extent()+1);return {{"array:"+std::to_string(t.extent()),std::vector<Type>(t.extent(),t.elements()[0])}};}
 if(t.kind()==Type::Slice){std::vector<Constructor> out;for(std::size_t n=0;n<=t.extent();++n){budget.spend(at,n+1);out.push_back({"array:"+std::to_string(n),std::vector<Type>(n,t.elements()[0])});}return out;}
 if(t.kind()==Type::Sum){std::vector<Constructor> out;for(std::size_t i=0;i<t.labels().size();++i)out.push_back({"tag:"+t.labels()[i],{t.elements()[i]}});return out;}
 return {};
}
std::string key(const Pattern& p){
 auto t=literal_type(p);if(t==Type::Bool)return "b:"+p.name;
 if(t==Type::String)return "s:"+meta::literal_value(p.name);
 if(t==Type::Char)return "c:"+std::to_string(literals::character(p.name,p.where));
 auto e=literal(p);bool neg=e.kind==Expr::Kind::Unary;auto& n=neg?*e.children[0]:e;
 auto bits=t.is_float()?float_literal_bits(n.text,n.where):integer_literal_bits(n.text,n.where);
 if(t.is_float()){
  auto u=std::bit_cast<std::uint64_t>(bits);auto sign=t==Type::F32?UINT64_C(0x80000000):UINT64_C(0x8000000000000000);if(neg)u^=sign;if((u&~sign)==0)u=0;bits=std::bit_cast<std::int64_t>(u);
 }
 return type_name(t)+":"+std::to_string(bits);
}
std::vector<Node> expand(const Pattern& p,Budget& budget,std::size_t depth){
 budget.tick(p.where,depth);
 if(p.kind==P::Or){auto a=expand(p.elements[0],budget,depth+1),b=expand(p.elements[1],budget,depth+1);if(b.size()>4096-a.size())throw Error(p.where,"pattern alternative expansion budget exhausted","ON1X-PATTERN-LIMIT");budget.spend(p.where,b.size());a.insert(a.end(),std::make_move_iterator(b.begin()),std::make_move_iterator(b.end()));return a;}
 if(p.kind==P::As)return expand(p.elements[0],budget,depth+1);
 if((p.kind==P::Name&&!p.is_tuple())||p.kind==P::Wildcard||p.kind==P::Rest)return {Node{}};
 Node base;base.wild=false;Pattern grouped;
 std::vector<const Pattern*> fields;for(const auto& c:p.elements)fields.push_back(&c);
 if(p.is_tuple())base.key="tuple";
 else if(p.kind==P::Literal){base.key=key(p);budget.text(p.where,base.key.size());return {std::move(base)};}
 else if(p.kind==P::Variant){base.key="tag:"+p.name;if(fields.size()!=1){grouped.kind=fields.empty()?P::Record:P::Tuple;grouped.type=fields.empty()?Type::data(Type::Record,{},{}):Type{Type::Invalid};grouped.where=p.where;grouped.elements=p.elements;fields={&grouped};}}
 else if(p.kind==P::Array)base.key="array:"+std::to_string(fields.size());
 else if(p.kind==P::Record){base.key="record";fields.clear();grouped.kind=P::Wildcard;grouped.where=p.where;for(std::size_t i=0;i<p.type.labels().size();++i){auto f=std::find(p.labels.begin(),p.labels.end(),p.type.labels()[i]);fields.push_back(f!=p.labels.end()?&p.elements[static_cast<std::size_t>(f-p.labels.begin())]:&grouped);}}
 else fail(p.where,"unhandled pattern matrix form");
 std::vector<Node> out{base};
 for(const auto* field:fields){auto options=expand(*field,budget,depth+1);std::vector<Node> next;for(const auto& prefix:out)for(const auto& option:options){budget.tick(p.where,depth);budget.spend(p.where,prefix.args?prefix.args->size()+1:1);budget.text(p.where,prefix.key.size()+option.key.size());if(prefix.args)copy_keys(*prefix.args,budget,p.where);auto node=prefix;auto args=prefix.args?*prefix.args:std::vector<Node>{};args.push_back(option);node.args=std::make_shared<const std::vector<Node>>(std::move(args));next.push_back(std::move(node));if(next.size()>4096)throw Error(p.where,"pattern alternative expansion budget exhausted","ON1X-PATTERN-LIMIT");}out=std::move(next);}
 return out;
}
Matrix specialize(const Matrix& matrix,const Constructor& c,Budget& budget,const Location& at){Matrix out;for(const auto& row:matrix){budget.spend(at,row.size()+c.args.size()+1);copy_keys(row,budget,at);if(row.empty())continue;Row next;if(row[0].wild)next.resize(c.args.size());else if(row[0].key==c.key){if(row[0].args){copy_keys(*row[0].args,budget,at);next=*row[0].args;}}else continue;next.insert(next.end(),row.begin()+1,row.end());out.push_back(std::move(next));}return out;}
bool useful(const Matrix& matrix,const Row& query,const std::vector<Type>& types,Budget& budget,const Location& at,std::size_t depth=0){
 if(depth>520)throw Error(at,"pattern decision depth exhausted","ON1X-PATTERN-LIMIT");
 budget.tick(at);if(query.empty())return matrix.empty();budget.spend(at,query.size()+types.size());
 auto recurse=[&](const Constructor& c,const Row& args){budget.spend(at,args.size()+c.args.size()+query.size()+types.size());copy_keys(args,budget,at);copy_keys(query,budget,at);auto q=args;q.insert(q.end(),query.begin()+1,query.end());auto ts=c.args;ts.insert(ts.end(),types.begin()+1,types.end());return useful(specialize(matrix,c,budget,at),q,ts,budget,at,depth+1);};
 auto constructors=signature(types[0],budget,at);
 if(!query[0].wild){auto c=std::find_if(constructors.begin(),constructors.end(),[&](const auto& v){return v.key==query[0].key;});return recurse(c==constructors.end()?Constructor{query[0].key,{}}:*c,query[0].args?*query[0].args:Row{});}
 if(!constructors.empty()){for(const auto& c:constructors)if(recurse(c,Row(c.args.size())))return true;return false;}
 Matrix defaults;for(const auto& row:matrix){budget.spend(at,row.size()+1);copy_keys(row,budget,at);if(row[0].wild)defaults.emplace_back(row.begin()+1,row.end());}copy_keys(query,budget,at);return useful(defaults,Row(query.begin()+1,query.end()),std::vector<Type>(types.begin()+1,types.end()),budget,at,depth+1);
}
}
Expr literal(const Pattern& p){
 auto spelling=scan(p.name,p.where.file);std::size_t token=p.name.starts_with('-')?1:0;
 if(spelling.size()!=token+2||(token&&spelling[0].text!="-")||spelling[token].text!=(token?p.name.substr(1):p.name))fail(p.where,"pattern literal must be one complete literal token");
 Expr e;e.where=p.where;e.text=p.name;e.type=literal_type(p);
 if(e.type==Type::Bool)e.kind=Expr::Kind::Boolean;
 else if(e.type==Type::Char){e.kind=Expr::Kind::Character;(void)literals::character(e.text,e.where);}
 else if(e.type==Type::String){e.kind=Expr::Kind::String;for(std::size_t i=1;i+1<e.text.size();++i)if(e.text[i]=='\\'&&e.text[++i]=='0')fail(p.where,"NUL pattern strings are unsupported");(void)meta::literal_value(e.text);}
 else if(e.type.is_float()){
  e.kind=Expr::Kind::Float;if(e.text.starts_with('-')){auto child=std::make_unique<Expr>();child->kind=e.kind;child->where=e.where;child->text=e.text.substr(1);child->type=e.type;(void)float_literal_bits(child->text,p.where);e.kind=Expr::Kind::Unary;e.text="-";e.children.push_back(std::move(child));}else(void)float_literal_bits(e.text,p.where);
 }else{
  e.kind=Expr::Kind::Integer;
  if(e.text.starts_with("-0x")||e.text.starts_with("-0X")){std::uint64_t value=0;auto text=std::string_view(e.text).substr(3);auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,16);if(ec!=std::errc{}||end!=text.data()+text.size()||value>UINT64_C(0x8000000000000000))fail(p.where,"negative hexadecimal literal out of i64 range");e.text=std::to_string(std::bit_cast<std::int64_t>(UINT64_C(0)-value));}
  (void)integer_literal_bits(e.text,p.where);
 }
 return e;
}
Bindings check(Pattern& p,const Type& type,bool root){if(type==Type::Void)fail(p.where,"match scrutinee cannot be void");if(type==Type::Json||type==Type::JsonView)fail(p.where,"unique resources are not copyable pattern scrutinees");Budget b;return inspect(p,type,b,0,root);}
void require_binding(const Pattern& p){
 if(p.kind!=P::Name&&p.kind!=P::Tuple)fail(p.where,"refutable/wildcard patterns require match; let and parameters retain name/tuple semantics");
 for(const auto& c:p.elements)require_binding(c);
}
void coverage(const Expr& match){
 if(match.children.size()<2)fail(match.where,"empty match is not exhaustive");
 Budget budget;Matrix matrix;
 for(std::size_t i=1;i<match.children.size();++i){const auto& arm=*match.children[i];if(arm.patterns.size()!=1||arm.children.size()!=2)fail(arm.where,"malformed match arm");const auto& p=arm.patterns[0];
  if(p.kind==P::Rest&&i+1!=match.children.size())fail(p.where,"rest is allowed only in the final arm");
  if(arm.children[0]->kind==Expr::Kind::Boolean&&arm.children[0]->text=="false")fail(arm.where,"unreachable arm with false guard");
  auto alternatives=expand(p,budget,0);bool reachable=false;for(const auto& node:alternatives)reachable=useful(matrix,{node},{match.children[0]->type},budget,p.where)||reachable;
  if(!reachable)fail(p.where,"unreachable or redundant match arm");
  if(arm.children[0]->kind==Expr::Kind::Boolean&&arm.children[0]->text=="true")for(auto& node:alternatives)matrix.push_back({std::move(node)});
 }
 if(useful(matrix,{Node{}},{match.children[0]->type},budget,match.where))fail(match.where,"non-exhaustive match; guards do not prove coverage");
}
bool terminates(const Expr& e){
 if(e.noreturn)return true;
 if(e.kind!=Expr::Kind::PatternMatch||e.children.size()<2)return false;
 for(std::size_t i=1;i<e.children.size();++i){const auto& arm=*e.children[i];if(arm.children.size()!=2||(!terminates(arm.body)&&!terminates(*arm.children[1])))return false;}
 return true;
}
bool terminates(const std::vector<Stmt>& body){for(const auto& s:body){if(s.kind==Stmt::Kind::Return||s.kind==Stmt::Kind::Break||s.kind==Stmt::Kind::Continue||s.kind==Stmt::Kind::Leave||s.kind==Stmt::Kind::Trap)return true;if(s.kind==Stmt::Kind::Expr&&s.value&&terminates(*s.value))return true;if(s.kind==Stmt::Kind::Block&&terminates(s.body))return true;if(s.kind==Stmt::Kind::If&&!s.otherwise.empty()&&terminates(s.body)&&terminates(s.otherwise))return true;}return false;}
}

#include <on1x/nominal.hpp>
#include <algorithm>
#include <cctype>

namespace on1x {
namespace {
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-NOMINAL: "+message,"ON1X-NOMINAL");}
bool identifier(const std::string& name){
 if(name.empty()||!(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_'))return false;
 return std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}
bool primitive_typo(std::string_view value){
 auto distance_one=[&](std::string_view expected){
  if(value.size()+1<expected.size()||expected.size()+1<value.size())return false;
  std::size_t left=0,right=0,edits=0;
  while(left<value.size()&&right<expected.size()){
   if(value[left]==expected[right]){++left;++right;continue;}
   if(++edits>1)return false;
   if(value.size()>expected.size())++left;
   else if(expected.size()>value.size())++right;
   else {++left;++right;}
  }
  edits+=(left<value.size())+(right<expected.size());
  return edits==1;
 };
 for(auto type:{Type::I64,Type::F64,Type::F32,Type::U8,Type::U16,Type::U32,Type::U64,
                Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})
  if(distance_one(type_name(type)))return true;
 return false;
}
}
void NominalCatalog::tick(const Location& at,std::size_t depth) const{
 if(depth>32||!work)throw Error(at,"ON1X-NOMINAL: layout depth/work budget exhausted","ON1X-NOMINAL-LIMIT");
 --work;
}
const NominalDeclaration& NominalCatalog::find(const std::string& name,const Location& at) const{
 auto it=declarations.find(name);
 if(it==declarations.end()){
  if(primitive_typo(name))throw Error(at,"unknown type: "+name,"ON1X-PARSE-TYPE",{},name);
  fail(at,"unknown type: "+name);
 }
 return *it->second;
}
const NominalDeclaration& NominalCatalog::result(const std::string& name,const Location& at) const{
 const auto& d=find(name,at);
 if(d.kind!=Type::Sum||d.parameters.size()!=2||d.labels.size()!=2)fail(at,"propagation requires a nominal enum with exactly Ok(T) and Err(E)");
 auto ok=std::find(d.labels.begin(),d.labels.end(),"Ok"),err=std::find(d.labels.begin(),d.labels.end(),"Err");
 if(ok==d.labels.end()||err==d.labels.end()||d.fields[static_cast<std::size_t>(ok-d.labels.begin())]!=Type::bound(0)||d.fields[static_cast<std::size_t>(err-d.labels.begin())]!=Type::bound(1))fail(at,"propagation requires exactly Ok(T) and Err(E) payloads");
 return d;
}
NominalCatalog::NominalCatalog(const Unit& unit){
 if(unit.nominals.size()>4096)throw Error(unit.nominals.front().where,"ON1X-NOMINAL: loaded declaration budget exhausted","ON1X-NOMINAL-LIMIT");
 for(const auto& d:unit.nominals){
  if(!identifier(d.name)||!declarations.emplace(d.name,&d).second)fail(d.where,"invalid or duplicate nominal declaration: "+d.name);
  static const std::set<std::string> reserved={"i64","f64","f32","u8","u16","u32","u64","bool","string","void","json","json_view","sexpr","buffer","record","sum","array","slice","code","forall","fn","_"};
  if(reserved.contains(d.name))fail(d.where,"built-in type name cannot be redeclared: "+d.name);
  if((d.kind!=Type::Record&&d.kind!=Type::Sum)||d.fields.size()!=d.labels.size()||d.fields.size()>256||(d.kind==Type::Sum&&d.fields.empty())||d.parameters.size()>32)fail(d.where,"invalid declaration shape");
  std::set<std::string> params,labels;
  for(const auto& p:d.parameters)if(!identifier(p)||reserved.contains(p)||!params.insert(p).second)fail(d.where,"invalid or duplicate nominal parameter: "+p);
  for(const auto& l:d.labels)if(!identifier(l)||!labels.insert(l).second)fail(d.where,"invalid or duplicate nominal label: "+l);
 }
 // Validate the declaration dependency graph before any layout instantiation,
 // including unused declarations and phantom arguments. No cycles are admitted.
 std::map<std::string,unsigned> states;
 auto declaration=[&](auto&& self,const NominalDeclaration& d,std::size_t depth)->void{
  tick(d.where,depth);auto& state=states[d.name];if(state==2)return;if(state==1)fail(d.where,"recursive nominal types require owning indirection (not implemented): "+d.name);state=1;
  auto field=[&](auto&& walk,const Type& t,std::size_t level)->void{
   tick(d.where,level);
   if(t.kind()==Type::Bound){if(t.generic_identity()>=d.parameters.size())fail(d.where,"unbound nominal type parameter");return;}
   if(t.kind()==Type::Named){const auto& target=find(t.nominal(),d.where);if(t.arguments().size()!=target.parameters.size())fail(d.where,"wrong type argument arity for "+t.nominal());for(const auto& a:t.arguments())walk(walk,a,level+1);self(self,target,level+1);return;}
   if(t.is_numeric()||t==Type::Bool)return;
   if(t.is_tuple()||t.is_data()){if(!t.nominal().empty())fail(d.where,"source declaration contains a checked layout");for(const auto& c:t.elements())walk(walk,c,level+1);return;}
   fail(d.where,"nominal fields require trivially copied types; unsupported "+type_name(t));
  };
  for(const auto& t:d.fields)field(field,t,depth+1);
  state=2;
 };
 for(const auto& [name,d]:declarations){(void)name;declaration(declaration,*d,0);}
 for(const auto& [name,d]:declarations){std::vector<Type> args;for(std::size_t i=0;i<d->parameters.size();++i)args.push_back(Type::generic(i));(void)layout(instantiate(name,args,d->where),d->where);}
}
std::pair<std::size_t,std::size_t> NominalCatalog::layout(const Type& t,const Location& at,std::size_t depth) const{
 tick(at,depth);
 if(auto hit=layouts.find(t);hit!=layouts.end()){
  if(hit->second.second>32-depth)throw Error(at,"ON1X-NOMINAL: expanded layout depth budget exhausted","ON1X-NOMINAL-LIMIT");
  return hit->second;
 }
 if(t.is_numeric()||t==Type::Bool||t.kind()==Type::Generic||t.kind()==Type::Bound)return {1,0};
 if(!t.is_tuple()&&!t.is_data())fail(at,"nominal fields/arguments require trivially copied types; unsupported "+type_name(t));
 std::size_t size=1,height=0;
 if(t.kind()==Type::Array||t.kind()==Type::Slice){
  auto count=std::max<std::size_t>(1,t.extent());auto child=layout(t.elements()[0],at,depth+1);
  if(child.first>4096/count)fail(at,"expanded nominal value exceeds 4096 storage-unit limit");
  size+=child.first*count+(t.kind()==Type::Slice?2:0);height=child.second+1;
 }else for(const auto& c:t.elements()){
  auto child=layout(c,at,depth+1);if(child.first>4096-size)fail(at,"expanded nominal value exceeds 4096 storage-unit limit");size+=child.first;height=std::max(height,child.second+1);
 }
 if(size>4096)fail(at,"expanded nominal value exceeds 4096 storage-unit limit");
 auto result=std::pair{size,height};layouts.emplace(t,result);return result;
}
Type NominalCatalog::expand(const Type& type,const std::vector<Type>& variables,const Location& at,std::size_t depth) const{
 tick(at,depth);
 if(type.kind()==Type::Bound){if(type.generic_identity()>=variables.size())fail(at,"unbound declaration parameter");return variables[type.generic_identity()];}
 if(type.kind()==Type::Named){
  const auto& d=find(type.nominal(),at);if(type.arguments().size()!=d.parameters.size())fail(at,"wrong type argument arity for "+d.name);
  std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(expand(a,variables,at,depth+1));
  auto key=Type::named(d.name,args);if(auto hit=expanded.find(key);hit!=expanded.end())return hit->second;
  std::vector<Type> fields;for(const auto& f:d.fields)fields.push_back(expand(f,args,at,depth+1));
  auto result=Type::data(d.kind,std::move(fields),d.labels).with_nominal(d.name,std::move(args));expanded.emplace(std::move(key),result);return result;
 }
 if(type.is_tuple()||type.is_data()){
  std::vector<Type> fields;for(const auto& f:type.elements())fields.push_back(expand(f,variables,at,depth+1));
  auto result=type.is_tuple()?Type::tuple(std::move(fields)):Type::data(type.kind(),std::move(fields),type.labels(),type.extent());
  if(!type.nominal().empty()){std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(expand(a,variables,at,depth+1));result=result.with_nominal(type.nominal(),std::move(args));}
  return result;
 }
 return type;
}
Type NominalCatalog::instantiate(const std::string& name,const std::vector<Type>& arguments,const Location& at) const{
 const auto& d=find(name,at);if(arguments.size()!=d.parameters.size())fail(at,"wrong type argument arity for "+name);
 auto key=Type::named(name,arguments);if(auto hit=expanded.find(key);hit!=expanded.end())return hit->second;
 std::vector<Type> fields;for(const auto& f:d.fields)fields.push_back(expand(f,arguments,at,1));
 auto result=Type::data(d.kind,std::move(fields),d.labels).with_nominal(name,arguments);expanded.emplace(std::move(key),result);return result;
}
Type NominalCatalog::resolve(const Type& type,const Location& at) const{
 tick(at,0);
 if(type.kind()==Type::Named){std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(resolve(a,at));return instantiate(type.nominal(),args,at);}
 if(type.is_forall())return Type::for_all(type.generic_identity(),resolve(type.body(),at));
 if(type.is_code())return Type::code(resolve(type.code_result(),at));
 if(type.is_function()){std::vector<Type> args;for(const auto& p:type.parameters())args.push_back(resolve(p,at));return Type::function(std::move(args),resolve(type.result(),at));}
 if(type.is_tuple()||type.is_data()){
  std::vector<Type> fields;for(const auto& f:type.elements())fields.push_back(resolve(f,at));
  auto result=type.is_tuple()?Type::tuple(std::move(fields)):Type::data(type.kind(),std::move(fields),type.labels(),type.extent());
  if(!type.nominal().empty()){std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(resolve(a,at));result=result.with_nominal(type.nominal(),std::move(args));}return result;
 }
 return type;
}
void NominalCatalog::verify(const Type& type,const Location& at) const{
 tick(at,0);
 if(type.kind()==Type::Named){
  if(primitive_typo(type.nominal()))
   throw Error(at,"unknown type: "+type.nominal(),"ON1X-PARSE-TYPE",{},type.nominal());
  fail(at,"unresolved named type in checked HIR: "+type.nominal());
 }
 if(type.nominal().empty()||verified.contains(type))return;
 if(type!=instantiate(type.nominal(),type.arguments(),at))fail(at,"checked nominal representation differs from declaration: "+type.nominal());
 (void)layout(type,at);
 auto argument=[&](auto&& self,const Type& t,std::size_t depth)->void{
  tick(at,depth);
  if(t.is_numeric()||t==Type::Bool||t.kind()==Type::Bound||t.kind()==Type::Generic)return;
  if(!t.is_tuple()&&!t.is_data())fail(at,"nominal arguments require trivially copied types; unsupported "+type_name(t));
  for(const auto& a:t.arguments())self(self,a,depth+1);
  for(const auto& f:t.elements())self(self,f,depth+1);
 };
 for(const auto& a:type.arguments()){argument(argument,a,0);(void)layout(a,at);}
 verified.insert(type);
}
void verify_nominals(const Unit& unit){
 NominalCatalog catalog(unit);std::set<Type> seen;std::size_t budget=262144;
 auto tick=[&](const Location& at,std::size_t depth){if(depth>520||!budget)throw Error(at,"ON1X-NOMINAL: HIR verification budget exhausted","ON1X-NOMINAL-LIMIT");--budget;};
 auto type=[&](auto&& self,const Type& t,const Location& at,std::size_t depth)->void{
  if(!seen.insert(t).second)return;
  tick(at,depth);catalog.verify(t,at);
  for(const auto& a:t.arguments())self(self,a,at,depth+1);
  if(t.is_function()){for(const auto& a:t.parameters())self(self,a,at,depth+1);self(self,t.result(),at,depth+1);}
  else if(t.is_forall())self(self,t.body(),at,depth+1);
  else if(t.is_code())self(self,t.code_result(),at,depth+1);
  else if(t.is_tuple()||t.is_data())for(const auto& f:t.elements())self(self,f,at,depth+1);
 };
 auto pattern=[&](auto&& self,const Pattern& p,std::size_t depth)->void{tick(p.where,depth);type(type,p.type,p.where,0);for(const auto& c:p.elements)self(self,c,depth+1);};
 auto expression=[&](auto&& self,const Expr& e,std::size_t depth)->void{
  tick(e.where,depth);type(type,e.type,e.where,0);type(type,e.result,e.where,0);
  for(const auto& p:e.parameters){type(type,p.type,p.where,0);if(p.pattern)pattern(pattern,*p.pattern,0);}
  for(const auto& c:e.captures)type(type,c.type,e.where,0);
  for(const auto& p:e.patterns)pattern(pattern,p,0);
  if(e.callee)self(self,*e.callee,depth+1);
  for(const auto& c:e.children){if(!c)fail(e.where,"null expression child in checked HIR");self(self,*c,depth+1);}
  for(const auto& p:e.proofs){if(!p)fail(e.where,"null proof child in checked HIR");self(self,*p,depth+1);}
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements,std::size_t level)->void{for(const auto& s:statements){tick(s.where,level);type(type,s.declared,s.where,0);if(s.pattern)pattern(pattern,*s.pattern,0);if(s.value)self(self,*s.value,level+1);walk(walk,s.body,level+1);walk(walk,s.otherwise,level+1);}};body(body,e.body,depth+1);
 };
 for(const auto& f:unit.functions){type(type,f.result,f.where,0);for(const auto& p:f.params){type(type,p.type,p.where,0);if(p.pattern)pattern(pattern,*p.pattern,0);}
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements,std::size_t depth)->void{for(const auto& s:statements){tick(s.where,depth);type(type,s.declared,s.where,0);if(s.pattern)pattern(pattern,*s.pattern,0);if(s.value)expression(expression,*s.value,depth+1);walk(walk,s.body,depth+1);walk(walk,s.otherwise,depth+1);}};body(body,f.body,0);
 }
 for(const auto& [schema,instances]:unit.forall_layouts){type(type,schema,{"<forall-layout>"},0);for(const auto& instance:instances)type(type,instance,{"<forall-layout>"},0);}
}
}

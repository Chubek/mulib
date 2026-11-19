#include <on1x/nominal.hpp>
#include <algorithm>
#include <cctype>
#include <tuple>

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
                Type::Bool,Type::String,Type::Char,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})
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
Type NominalCatalog::regex_builtin(const Type& type) const{
 // The loader has already rewritten bound aliases. Do not shadow an
 // existing local nominal type, including a declaration in legacy mode.
 if(type.kind()!=Type::Named||!type.arguments().empty()||declarations.contains(type.nominal()))return Type::Invalid;
 if(type.nominal()=="regex")return Type::Regex;
 if(type.nominal()=="captures")return Type::RegexCaptures;
 return Type::Invalid;
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
   static const std::set<std::string> reserved={"i64","f64","f32","u8","u16","u32","u64","bool","string","void","json","json_view","sexpr","buffer","record","sum","array","slice","code","forall","fn","own","_"};
   if(reserved.contains(d.name))fail(d.where,"built-in type name cannot be redeclared: "+d.name);
   if((d.kind!=Type::Record&&d.kind!=Type::Sum)||d.fields.size()!=d.labels.size()||d.fields.size()>256||(d.kind==Type::Sum&&d.fields.empty())||d.parameters.size()>32)fail(d.where,"invalid declaration shape");
  std::set<std::string> params,labels;
   for(const auto& p:d.parameters)if(!identifier(p)||reserved.contains(p)||!params.insert(p).second)fail(d.where,"invalid or duplicate nominal parameter: "+p);
   for(const auto& l:d.labels)if(!identifier(l)||!labels.insert(l).second)fail(d.where,"invalid or duplicate nominal label: "+l);
    TypeFormOptions options;options.bound_variables=d.parameters.size();
    // Source declarations historically diagnose `_` through the nominal field
    // policy below. Keep that stable; checked HIR still rejects escaped holes.
    options.allow_holes=true;options.allow_named=true;options.allow_foralls=false;
   for(const auto& field:d.fields){
    auto form=check_type_form(field,options);
    if(form.status==TypeFormStatus::Limit)
     throw Error(d.where,"ON1X-NOMINAL: declaration type-form validation limit: "+
                 form.message,"ON1X-NOMINAL-LIMIT");
    if(!form)fail(d.where,"malformed declaration type: "+form.message);
   }
  }
  // Validate the declaration dependency graph before any layout instantiation,
  // including unused declarations and phantom arguments. Only own<Named> cuts
  // a by-value dependency cycle; its target and arguments are still validated.
  std::map<std::string,unsigned> states;
  auto declaration=[&](auto&& self,const NominalDeclaration& d,std::size_t depth)->void{
   tick(d.where,depth);auto& state=states[d.name];if(state==2)return;if(state==1)fail(d.where,"recursive nominal type has no owning indirection: "+d.name);state=1;
   auto reference=[&](auto&& walk,const Type& t,std::size_t level)->void{
    tick(d.where,level);
    if(t.kind()==Type::Bound){if(t.generic_identity()>=d.parameters.size())fail(d.where,"unbound nominal type parameter");return;}
    if(t.kind()==Type::Named){
     const auto& target=find(t.nominal(),d.where);
     if(t.arguments().size()!=target.parameters.size())fail(d.where,"wrong type argument arity for "+t.nominal());
     for(const auto& a:t.arguments())walk(walk,a,level+1);
     return;
    }
    if(t.is_own()){walk(walk,t.owned(),level+1);return;}
    if(t.is_numeric()||t==Type::Bool||t==Type::Char||t==Type::SExpr)return;
    if(t.is_tuple()||t.is_data()){
     if(!t.nominal().empty())fail(d.where,"source declaration contains a checked layout");
     for(const auto& c:t.elements())walk(walk,c,level+1);
     return;
    }
    fail(d.where,"unsupported nominal type argument "+type_name(t));
   };
   auto field=[&](auto&& walk,const Type& t,std::size_t level)->void{
    tick(d.where,level);
    if(t.kind()==Type::Bound){if(t.generic_identity()>=d.parameters.size())fail(d.where,"unbound nominal type parameter");return;}
    if(t.kind()==Type::Named){const auto& target=find(t.nominal(),d.where);if(t.arguments().size()!=target.parameters.size())fail(d.where,"wrong type argument arity for "+t.nominal());for(const auto& a:t.arguments())walk(walk,a,level+1);self(self,target,level+1);return;}
    if(t.is_own()){reference(reference,t.owned(),level+1);return;}
    if(t.is_numeric()||t==Type::Bool||t==Type::Char||t==Type::SExpr)return;
    if(t.is_tuple()||t.is_data()){if(!t.nominal().empty())fail(d.where,"source declaration contains a checked layout");for(const auto& c:t.elements())walk(walk,c,level+1);return;}
     fail(d.where,"nominal fields require trivially copied value data, immutable shared sexpr, or own<Named>; unsupported "+type_name(t));
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
  if(t.is_numeric()||t==Type::Bool||t==Type::Char||t==Type::SExpr||t.is_own()||t.kind()==Type::Generic||t.kind()==Type::Bound)return {1,0};
 if(t.is_data()&&t.nominal().empty()&&t.has_functions()){
   fail(at,"structural record, sum, array and slice fields require trivially copied types");
 }
 if(!t.is_tuple()&&!t.is_data())
   fail(at,"nominal fields/arguments require trivially copied value data, immutable shared sexpr, or own<Named>; unsupported "+type_name(t));
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
  if(type.is_own()){
   const auto& target=type.owned();
   if(target.kind()!=Type::Named)fail(at,"own<T> target is not nominal");
   const auto& d=find(target.nominal(),at);
   if(target.arguments().size()!=d.parameters.size())fail(at,"wrong type argument arity for "+d.name);
   std::vector<Type> arguments;
   for(const auto& argument:target.arguments())arguments.push_back(expand(argument,variables,at,depth+1));
   return Type::own(Type::named(d.name,std::move(arguments)));
  }
  if(type.kind()==Type::Named){
  const auto& d=find(type.nominal(),at);if(type.arguments().size()!=d.parameters.size())fail(at,"wrong type argument arity for "+d.name);
  std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(expand(a,variables,at,depth+1));
  auto key=Type::named(d.name,args);if(auto hit=expanded.find(key);hit!=expanded.end())return hit->second;
  std::vector<Type> fields;for(const auto& f:d.fields)fields.push_back(expand(f,args,at,depth+1));
  auto result=Type::data(d.kind,std::move(fields),d.labels).with_nominal(d.name,std::move(args));expanded.emplace(std::move(key),result);return result;
  }
  if(type.kind()==Type::Channel)
   return Type::channel(expand(type.csp_payload(),variables,at,depth+1),
                        type.sender_cardinality(),type.receiver_cardinality());
  if(type.kind()==Type::Sender)
   return Type::sender(expand(type.csp_payload(),variables,at,depth+1));
  if(type.kind()==Type::Receiver)
   return Type::receiver(expand(type.csp_payload(),variables,at,depth+1));
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
  if(type.is_own()){
   const auto& target=type.owned();
   if(target.kind()!=Type::Named)fail(at,"own<T> target is not nominal");
   const auto& d=find(target.nominal(),at);
   if(target.arguments().size()!=d.parameters.size())fail(at,"wrong type argument arity for "+d.name);
   std::vector<Type> arguments;
   for(const auto& argument:target.arguments())arguments.push_back(resolve(argument,at));
   return Type::own(Type::named(d.name,std::move(arguments)));
  }
  if(type.kind()==Type::Named){
  if(auto builtin=regex_builtin(type);builtin!=Type::Invalid)return builtin;
  std::vector<Type> args;for(const auto& a:type.arguments())args.push_back(resolve(a,at));return instantiate(type.nominal(),args,at);
  }
  if(type.kind()==Type::Channel)
   return Type::channel(resolve(type.csp_payload(),at),
                        type.sender_cardinality(),type.receiver_cardinality());
  if(type.kind()==Type::Sender)return Type::sender(resolve(type.csp_payload(),at));
  if(type.kind()==Type::Receiver)return Type::receiver(resolve(type.csp_payload(),at));
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
  if(type.is_own()){
   const auto& target=type.owned();
   if(target.kind()!=Type::Named)fail(at,"own<T> target is not nominal");
   const auto& d=find(target.nominal(),at);
   if(target.arguments().size()!=d.parameters.size())fail(at,"wrong type argument arity for "+d.name);
   for(const auto& argument:target.arguments())verify(argument,at);
   (void)layout(instantiate(d.name,target.arguments(),at),at);
   return;
  }
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
   if(t.is_numeric()||t==Type::Bool||t==Type::Char||t==Type::SExpr||t.kind()==Type::Bound||t.kind()==Type::Generic)return;
   if(t.is_own()){verify(t,at);return;}
    if(!t.is_tuple()&&!t.is_data())fail(at,"nominal arguments require trivially copied value data, immutable shared sexpr, or own<Named>; unsupported "+type_name(t));
  for(const auto& a:t.arguments())self(self,a,depth+1);
  for(const auto& f:t.elements())self(self,f,depth+1);
 };
 for(const auto& a:type.arguments()){argument(argument,a,0);(void)layout(a,at);}
 verified.insert(type);
}
bool NominalCatalog::copy_safe(const Type& source,const Location& at) const{
 auto type=source.kind()==Type::Named?resolve(source,at):source;
 std::size_t budget=4096;
 auto visit=[&](auto&& self,const Type& t,std::size_t depth)->bool{
  if(depth>32||!budget)throw Error(at,"ON1X-IMPL-LIMIT: receiver ownership check budget exhausted","ON1X-IMPL-LIMIT");
  --budget;
  if(t.is_numeric()||t==Type::Bool||t==Type::Char||
     t.kind()==Type::Bound||t.kind()==Type::Generic)return true;
   if(t.is_own())return false;
  // Slices and all other handles/views carry provenance or ownership profiles
  // which this by-value receiver ABI does not model.
  if(t.kind()==Type::Slice||(!t.is_tuple()&&!t.is_data()))return false;
  for(const auto& argument:t.arguments())if(!self(self,argument,depth+1))return false;
  for(const auto& field:t.elements())if(!self(self,field,depth+1))return false;
  return true;
 };
 return visit(visit,type,0);
}
void verify_nominals(const Unit& unit){
 NominalCatalog catalog(unit);
 std::set<std::tuple<Type,bool,std::size_t>> seen;
 std::size_t budget=262144;
 auto tick=[&](const Location& at,std::size_t depth){if(depth>520||!budget)throw Error(at,"ON1X-NOMINAL: HIR verification budget exhausted","ON1X-NOMINAL-LIMIT");--budget;};
 auto type=[&](auto&& self,const Type& t,const Location& at,std::size_t depth,
               bool generics=false,std::size_t bound=0,bool optional=false)->void{
   if(optional&&t==Type::Invalid)return;
   if(!seen.emplace(t,generics,bound).second)return;
   tick(at,depth);
   TypeFormOptions options;options.allow_generics=generics;
   options.bound_variables=bound;
   options.max_nodes=std::min(options.max_nodes,budget+1);
   auto form=check_type_form(t,options);
   if(form.status==TypeFormStatus::Limit)
    throw Error(at,"ON1X-NOMINAL: checked type-form validation limit: "+
                form.message,"ON1X-NOMINAL-LIMIT");
   if(!form)fail(at,"malformed checked type: "+form.message);
   catalog.verify(t,at);
   if(t.is_own()){
    for(const auto& argument:t.owned().arguments())
     self(self,argument,at,depth+1,generics,bound);
    return;
   }
   if(t.is_data()&&t.nominal().empty()&&t.has_functions())
    fail(at,"managed structural aggregate is outside the nominal ownership profile");
   for(const auto& a:t.arguments())self(self,a,at,depth+1,generics,bound);
   if(t.is_function()){
    for(const auto& a:t.parameters())self(self,a,at,depth+1,generics,bound);
    self(self,t.result(),at,depth+1,generics,bound);
   }else if(t.is_forall())
    self(self,t.body(),at,depth+1,false,t.generic_identity());
   else if(t.is_code())self(self,t.code_result(),at,depth+1,generics,bound);
    else if(t.kind()==Type::Channel||t.kind()==Type::Sender||t.kind()==Type::Receiver)
     self(self,t.csp_payload(),at,depth+1,generics,bound);
    else if(t.is_tuple()||t.is_data())
     for(const auto& f:t.elements())self(self,f,at,depth+1,generics,bound);
  };
 auto pattern=[&](auto&& self,const Pattern& p,std::size_t depth,
                  bool generics)->void{
  tick(p.where,depth);type(type,p.type,p.where,0,generics,0,true);
  for(const auto& c:p.elements)self(self,c,depth+1,generics);
 };
  auto expression=[&](auto&& self,const Expr& e,std::size_t depth,
                       bool generics,bool callee_position=false)->void{
    tick(e.where,depth);
    if(e.prototype&&!generics)
     fail(e.where,"prototype marker outside a generic checked-HIR context");
    generics=generics||e.prototype;
    const bool intrinsic=callee_position&&e.kind==Expr::Kind::Variable&&
                         e.reference==Expr::Reference::Intrinsic;
    // Generic prototypes retain source match-arm carriers until a concrete
    // instance is lowered. A Case is not itself a value and intentionally has
    // no expression type.
    const bool arm=e.kind==Expr::Kind::Case;
    if(e.type==Type::Invalid&&!intrinsic&&!arm)
     fail(e.where,"invalid expression type in checked HIR");
    if(e.result==Type::Invalid)
     fail(e.where,"invalid expression result type in checked HIR");
    type(type,e.type,e.where,0,generics,0,intrinsic||arm);
  type(type,e.result,e.where,0,generics);
  for(const auto& p:e.parameters){
   type(type,p.type,p.where,0,generics);
   if(p.pattern)pattern(pattern,*p.pattern,0,generics);
  }
  for(const auto& c:e.captures)type(type,c.type,e.where,0,generics);
  for(const auto& p:e.patterns)pattern(pattern,p,0,generics);
   if(e.callee)self(self,*e.callee,depth+1,generics,true);
  for(const auto& c:e.children){
   if(!c)fail(e.where,"null expression child in checked HIR");
   self(self,*c,depth+1,generics);
  }
  for(const auto& p:e.proofs){
   if(!p)fail(e.where,"null proof child in checked HIR");
   self(self,*p,depth+1,true);
  }
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements,
                std::size_t level,bool body_generics)->void{
   for(const auto& s:statements){
    tick(s.where,level);const bool statement_generics=body_generics||s.prototype;
     type(type,s.declared,s.where,0,
          statement_generics||s.kind==Stmt::Kind::Template,0,true);
    if(s.pattern)pattern(pattern,*s.pattern,0,statement_generics);
    if(s.value)self(self,*s.value,level+1,
                    s.kind==Stmt::Kind::Template||statement_generics);
    for(const auto& instance:s.instances)
     if(!instance)fail(s.where,"null specialization instance in checked HIR");
    walk(walk,s.body,level+1,body_generics);
    walk(walk,s.otherwise,level+1,body_generics);
   }
  };
  body(body,e.body,depth+1,generics);
 };
 for(const auto& f:unit.functions){
  const bool generics=f.prototype;
  type(type,f.result,f.where,0,generics);
  for(const auto& p:f.params){
   type(type,p.type,p.where,0,generics);
   if(p.pattern)pattern(pattern,*p.pattern,0,generics);
  }
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements,
                std::size_t depth,bool body_generics)->void{
   for(const auto& s:statements){
    tick(s.where,depth);const bool statement_generics=body_generics||s.prototype;
     type(type,s.declared,s.where,0,
          statement_generics||s.kind==Stmt::Kind::Template,0,true);
    if(s.pattern)pattern(pattern,*s.pattern,0,statement_generics);
    if(s.value)expression(expression,*s.value,depth+1,
                          s.kind==Stmt::Kind::Template||statement_generics);
    for(const auto& instance:s.instances)
     if(!instance)fail(s.where,"null specialization instance in checked HIR");
    walk(walk,s.body,depth+1,body_generics);
    walk(walk,s.otherwise,depth+1,body_generics);
   }
  };
  body(body,f.body,0,generics);
 }
 for(const auto& [schema,instances]:unit.forall_layouts){
  type(type,schema,{"<forall-layout>"},0);
  for(const auto& instance:instances)
   type(type,instance,{"<forall-layout>"},0);
 }
}
}

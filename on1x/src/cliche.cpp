#include <on1x/cliche.hpp>
#include <on1x/clone.hpp>
#include <on1x/nominal.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace on1x::cliches {
namespace {

[[noreturn]] void fail(const Location& at,const std::string& message,
                       const std::string& id="ON1X-CLICHE"){
 throw Error(at,message,id);
}

bool identifier(const std::string& name){
 if(name.empty()||!(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_'))return false;
 return std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}

std::string constraint_name(const ClicheConstraint& constraint){
 std::string result=constraint.name+"<";
 for(std::size_t i=0;i<constraint.arguments.size();++i){
  if(i)result+=",";
  result+=type_name(constraint.arguments[i]);
 }
 return result+">";
}

std::string implementation_key(const ClicheImplementation& implementation){
 std::string result=implementation.name+"|";
 for(const auto& argument:implementation.arguments)result+=type_name(argument)+";";
 result+="|";
 std::vector<std::string> constraints;
 for(const auto& constraint:implementation.constraints)constraints.push_back(constraint_name(constraint));
 std::sort(constraints.begin(),constraints.end());
 for(const auto& constraint:constraints)result+=constraint+";";
 return result;
}

bool contains_bound(const Type& type){
 if(type.kind()==Type::Bound)return true;
 if(type.is_own())return contains_bound(type.owned());
 for(const auto& argument:type.arguments())if(contains_bound(argument))return true;
 if(type.is_function()){
  for(const auto& parameter:type.parameters())if(contains_bound(parameter))return true;
  return contains_bound(type.result());
 }
 if(type.is_forall())return contains_bound(type.body());
 if(type.is_code())return contains_bound(type.code_result());
 if(type.is_tuple()||type.is_data())
  for(const auto& element:type.elements())if(contains_bound(element))return true;
 return false;
}

std::size_t type_size(const Type& type,std::size_t depth=0){
 if(depth>256)return 1024*1024;
 std::size_t result=1;
 auto add=[&](const Type& child){
  auto size=type_size(child,depth+1);
  result=size>1024*1024-result?1024*1024:result+size;
 };
 if(type.is_own()){add(type.owned());return result;}
 for(const auto& argument:type.arguments())add(argument);
 if(type.is_function()){for(const auto& parameter:type.parameters())add(parameter);add(type.result());}
 else if(type.is_forall())add(type.body());
 else if(type.is_code())add(type.code_result());
 else if(type.is_tuple()||type.is_data())for(const auto& element:type.elements())add(element);
 return result;
}

void validate_type(const Type& type,std::size_t variables,const Location& at,
                   std::size_t& work,std::size_t depth=0){
 if(!work--)fail(at,"cliche type-validation work budget exhausted","ON1X-CLICHE-LIMIT");
 if(depth>256)fail(at,"cliche type exceeds 256-level limit","ON1X-CLICHE-LIMIT");
 if(type==Type::Invalid||type==Type::Hole||type.kind()==Type::Generic||type.is_forall())
  fail(at,"cliche signatures require explicit first-order types","ON1X-CLICHE-SIGNATURE");
 if(type.kind()==Type::Bound){
  if(type.generic_identity()>=variables)
   fail(at,"unbound cliche type parameter","ON1X-CLICHE-SIGNATURE");
  return;
 }
 if(type.is_own()){
  validate_type(type.owned(),variables,at,work,depth+1);
  return;
 }
 for(const auto& argument:type.arguments())validate_type(argument,variables,at,work,depth+1);
 if(type.is_function()){
  for(const auto& parameter:type.parameters())validate_type(parameter,variables,at,work,depth+1);
  validate_type(type.result(),variables,at,work,depth+1);
 }else if(type.is_code())validate_type(type.code_result(),variables,at,work,depth+1);
 else if(type.is_tuple()||type.is_data())
  for(const auto& element:type.elements())validate_type(element,variables,at,work,depth+1);
}

Type rebuild(const Type& source,std::vector<Type> children,std::vector<Type> arguments,
             Type result,const Location& at){
 (void)at;
 if(source.kind()==Type::Named)return Type::named(source.nominal(),std::move(arguments));
 if(source.is_function())return Type::function(std::move(children),std::move(result));
 if(source.is_code())return Type::code(std::move(result));
 if(source.is_tuple())return Type::tuple(std::move(children));
 if(source.is_data()){
  auto out=Type::data(source.kind(),std::move(children),source.labels(),source.extent());
  if(!source.nominal().empty())out=out.with_nominal(source.nominal(),std::move(arguments));
  return out;
 }
 return source;
}

bool same_constraints(std::vector<ClicheConstraint> left,std::vector<ClicheConstraint> right){
 auto order=[](const auto& a,const auto& b){return constraint_name(a)<constraint_name(b);};
 std::sort(left.begin(),left.end(),order);std::sort(right.begin(),right.end(),order);
 if(left.size()!=right.size())return false;
 for(std::size_t i=0;i<left.size();++i)
  if(left[i].name!=right[i].name||left[i].arguments!=right[i].arguments)return false;
 return true;
}

// Conservative head overlap. Returning true for a repeated-variable shape that
// later proves incompatible only rejects an implementation; it can never make
// resolution incoherent.
bool may_overlap(const Type& left,const Type& right){
 if(left.kind()==Type::Bound||right.kind()==Type::Bound)return true;
 if(left.is_own()||right.is_own())
  return left.is_own()&&right.is_own()&&may_overlap(left.owned(),right.owned());
 if(left.kind()==Type::Named||right.kind()==Type::Named){
  if(left.kind()==Type::Named&&right.kind()==Type::Named){
   if(left.nominal()!=right.nominal()||left.arguments().size()!=right.arguments().size())return false;
   for(std::size_t i=0;i<left.arguments().size();++i)
    if(!may_overlap(left.arguments()[i],right.arguments()[i]))return false;
   return true;
  }
  const auto& named=left.kind()==Type::Named?left:right;
  const auto& concrete=left.kind()==Type::Named?right:left;
  if(named.nominal()!=concrete.nominal()||named.arguments().size()!=concrete.arguments().size())return false;
  for(std::size_t i=0;i<named.arguments().size();++i)
   if(!may_overlap(named.arguments()[i],concrete.arguments()[i]))return false;
  return true;
 }
 if(left.kind()!=right.kind()||left.nominal()!=right.nominal()||
    left.arguments().size()!=right.arguments().size())return false;
 for(std::size_t i=0;i<left.arguments().size();++i)
  if(!may_overlap(left.arguments()[i],right.arguments()[i]))return false;
 if(left.is_function()){
  if(left.parameters().size()!=right.parameters().size())return false;
  for(std::size_t i=0;i<left.parameters().size();++i)
   if(!may_overlap(left.parameters()[i],right.parameters()[i]))return false;
  return may_overlap(left.result(),right.result());
 }
 if(left.is_code())return may_overlap(left.code_result(),right.code_result());
 if(left.is_tuple()||left.is_data()){
  if(left.elements().size()!=right.elements().size())return false;
  if(left.is_data()&&(left.labels()!=right.labels()||left.extent()!=right.extent()))return false;
  for(std::size_t i=0;i<left.elements().size();++i)
   if(!may_overlap(left.elements()[i],right.elements()[i]))return false;
 }
 return true;
}

bool match(const Type& pattern,const Type& actual,std::vector<Type>& substitutions){
 if(pattern.kind()==Type::Bound){
  auto index=pattern.generic_identity();
  if(index>=substitutions.size())return false;
  if(substitutions[index]==Type::Invalid){substitutions[index]=actual;return true;}
   return substitutions[index]==actual;
  }
  if(pattern.is_own())
   return actual.is_own()&&match(pattern.owned(),actual.owned(),substitutions);
  if(pattern.kind()==Type::Named){
  if(actual.nominal()!=pattern.nominal()||actual.arguments().size()!=pattern.arguments().size())return false;
  for(std::size_t i=0;i<pattern.arguments().size();++i)
   if(!match(pattern.arguments()[i],actual.arguments()[i],substitutions))return false;
  return true;
 }
 if(pattern.kind()!=actual.kind()||pattern.nominal()!=actual.nominal()||
    pattern.arguments().size()!=actual.arguments().size())return false;
 for(std::size_t i=0;i<pattern.arguments().size();++i)
  if(!match(pattern.arguments()[i],actual.arguments()[i],substitutions))return false;
 if(pattern.is_function()){
  if(pattern.parameters().size()!=actual.parameters().size())return false;
  for(std::size_t i=0;i<pattern.parameters().size();++i)
   if(!match(pattern.parameters()[i],actual.parameters()[i],substitutions))return false;
  return match(pattern.result(),actual.result(),substitutions);
 }
 if(pattern.is_code())return match(pattern.code_result(),actual.code_result(),substitutions);
 if(pattern.is_tuple()||pattern.is_data()){
  if(pattern.elements().size()!=actual.elements().size())return false;
  if(pattern.is_data()&&(pattern.labels()!=actual.labels()||pattern.extent()!=actual.extent()))return false;
  for(std::size_t i=0;i<pattern.elements().size();++i)
   if(!match(pattern.elements()[i],actual.elements()[i],substitutions))return false;
 }
 return true;
}

const ClicheImplementationMember& implementation_member(
    const ClicheImplementation& implementation,const std::string& name,const Location& at){
 auto found=std::find_if(implementation.members.begin(),implementation.members.end(),
                         [&](const auto& item){return item.name==name;});
 if(found==implementation.members.end())
  fail(at,"selected cliche implementation has no member '"+name+"'","ON1X-CLICHE-SIGNATURE");
 return *found;
}

class Resolver {
 const Unit& unit;
 NominalCatalog nominals;
 ResolutionLimits limits;
 std::size_t work=0;
 std::vector<std::string> stack;

 void tick(const Location& at){
  if(work++>=limits.work)
   fail(at,"cliche resolution work budget exhausted","ON1X-CLICHE-LIMIT");
 }
 std::vector<Type> normalize(const std::vector<Type>& types,const Location& at){
  std::vector<Type> result;result.reserve(types.size());
  for(const auto& type:types)result.push_back(nominals.resolve(type,at));
  return result;
 }
 Selection select(const std::string& name,const std::vector<Type>& raw,const Location& at){
  tick(at);
  const auto& cliche=declaration(unit,name,at);
  if(raw.size()!=cliche.type_parameters.size()+1)
   fail(at,"wrong cliche argument arity for "+name,"ON1X-CLICHE-OBLIGATION");
  auto arguments=normalize(raw,at);
  std::string key=name+"<";for(const auto& argument:arguments)key+=type_name(argument)+";";key+=">";
  if(stack.size()>=limits.depth)
   fail(at,"cliche resolution depth budget exhausted","ON1X-CLICHE-LIMIT");
  if(std::find(stack.begin(),stack.end(),key)!=stack.end())
   fail(at,"recursive cliche obligation: "+key,"ON1X-CLICHE-RECURSIVE");

  struct Candidate {
   const ClicheImplementation* implementation;
   std::vector<Type> substitutions;
   bool exact;
  };
  std::vector<Candidate> candidates;
  for(const auto& implementation:unit.cliche_implementations){
   tick(at);
   if(implementation.name!=name||implementation.arguments.size()!=arguments.size())continue;
   std::vector<Type> substitutions(implementation.type_parameters.size(),Type::Invalid);
   bool okay=true;
   for(std::size_t i=0;i<arguments.size();++i)
    if(!match(implementation.arguments[i],arguments[i],substitutions)){okay=false;break;}
   if(okay&&std::none_of(substitutions.begin(),substitutions.end(),
                         [](const Type& type){return type==Type::Invalid;}))
    candidates.push_back({&implementation,std::move(substitutions),
                          std::none_of(implementation.arguments.begin(),implementation.arguments.end(),contains_bound)});
  }
  std::vector<Candidate*> exact;
  for(auto& candidate:candidates)if(candidate.exact)exact.push_back(&candidate);
  Candidate* selected=nullptr;
  if(exact.size()==1)selected=exact.front();
  else if(exact.size()>1)
   fail(at,"ambiguous exact cliche implementation for "+key,"ON1X-CLICHE-AMBIGUOUS");
  else if(candidates.size()==1)selected=&candidates.front();
  else if(candidates.size()>1)
   fail(at,"ambiguous generic cliche implementation for "+key,"ON1X-CLICHE-AMBIGUOUS");
  else fail(at,"unsatisfied cliche obligation: "+key,"ON1X-CLICHE-UNSATISFIED");

  stack.push_back(key);
  std::vector<Type> cliche_variables=arguments;
  for(const auto& constraint:cliche.constraints)
   require_constraint(substitute(constraint,cliche_variables,at),at);
  for(const auto& constraint:selected->implementation->constraints)
   require_constraint(substitute(constraint,selected->substitutions,at),at);
  stack.pop_back();
  return {selected->implementation,nullptr,std::move(selected->substitutions)};
 }
 public:
 explicit Resolver(const Unit& source,ResolutionLimits budget)
   :unit(source),nominals(source),limits(budget){}
 void require_constraint(const ClicheConstraint& constraint,const Location& at){
  (void)select(constraint.name,constraint.arguments,at);
 }
 Selection resolve_member(const std::string& name,const std::vector<Type>& arguments,
                          const std::string& member_name,const std::vector<Type>& member_arguments,
                          const Location& at){
  auto selected=select(name,arguments,at);
  const auto& declared=member(unit,name,member_name,at);
  if(member_arguments.size()!=declared.type_parameters.size())
   fail(at,"wrong member type-argument arity for "+name+"::"+member_name,
        "ON1X-CLICHE-OBLIGATION");
  std::vector<Type> variables=normalize(arguments,at);
  auto member_types=normalize(member_arguments,at);
  variables.insert(variables.end(),member_types.begin(),member_types.end());
  for(const auto& constraint:declared.constraints)
   require_constraint(substitute(constraint,variables,at),at);
  selected.member=&implementation_member(*selected.implementation,member_name,at);
  return selected;
 }
};

void validate_constraint(const Unit& unit,const ClicheConstraint& constraint,
                         std::size_t variables,std::size_t& work){
 const auto& target=declaration(unit,constraint.name,constraint.where);
 if(constraint.arguments.size()!=target.type_parameters.size()+1)
  fail(constraint.where,"wrong cliche argument arity for "+constraint.name,
       "ON1X-CLICHE-SIGNATURE");
 for(const auto& argument:constraint.arguments)
  validate_type(argument,variables,constraint.where,work);
}

std::vector<ClicheConstraint> substituted_member_constraints(
    const ClicheMember& member,
    const ClicheImplementation& implementation,const Location& at){
 std::vector<Type> variables=implementation.arguments;
 for(std::size_t i=0;i<member.type_parameters.size();++i)
  variables.push_back(Type::bound(implementation.type_parameters.size()+i));
 std::vector<ClicheConstraint> result;
 for(const auto& constraint:member.constraints)result.push_back(substitute(constraint,variables,at));
 return result;
}

bool same_type_parameters(const std::vector<TypeParameter>& left,
                          const std::vector<TypeParameter>& right){
 if(left.size()!=right.size())return false;
 for(std::size_t i=0;i<left.size();++i)
  if(left[i].name!=right[i].name||
     !same_constraints(left[i].bounds,right[i].bounds))return false;
 return true;
}

void verify_generated_members(const Unit& unit){
 NominalCatalog nominals(unit);
 std::set<std::string> runtime_names;
 std::vector<const ClicheImplementation*> ordered;
 for(const auto& implementation:unit.cliche_implementations)
  ordered.push_back(&implementation);
 std::sort(ordered.begin(),ordered.end(),[](const auto* left,const auto* right){
  return implementation_key(*left)<implementation_key(*right);
 });
 std::map<const ClicheImplementation*,std::size_t> identities;
 for(std::size_t i=0;i<ordered.size();++i)identities.emplace(ordered[i],i+1);
 std::size_t work=262144;
 for(const auto& implementation:unit.cliche_implementations){
  for(const auto& parameter:implementation.type_parameters)
   for(const auto& bound:parameter.bounds)
    validate_constraint(unit,bound,implementation.type_parameters.size(),work);
  for(const auto& constraint:implementation.constraints)
   validate_constraint(unit,constraint,implementation.type_parameters.size(),work);
  for(const auto& item:implementation.members){
   if(!work--)fail(item.where,"generated cliche verification budget exhausted",
                   "ON1X-CLICHE-LIMIT");
   const auto identity="@cliche_"+
    std::to_string(identities.at(&implementation))+"_"+item.name;
   if(item.runtime_name!=identity)
    fail(item.where,"generated cliche member identity mismatch",
         "ON1X-CLICHE-HIR");
   if(item.runtime_name.empty()||
      !runtime_names.insert(item.runtime_name).second)
    fail(item.where,"missing or duplicate generated cliche member identity",
         "ON1X-CLICHE-HIR");
   const Function* generated=nullptr;
   for(const auto& function:unit.functions)
    if(function.name==item.runtime_name){
     if(generated)
      fail(item.where,"duplicate generated cliche member function",
           "ON1X-CLICHE-HIR");
     generated=&function;
    }
   if(!generated)
    fail(item.where,"generated cliche member function is missing",
         "ON1X-CLICHE-HIR");

   std::vector<TypeParameter> type_parameters=implementation.type_parameters;
   type_parameters.insert(type_parameters.end(),
                          item.function.type_parameters.begin(),
                          item.function.type_parameters.end());
   std::vector<ClicheConstraint> constraints=implementation.constraints;
   constraints.insert(constraints.end(),item.function.constraints.begin(),
                      item.function.constraints.end());
   for(const auto& parameter:item.function.type_parameters)
    for(const auto& bound:parameter.bounds)
     validate_constraint(unit,bound,type_parameters.size(),work);
   for(const auto& constraint:item.function.constraints)
    validate_constraint(unit,constraint,type_parameters.size(),work);
   if(!same_type_parameters(generated->type_parameters,type_parameters)||
      !same_constraints(generated->constraints,constraints))
    fail(item.where,"generated cliche member generic metadata mismatch",
         "ON1X-CLICHE-HIR");
   if(generated->external||generated->extension_generated||
      !generated->kernel_symbol.empty()||generated->kernel_link_only||
      !generated->attributes.empty()||
      generated->prototype!=!type_parameters.empty())
    fail(item.where,"forged generated cliche member function metadata",
         "ON1X-CLICHE-HIR");

   const auto& expected_parameters=item.function.params;
   if(generated->params.size()!=expected_parameters.size())
    fail(item.where,"generated cliche member signature mismatch",
         "ON1X-CLICHE-HIR");
   std::vector<Type> parameters;
   for(std::size_t i=0;i<expected_parameters.size();++i){
    if(generated->params[i].name!=expected_parameters[i].name||
       generated->params[i].exclusive!=expected_parameters[i].exclusive)
     fail(item.where,"generated cliche member parameter metadata mismatch",
          "ON1X-CLICHE-HIR");
    parameters.push_back(expected_parameters[i].type);
   }
   auto result=item.kind==ClicheMember::Kind::Function?
               item.function.result:item.value_type;
   auto expected=nominals.resolve(
    Type::function(std::move(parameters),std::move(result)),item.where);
   std::vector<Type> actual_parameters;
   for(const auto& parameter:generated->params)
    actual_parameters.push_back(parameter.type);
   auto actual=Type::function(std::move(actual_parameters),generated->result);
   const bool signature_matches=type_parameters.empty()?
    expected==actual:
    forall_proof(Type::for_all(type_parameters.size(),expected),actual);
   if(!signature_matches)
    fail(item.where,"generated cliche member signature mismatch",
         "ON1X-CLICHE-HIR");
  }
 }
}

} // namespace

bool present(const Unit& unit){
 return !unit.cliches.empty()||!unit.cliche_implementations.empty();
}

const ClicheDeclaration& declaration(const Unit& unit,const std::string& name,const Location& at){
 auto found=std::find_if(unit.cliches.begin(),unit.cliches.end(),
                         [&](const auto& cliche){return cliche.name==name;});
 if(found==unit.cliches.end())fail(at,"unknown cliche: "+name,"ON1X-CLICHE-OBLIGATION");
 return *found;
}

const ClicheMember& member(const Unit& unit,const std::string& cliche,
                           const std::string& name,const Location& at){
 const auto& owner=declaration(unit,cliche,at);
 auto found=std::find_if(owner.members.begin(),owner.members.end(),
                         [&](const auto& item){return item.name==name;});
 if(found==owner.members.end())
  fail(at,"cliche "+cliche+" has no member '"+name+"'","ON1X-CLICHE-OBLIGATION");
 return *found;
}

Type substitute(const Type& source,const std::vector<Type>& variables,const Location& at){
 std::size_t work=16384;
 auto walk=[&](auto&& self,const Type& type,std::size_t depth)->Type{
  if(!work--)fail(at,"cliche substitution work budget exhausted","ON1X-CLICHE-LIMIT");
  if(depth>256)fail(at,"cliche substitution depth budget exhausted","ON1X-CLICHE-LIMIT");
   if(type.kind()==Type::Bound){
    if(type.generic_identity()>=variables.size())
     fail(at,"unbound cliche substitution variable","ON1X-CLICHE-SIGNATURE");
    return variables[type.generic_identity()];
   }
   if(type.is_own())return Type::own(self(self,type.owned(),depth+1));
   std::vector<Type> arguments;
  for(const auto& argument:type.arguments())arguments.push_back(self(self,argument,depth+1));
  std::vector<Type> children;Type result=Type::Invalid;
  if(type.is_function()){
   for(const auto& parameter:type.parameters())children.push_back(self(self,parameter,depth+1));
   result=self(self,type.result(),depth+1);
  }else if(type.is_code())result=self(self,type.code_result(),depth+1);
  else if(type.is_forall())fail(at,"forall is not supported in cliche signatures","ON1X-CLICHE-SIGNATURE");
  else if(type.is_tuple()||type.is_data())
   for(const auto& element:type.elements())children.push_back(self(self,element,depth+1));
  return rebuild(type,std::move(children),std::move(arguments),std::move(result),at);
 };
 return walk(walk,source,0);
}

ClicheConstraint substitute(const ClicheConstraint& source,
                             const std::vector<Type>& variables,const Location& at){
 ClicheConstraint result;result.where=source.where;result.name=source.name;
 for(const auto& argument:source.arguments)result.arguments.push_back(substitute(argument,variables,at));
 return result;
}

void prepare(Unit& unit){
 if(unit.cliches_prepared)return;
 if(!present(unit)){unit.cliches_prepared=true;return;}
  if(!unit.ppx.empty()||!unit.features.empty()||!unit.extensions.empty()||
     !unit.extension_evidence.empty()){
   const auto& at=!unit.ppx.empty()?unit.ppx.front().where:
                  !unit.features.empty()?unit.features.front().where:
                  !unit.extensions.empty()?unit.extensions.front().where:
                  unit.cliche_implementations.empty()?unit.cliches.front().where:
                 unit.cliche_implementations.front().where;
  fail(at,"core cliche declarations cannot cross PPX/Category Unit-v1 transport",
       "ON1X-CLICHE-UNSUPPORTED");
 }
 if(unit.cliches.size()>256||unit.cliche_implementations.size()>1024)
  fail((!unit.cliches.empty()?unit.cliches.front().where:unit.cliche_implementations.front().where),
       "cliche declaration/implementation budget exhausted","ON1X-CLICHE-LIMIT");

 std::size_t validation_work=262144;
 std::set<std::string> names;
 for(const auto& cliche:unit.cliches){
  if(!identifier(cliche.name)||!names.insert(cliche.name).second)
   fail(cliche.where,"invalid or duplicate cliche declaration: "+cliche.name,
        "ON1X-CLICHE-SIGNATURE");
  if(cliche.type_parameters.size()>31||cliche.members.empty()||cliche.members.size()>128)
   fail(cliche.where,"invalid cliche parameter/member count","ON1X-CLICHE-SIGNATURE");
  std::set<std::string> parameters{"Self"},members;
  for(const auto& parameter:cliche.type_parameters)
   if(!identifier(parameter.name)||!parameters.insert(parameter.name).second)
    fail(parameter.where,"invalid or duplicate cliche type parameter: "+parameter.name,
         "ON1X-CLICHE-SIGNATURE");
  auto base=cliche.type_parameters.size()+1;
  for(const auto& constraint:cliche.constraints)
   validate_constraint(unit,constraint,base,validation_work);
  for(const auto& item:cliche.members){
   if(!identifier(item.name)||!members.insert(item.name).second)
    fail(item.where,"invalid or duplicate cliche member: "+item.name,
         "ON1X-CLICHE-SIGNATURE");
   if(item.type_parameters.size()>31)
    fail(item.where,"cliche member type-parameter limit exceeded","ON1X-CLICHE-LIMIT");
   auto variables=base+item.type_parameters.size();
   std::set<std::string> member_parameters=parameters;
    for(const auto& parameter:item.type_parameters){
     if(!identifier(parameter.name)||!member_parameters.insert(parameter.name).second)
      fail(parameter.where,"invalid or duplicate member type parameter: "+parameter.name,
           "ON1X-CLICHE-SIGNATURE");
    }
    std::set<std::string> arguments;
    if(item.kind==ClicheMember::Kind::Value&&!item.params.empty())
     fail(item.where,"associated values cannot declare value parameters","ON1X-CLICHE-SIGNATURE");
    for(const auto& parameter:item.params){
     if(parameter.exclusive)
      fail(parameter.where,"ref parameters are not supported in cliche declarations",
           "ON1X-CLICHE-SIGNATURE");
     if(parameter.pattern)
      fail(parameter.where,"cliche declarations require named parameters",
           "ON1X-CLICHE-SIGNATURE");
     if(!arguments.insert(parameter.name).second)
      fail(parameter.where,"duplicate cliche member parameter: "+parameter.name,
           "ON1X-CLICHE-SIGNATURE");
    validate_type(parameter.type,variables,parameter.where,validation_work);
   }
   validate_type(item.result,variables,item.where,validation_work);
   for(const auto& constraint:item.constraints)
    validate_constraint(unit,constraint,variables,validation_work);
  }
 }

 std::map<std::string,const NominalDeclaration*> nominals;
 for(const auto& nominal:unit.nominals)nominals.emplace(nominal.name,&nominal);
 std::set<std::string> implementation_heads;
 for(const auto& implementation:unit.cliche_implementations){
  const auto& cliche=declaration(unit,implementation.name,implementation.where);
  if(implementation.arguments.size()!=cliche.type_parameters.size()+1)
   fail(implementation.where,"wrong implementation head arity for "+implementation.name,
        "ON1X-CLICHE-SIGNATURE");
  if(implementation.type_parameters.size()>32||implementation.members.size()>128)
   fail(implementation.where,"cliche implementation budget exhausted","ON1X-CLICHE-LIMIT");
  std::set<std::string> parameters;
  for(const auto& parameter:implementation.type_parameters)
   if(!identifier(parameter.name)||!parameters.insert(parameter.name).second)
    fail(parameter.where,"invalid or duplicate implementation type parameter: "+parameter.name,
         "ON1X-CLICHE-SIGNATURE");
  for(const auto& argument:implementation.arguments)
   validate_type(argument,implementation.type_parameters.size(),implementation.where,validation_work);
  for(const auto& constraint:implementation.constraints)
   validate_constraint(unit,constraint,implementation.type_parameters.size(),validation_work);
  std::vector<bool> used(implementation.type_parameters.size());
   auto mark=[&](auto&& self,const Type& type)->void{
    if(type.kind()==Type::Bound){if(type.generic_identity()<used.size())used[type.generic_identity()]=true;return;}
    if(type.is_own()){self(self,type.owned());return;}
    for(const auto& argument:type.arguments())self(self,argument);
   if(type.is_function()){for(const auto& parameter:type.parameters())self(self,parameter);self(self,type.result());}
   else if(type.is_code())self(self,type.code_result());
   else if(type.is_tuple()||type.is_data())for(const auto& element:type.elements())self(self,element);
  };
  for(const auto& argument:implementation.arguments)mark(mark,argument);
  if(std::find(used.begin(),used.end(),false)!=used.end())
   fail(implementation.where,"every implementation type parameter must occur in its head",
        "ON1X-CLICHE-COHERENCE");
  const bool local_cliche=cliche.where.file==implementation.where.file;
  bool local_type=false;
  const auto& target=implementation.arguments.front();
  auto nominal=target.kind()==Type::Named?target.nominal():target.nominal();
  if(!nominal.empty())if(auto found=nominals.find(nominal);found!=nominals.end())
   local_type=found->second->where.file==implementation.where.file;
  if(!local_cliche&&!local_type)
   fail(implementation.where,"orphan cliche implementation: define the cliche or target type in this module",
        "ON1X-CLICHE-ORPHAN");
  auto head_size=std::size_t{0};
  for(const auto& argument:implementation.arguments)head_size+=type_size(argument);
  for(const auto& constraint:implementation.constraints){
   std::size_t constraint_size=0;
   for(const auto& argument:constraint.arguments)constraint_size+=type_size(argument);
   if(constraint_size>=head_size)
    fail(constraint.where,"implementation prerequisite does not decrease structurally: "+
         constraint_name(constraint),"ON1X-CLICHE-RECURSIVE");
  }

  std::map<std::string,const ClicheImplementationMember*> members;
  for(const auto& item:implementation.members)
   if(!members.emplace(item.name,&item).second)
    fail(item.where,"duplicate implementation member: "+item.name,"ON1X-CLICHE-SIGNATURE");
  if(members.size()!=cliche.members.size())
   fail(implementation.where,"cliche implementation has missing or extra members",
        "ON1X-CLICHE-SIGNATURE");
  for(const auto& declared:cliche.members){
   auto found=members.find(declared.name);
   if(found==members.end())
    fail(implementation.where,"missing cliche member: "+declared.name,"ON1X-CLICHE-SIGNATURE");
   const auto& actual=*found->second;
    if(actual.kind!=declared.kind)
     fail(actual.where,"member kind mismatch for "+declared.name,"ON1X-CLICHE-SIGNATURE");
    const auto& actual_function=actual.function;
    if(actual_function.name!=actual.name||!actual.runtime_name.empty()||
       actual_function.external||actual_function.prototype||
       actual_function.extension_generated||!actual_function.kernel_symbol.empty()||
       actual_function.kernel_link_only||!actual_function.attributes.empty())
     fail(actual.where,"malformed hidden cliche implementation metadata",
          "ON1X-CLICHE-HIR");
    if(actual_function.type_parameters.size()!=declared.type_parameters.size())
      fail(actual.where,"member generic arity mismatch for "+declared.name,
           "ON1X-CLICHE-SIGNATURE");
    std::vector<Type> variables=implementation.arguments;
    for(std::size_t i=0;i<declared.type_parameters.size();++i){
     variables.push_back(Type::bound(implementation.type_parameters.size()+i));
    }
    auto expected_result=substitute(declared.result,variables,actual.where);
    if(declared.kind==ClicheMember::Kind::Function){
     if(actual.value||actual.value_type!=Type::Invalid)
      fail(actual.where,"function member carries associated-value metadata",
           "ON1X-CLICHE-HIR");
     std::vector<Type> expected_parameters;
     for(const auto& parameter:declared.params){
      expected_parameters.push_back(substitute(parameter.type,variables,actual.where));
     }
     std::vector<Type> actual_parameters;
     for(const auto& parameter:actual_function.params)actual_parameters.push_back(parameter.type);
     bool modes_match=actual_function.params.size()==declared.params.size();
     for(std::size_t i=0;modes_match&&i<declared.params.size();++i)
      modes_match=actual_function.params[i].exclusive==declared.params[i].exclusive;
     if(actual_parameters!=expected_parameters||actual_function.result!=expected_result||
        !modes_match)
      fail(actual.where,"signature mismatch for cliche member "+declared.name+
           ": expected "+type_name(Type::function(expected_parameters,expected_result))+
           ", found "+type_name(Type::function(actual_parameters,actual_function.result))+
           (modes_match?"":" (parameter modes differ)"),
           "ON1X-CLICHE-SIGNATURE");
     }else{
      if(!actual_function.params.empty()||!actual_function.body.empty()||
         actual_function.result!=actual.value_type)
       fail(actual.where,"associated value carries function metadata",
            "ON1X-CLICHE-HIR");
      if(!actual.value)
      fail(actual.where,"associated value has no implementation: "+declared.name,
           "ON1X-CLICHE-SIGNATURE");
     if(actual.value_type!=expected_result)
      fail(actual.where,"associated-value type mismatch for "+declared.name+
           ": expected "+type_name(expected_result)+", found "+type_name(actual.value_type),
           "ON1X-CLICHE-SIGNATURE");
    }
   auto expected_constraints=substituted_member_constraints(
       declared,implementation,actual.where);
   if(!same_constraints(expected_constraints,actual_function.constraints))
    fail(actual.where,"member constraint mismatch for "+declared.name,
         "ON1X-CLICHE-SIGNATURE");
  }
  auto key=implementation_key(implementation);
  if(!implementation_heads.insert(key).second)
   fail(implementation.where,"duplicate cliche implementation head","ON1X-CLICHE-COHERENCE");
 }

 for(std::size_t i=0;i<unit.cliche_implementations.size();++i)
  for(std::size_t j=i+1;j<unit.cliche_implementations.size();++j){
   const auto& left=unit.cliche_implementations[i];
   const auto& right=unit.cliche_implementations[j];
   if(left.name!=right.name||left.arguments.size()!=right.arguments.size())continue;
   const bool left_generic=std::any_of(left.arguments.begin(),left.arguments.end(),contains_bound);
   const bool right_generic=std::any_of(right.arguments.begin(),right.arguments.end(),contains_bound);
   if(left_generic!=right_generic)continue; // Exact implementations deliberately override blanket ones.
   bool overlap=true;
   for(std::size_t k=0;k<left.arguments.size();++k)
    if(!may_overlap(left.arguments[k],right.arguments[k])){overlap=false;break;}
   if(overlap)fail(right.where,left_generic?
     "overlapping generic cliche implementations are not coherent":
     "duplicate exact cliche implementations are not coherent",
     "ON1X-CLICHE-COHERENCE");
  }

 std::vector<ClicheImplementation*> ordered;
 for(auto& implementation:unit.cliche_implementations)ordered.push_back(&implementation);
 std::sort(ordered.begin(),ordered.end(),[](const auto* left,const auto* right){
  return implementation_key(*left)<implementation_key(*right);
 });
 std::vector<Function> generated;
 std::vector<std::vector<std::string>> runtime_names;
 std::size_t clone_budget=262144;
 for(std::size_t i=0;i<ordered.size();++i){
  auto* implementation=ordered[i];
  std::vector<ClicheImplementationMember*> members;
  for(auto& item:implementation->members)members.push_back(&item);
  std::sort(members.begin(),members.end(),[](const auto* left,const auto* right){return left->name<right->name;});
  runtime_names.emplace_back();
  for(const auto* item:members){
   auto runtime="@cliche_"+std::to_string(i+1)+"_"+item->name;
   if(std::any_of(unit.functions.begin(),unit.functions.end(),
                  [&](const Function& function){return function.name==runtime;}))
    fail(item->where,"internal cliche member name collision","ON1X-CLICHE-COHERENCE");
   Function function;function.name=runtime;function.where=item->where;
   function.type_parameters=implementation->type_parameters;
   function.type_parameters.insert(function.type_parameters.end(),
                                    item->function.type_parameters.begin(),
                                    item->function.type_parameters.end());
   function.constraints=implementation->constraints;
   function.constraints.insert(function.constraints.end(),
                               item->function.constraints.begin(),item->function.constraints.end());
   if(item->kind==ClicheMember::Kind::Function){
    function.params=item->function.params;function.result=item->function.result;
    for(const auto& statement:item->function.body)
     function.body.push_back(clone_statement(statement,clone_budget));
   }else{
    function.result=item->value_type;
    Stmt statement;statement.kind=Stmt::Kind::Return;statement.where=item->where;
    statement.value=clone_expression(*item->value,clone_budget);
    function.body.push_back(std::move(statement));
   }
   generated.push_back(std::move(function));runtime_names.back().push_back(std::move(runtime));
  }
 }
 for(std::size_t i=0;i<ordered.size();++i){
  std::vector<ClicheImplementationMember*> members;
  for(auto& item:ordered[i]->members)members.push_back(&item);
  std::sort(members.begin(),members.end(),[](const auto* left,const auto* right){return left->name<right->name;});
  for(std::size_t j=0;j<members.size();++j)members[j]->runtime_name=runtime_names[i][j];
 }
 for(auto& function:generated)unit.functions.push_back(std::move(function));
 unit.cliches_prepared=true;
}

Selection resolve(const Unit& unit,const std::string& name,const std::vector<Type>& arguments,
                  const std::string& member_name,const std::vector<Type>& member_arguments,
                  const Location& at,const ResolutionLimits& limits){
 if(!unit.cliches_prepared)
  fail(at,"cliche resolution attempted before declaration preparation","ON1X-CLICHE-UNSUPPORTED");
 return Resolver(unit,limits).resolve_member(name,arguments,member_name,member_arguments,at);
}

void require(const Unit& unit,const ClicheConstraint& constraint,const Location& at,
             const ResolutionLimits& limits){
 if(!unit.cliches_prepared)
  fail(at,"cliche resolution attempted before declaration preparation","ON1X-CLICHE-UNSUPPORTED");
 Resolver(unit,limits).require_constraint(constraint,at);
}

void verify_elaborated(const Unit& unit){
  if(present(unit)&&!unit.cliches_prepared){
   const auto& at=!unit.cliches.empty()?unit.cliches.front().where:
                  unit.cliche_implementations.front().where;
    fail(at,"cliche metadata was not prepared before backend admission",
         "ON1X-CLICHE-HIR");
   }
   if(unit.cliches_prepared)verify_generated_members(unit);
   auto expression=[&](auto&& self,const Expr& value)->void{
    if(value.kind==Expr::Kind::ClicheMember)
     fail(value.where,"unresolved concrete cliche member reached checked HIR",
          "ON1X-CLICHE-UNSUPPORTED");
    if(value.callee)self(self,*value.callee);
    if(value.syntax_tail)self(self,*value.syntax_tail);
   for(const auto& child:value.children){
    if(!child)fail(value.where,"null expression child in checked HIR",
                   "ON1X-CLICHE-HIR");
    self(self,*child);
   }
   for(const auto& proof:value.proofs){
    if(!proof)fail(value.where,"null universal proof in checked HIR",
                   "ON1X-CLICHE-HIR");
    self(self,*proof);
   }
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements)->void{
   for(const auto& statement:statements){
    if(statement.value)self(self,*statement.value);
    walk(walk,statement.body);walk(walk,statement.otherwise);
   }
  };
  body(body,value.body);
 };
 auto body=[&](auto&& self,const std::vector<Stmt>& statements)->void{
  for(const auto& statement:statements){
   if(statement.value)expression(expression,*statement.value);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
  for(const auto& function:unit.functions)if(!function.prototype){
   for(const auto& attribute:function.attributes){
    if(!attribute)fail(function.where,"null function attribute in checked HIR",
                       "ON1X-CLICHE-HIR");
    expression(expression,*attribute);
   }
   body(body,function.body);
  }
}

} // namespace on1x::cliches

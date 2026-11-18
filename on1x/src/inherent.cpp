#include <on1x/inherent.hpp>
#include <on1x/clone.hpp>
#include <on1x/nominal.hpp>
#include <algorithm>
#include <cctype>
#include <set>

namespace on1x::inherent {
namespace {

[[noreturn]] void fail(const Location& at,const std::string& message,
                       const char* id="ON1X-IMPL"){
 throw Error(at,std::string(id)+": "+message,id);
}

bool identifier(const std::string& name){
 if(name.empty()||!(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_'))
  return false;
 return std::all_of(name.begin(),name.end(),[](unsigned char c){
  return std::isalnum(c)||c=='_';
 });
}

void check_type(const Type& type,std::size_t variables,const Location& at,
                std::size_t& budget,std::size_t depth=0){
 if(depth>64||!budget)fail(at,"type validation budget exhausted","ON1X-IMPL-LIMIT");
 --budget;
 if(type.kind()==Type::Bound&&type.generic_identity()>=variables)
  fail(at,"member type refers to an unbound implementation parameter");
 if(type.is_own()){
  check_type(type.owned(),variables,at,budget,depth+1);
  return;
 }
 for(const auto& argument:type.arguments())
  check_type(argument,variables,at,budget,depth+1);
 if(type.is_function()){
  for(const auto& parameter:type.parameters())
   check_type(parameter,variables,at,budget,depth+1);
  check_type(type.result(),variables,at,budget,depth+1);
 }else if(type.is_forall())check_type(type.body(),variables,at,budget,depth+1);
 else if(type.is_code())check_type(type.code_result(),variables,at,budget,depth+1);
 else if(type.is_tuple()||type.is_data())
  for(const auto& field:type.elements())
   check_type(field,variables,at,budget,depth+1);
}

void collect_bounds(const Type& type,std::set<std::size_t>& found,
                    std::size_t& budget,std::size_t depth=0){
 if(depth>64||!budget)return;
 --budget;
 if(type.kind()==Type::Bound)found.insert(type.generic_identity());
 if(type.is_own()){
  collect_bounds(type.owned(),found,budget,depth+1);
  return;
 }
 for(const auto& argument:type.arguments())collect_bounds(argument,found,budget,depth+1);
 if(type.is_function()){
  for(const auto& parameter:type.parameters())collect_bounds(parameter,found,budget,depth+1);
  collect_bounds(type.result(),found,budget,depth+1);
 }else if(type.is_forall())collect_bounds(type.body(),found,budget,depth+1);
 else if(type.is_code())collect_bounds(type.code_result(),found,budget,depth+1);
 else if(type.is_tuple()||type.is_data())
  for(const auto& field:type.elements())collect_bounds(field,found,budget,depth+1);
}

Function generated_function(const InherentImplementation& implementation,
                            const InherentMember& member,std::size_t& clone_budget){
 Function out;
 out.name=member.runtime_name;
 out.where=member.where;
 out.type_parameters=implementation.type_parameters;
 out.type_parameters.insert(out.type_parameters.end(),
                            member.type_parameters.begin(),member.type_parameters.end());
 if(member.kind==InherentMember::Kind::Function){
  out.params=member.function.params;
  out.result=member.function.result;
  for(const auto& statement:member.function.body)
   out.body.push_back(clone_statement(statement,clone_budget));
 }else{
  out.params=member.function.params; // Optional value receiver.
  out.result=member.value_type;
  Stmt returned;
  returned.kind=Stmt::Kind::Return;
  returned.where=member.where;
  returned.value=clone_expression(*member.value,clone_budget);
  out.body.push_back(std::move(returned));
 }
 return out;
}

template<class F> void expressions(const Unit& unit,F&& visit){
 auto expression=[&](auto&& self,const Expr& e)->void{
  visit(e);
  if(e.callee)self(self,*e.callee);
  if(e.syntax_tail)self(self,*e.syntax_tail);
  for(const auto& child:e.children){
   if(!child)fail(e.where,"null expression child in checked HIR",
                  "ON1X-IMPL-HIR");
   self(self,*child);
  }
  for(const auto& proof:e.proofs){
   if(!proof)fail(e.where,"null universal proof in checked HIR",
                  "ON1X-IMPL-HIR");
   self(self,*proof);
  }
  auto body=[&](auto&& walk,const std::vector<Stmt>& statements)->void{
   for(const auto& statement:statements){
    if(statement.value)self(self,*statement.value);
    walk(walk,statement.body);walk(walk,statement.otherwise);
   }
  };
  body(body,e.body);
 };
 auto body=[&](auto&& self,const std::vector<Stmt>& statements)->void{
  for(const auto& statement:statements){
   if(statement.value)expression(expression,*statement.value);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
 for(const auto& function:unit.functions){
  for(const auto& attribute:function.attributes){
   if(!attribute)fail(function.where,"null function attribute in checked HIR",
                       "ON1X-IMPL-HIR");
   expression(expression,*attribute);
  }
  body(body,function.body);
 }
}

bool same_constraint(const ClicheConstraint& left,
                     const ClicheConstraint& right){
 return left.name==right.name&&left.arguments==right.arguments;
}

bool same_type_parameters(const std::vector<TypeParameter>& left,
                          const std::vector<TypeParameter>& right){
 if(left.size()!=right.size())return false;
 for(std::size_t i=0;i<left.size();++i){
  if(left[i].name!=right[i].name||
     left[i].bounds.size()!=right[i].bounds.size())return false;
  for(std::size_t j=0;j<left[i].bounds.size();++j)
   if(!same_constraint(left[i].bounds[j],right[i].bounds[j]))return false;
 }
 return true;
}

void verify_generated_members(const Unit& unit){
 NominalCatalog nominals(unit);
 std::set<std::string> runtime_names;
 std::size_t work=262144;
 for(std::size_t i=0;i<unit.inherent_implementations.size();++i){
  const auto& implementation=unit.inherent_implementations[i];
  for(const auto& parameter:implementation.type_parameters)
   if(!parameter.bounds.empty())
    fail(parameter.where,
         "bounded inherent implementation metadata reached checked HIR",
         "ON1X-IMPL-HIR");
  for(std::size_t j=0;j<implementation.members.size();++j){
   const auto& member=implementation.members[j];
   if(!work--)fail(member.where,"generated member verification budget exhausted",
                   "ON1X-IMPL-LIMIT");
   for(const auto& parameter:member.type_parameters)
    if(!parameter.bounds.empty())
     fail(parameter.where,
          "bounded inherent member metadata reached checked HIR",
          "ON1X-IMPL-HIR");
   if(!member.function.type_parameters.empty()||
      !member.function.constraints.empty())
    fail(member.where,"forged hidden inherent generic metadata",
         "ON1X-IMPL-HIR");
   const auto identity="@impl_"+std::to_string(i)+"_"+std::to_string(j);
   const auto loaded_identity="@inherent_"+std::to_string(i)+"_"+
                              std::to_string(j);
   if(member.runtime_name!=identity&&
      member.runtime_name!=loaded_identity)
    fail(member.where,"generated inherent member identity mismatch",
         "ON1X-IMPL-HIR");
   if(member.runtime_name.empty()||
      !runtime_names.insert(member.runtime_name).second)
    fail(member.where,"missing or duplicate generated member identity",
         "ON1X-IMPL-HIR");
   const Function* generated=nullptr;
   for(const auto& function:unit.functions)
    if(function.name==member.runtime_name){
     if(generated)
      fail(member.where,"duplicate generated inherent member function",
           "ON1X-IMPL-HIR");
     generated=&function;
    }
   if(!generated)
    fail(member.where,"generated inherent member function is missing",
         "ON1X-IMPL-HIR");

   std::vector<TypeParameter> type_parameters=implementation.type_parameters;
   type_parameters.insert(type_parameters.end(),member.type_parameters.begin(),
                          member.type_parameters.end());
   if(!same_type_parameters(generated->type_parameters,type_parameters)||
      !generated->constraints.empty())
    fail(member.where,"generated inherent member generic metadata mismatch",
         "ON1X-IMPL-HIR");
   if(generated->external||generated->extension_generated||
      !generated->kernel_symbol.empty()||generated->kernel_link_only||
      !generated->attributes.empty()||
      generated->prototype!=!type_parameters.empty())
    fail(member.where,"forged generated inherent member function metadata",
         "ON1X-IMPL-HIR");

   if(generated->params.size()!=member.function.params.size())
    fail(member.where,"generated inherent member signature mismatch",
         "ON1X-IMPL-HIR");
   std::vector<Type> parameters;
   for(std::size_t i=0;i<member.function.params.size();++i){
    const auto& expected=member.function.params[i];
    if(generated->params[i].name!=expected.name||
       generated->params[i].exclusive!=expected.exclusive)
     fail(member.where,"generated inherent member parameter metadata mismatch",
          "ON1X-IMPL-HIR");
    parameters.push_back(expected.type);
   }
   auto result=member.kind==InherentMember::Kind::Function?
               member.function.result:member.value_type;
   auto expected=nominals.resolve(
    Type::function(std::move(parameters),std::move(result)),member.where);
   std::vector<Type> actual_parameters;
   for(const auto& parameter:generated->params)
    actual_parameters.push_back(parameter.type);
   auto actual=Type::function(std::move(actual_parameters),generated->result);
   const bool signature_matches=type_parameters.empty()?
    expected==actual:
    forall_proof(Type::for_all(type_parameters.size(),expected),actual);
   if(!signature_matches)
    fail(member.where,"generated inherent member signature mismatch",
         "ON1X-IMPL-HIR");
  }
 }
}

} // namespace

bool present(const Unit& unit){return !unit.inherent_implementations.empty();}

Catalog::Catalog(const Unit& unit){
 std::size_t count=0;
 for(const auto& implementation:unit.inherent_implementations){
  if(implementation.target.kind()!=Type::Named)
   fail(implementation.where,"implementation target is not a nominal source type");
  for(const auto& member:implementation.members){
   if(++count>4096)fail(member.where,"loaded member budget exhausted","ON1X-IMPL-LIMIT");
   auto key=std::pair{implementation.target.nominal(),member.name};
   auto [it,inserted]=members.emplace(std::move(key),Selection{&implementation,&member});
   if(!inserted){
    const auto& first=*it->second.member;
    fail(member.where,"duplicate inherent member '"+member.name+"' for "+
         implementation.target.nominal()+"; first declared at "+first.where.file+":"+
         std::to_string(first.where.line)+":"+std::to_string(first.where.column),
         "ON1X-IMPL-DUPLICATE");
   }
  }
 }
}

Selection Catalog::find(const std::string& nominal,const std::string& name,
                        const Location& at) const{
 auto found=members.find({nominal,name});
 if(found==members.end())
  fail(at,"type "+nominal+" has no inherent member '"+name+"'","ON1X-IMPL-MEMBER");
 return found->second;
}

void prepare(Unit& unit){
 if(unit.inherents_prepared)return;
 if(unit.inherent_implementations.size()>1024)
  fail(unit.inherent_implementations.front().where,
       "implementation block budget exhausted","ON1X-IMPL-LIMIT");

 NominalCatalog nominals(unit);
 std::set<std::string> runtime_names;
 for(const auto& function:unit.functions)runtime_names.insert(function.name);
 std::size_t generated=0,clone_budget=262144;

 for(std::size_t i=0;i<unit.inherent_implementations.size();++i){
  auto& implementation=unit.inherent_implementations[i];
  if(implementation.target.kind()!=Type::Named)
   fail(implementation.where,"target must be a declared record or enum");
  const auto& declaration=nominals.find(implementation.target.nominal(),
                                        implementation.where);
  if(declaration.where.file!=implementation.where.file)
   fail(implementation.where,"inherent implementation must be declared in the same module as "+
        declaration.name,"ON1X-IMPL-COHERENCE");
  if(implementation.target.arguments().size()!=declaration.parameters.size())
   fail(implementation.where,"wrong target type argument arity for "+declaration.name);
  if(implementation.type_parameters.size()>32)
   fail(implementation.where,"implementation type-parameter limit exceeded",
        "ON1X-IMPL-LIMIT");

  std::set<std::string> parameter_names;
  for(const auto& parameter:implementation.type_parameters){
    if(!identifier(parameter.name)||!parameter_names.insert(parameter.name).second)
     fail(parameter.where,"invalid or duplicate implementation type parameter: "+
          parameter.name);
    if(!parameter.bounds.empty())
     fail(parameter.where,"bounded inherent implementations are unsupported",
          "ON1X-IMPL-HIR");
   }
  std::size_t type_budget=16384;
  check_type(implementation.target,implementation.type_parameters.size(),
             implementation.where,type_budget);
  std::set<std::size_t> target_bounds;collect_bounds(implementation.target,target_bounds,type_budget);
  for(std::size_t p=0;p<implementation.type_parameters.size();++p)
   if(!target_bounds.contains(p))
    fail(implementation.type_parameters[p].where,
         "implementation type parameter is not determined by the target: "+
         implementation.type_parameters[p].name,"ON1X-IMPL-COHERENCE");
  auto target=nominals.resolve(implementation.target,implementation.where);

  for(std::size_t j=0;j<implementation.members.size();++j){
   auto& member=implementation.members[j];
   if(!identifier(member.name))fail(member.where,"invalid member name: "+member.name);
   if(member.type_parameters.size()+implementation.type_parameters.size()>32)
    fail(member.where,"combined member type-parameter limit exceeded",
         "ON1X-IMPL-LIMIT");
   auto member_names=parameter_names;
   for(const auto& parameter:member.type_parameters){
     if(!identifier(parameter.name)||!member_names.insert(parameter.name).second)
      fail(parameter.where,"invalid, duplicate or shadowing member type parameter: "+
           parameter.name);
     if(!parameter.bounds.empty())
      fail(parameter.where,"bounded inherent members are unsupported",
           "ON1X-IMPL-HIR");
    }
   const auto variables=implementation.type_parameters.size()+member.type_parameters.size();

   if((member.kind!=InherentMember::Kind::Function&&
       member.kind!=InherentMember::Kind::Value)||
      (member.receiver!=InherentMember::Receiver::None&&
       member.receiver!=InherentMember::Receiver::Value&&
       member.receiver!=InherentMember::Receiver::Borrow&&
       member.receiver!=InherentMember::Receiver::Move))
    fail(member.where,"invalid inherent member kind or receiver metadata",
         "ON1X-IMPL-HIR");

   if(member.receiver==InherentMember::Receiver::Borrow)
    fail(member.where,"borrowed receivers require lifetime/place lowering and are not supported",
         "ON1X-IMPL-RECEIVER");
   if(member.receiver==InherentMember::Receiver::Move)
    fail(member.where,"move receivers require consuming ownership lowering and are not supported",
         "ON1X-IMPL-RECEIVER");
   if(member.receiver==InherentMember::Receiver::Value&&!nominals.copy_safe(target,member.where))
    fail(member.where,"value receiver is not copy-safe","ON1X-IMPL-RECEIVER");

   auto& function=member.function;
   if(function.name!=member.name||!function.type_parameters.empty()||
      !function.constraints.empty()||function.external||function.prototype||
      function.extension_generated||!function.kernel_symbol.empty()||
      function.kernel_link_only||!function.attributes.empty())
    fail(member.where,"malformed hidden inherent member metadata",
         "ON1X-IMPL-HIR");
   std::set<std::string> names;
   for(std::size_t p=0;p<function.params.size();++p){
    auto& parameter=function.params[p];
    if(!names.insert(parameter.name).second)
     fail(parameter.where,"duplicate member parameter: "+parameter.name);
    if(parameter.exclusive)
     fail(parameter.where,"exclusive ref parameters are not supported in inherent members",
          "ON1X-IMPL-RECEIVER");
    if(parameter.name=="self"&&(member.receiver==InherentMember::Receiver::None||p!=0))
     fail(parameter.where,"self is reserved for the first receiver parameter",
          "ON1X-IMPL-RECEIVER");
    check_type(parameter.type,variables,parameter.where,type_budget);
   }
   if(member.receiver!=InherentMember::Receiver::None){
    if(function.params.empty()||function.params.front().name!="self")
     fail(member.where,"receiver member is missing its first self parameter",
          "ON1X-IMPL-RECEIVER");
    if(function.params.front().type!=implementation.target)
     fail(function.params.front().where,
          "self annotation must exactly match the implementation target",
          "ON1X-IMPL-RECEIVER");
   }
   if(member.kind==InherentMember::Kind::Function){
    if(member.value||member.value_type!=Type::Invalid)
     fail(member.where,"function member carries associated-value metadata",
          "ON1X-IMPL-HIR");
    check_type(function.result,variables,member.where,type_budget);
   }else{
    if(!function.body.empty()||function.result!=Type::Void)
     fail(member.where,"associated value carries function metadata",
          "ON1X-IMPL-HIR");
    if(!member.value)fail(member.where,"associated/member value has no initializer");
    check_type(member.value_type,variables,member.where,type_budget);
   }

   const auto identity="@impl_"+std::to_string(i)+"_"+std::to_string(j);
   if(member.runtime_name.empty())member.runtime_name=identity;
   if(!runtime_names.insert(member.runtime_name).second)
    fail(member.where,"private member runtime name collision","ON1X-IMPL-COHERENCE");
   if(++generated>4096||unit.functions.size()+generated>16384)
    fail(member.where,"generated member function budget exhausted","ON1X-IMPL-LIMIT");
  }
 }

 // Constructing the catalog performs the source-order-independent duplicate
 // check before any generated function is published.
 (void)Catalog(unit);
 std::vector<Function> functions;
 functions.reserve(generated);
 for(const auto& implementation:unit.inherent_implementations)
  for(const auto& member:implementation.members)
   functions.push_back(generated_function(implementation,member,clone_budget));
 for(auto& function:functions)unit.functions.push_back(std::move(function));
 unit.inherents_prepared=true;
}

void verify_elaborated(const Unit& unit){
 if(present(unit)&&!unit.inherents_prepared)
  fail(unit.inherent_implementations.front().where,
       "inherent implementations were not prepared before backend admission");
 if(unit.inherents_prepared)verify_generated_members(unit);
 expressions(unit,[&](const Expr& expression){
  if(expression.kind==Expr::Kind::InherentMember)
   fail(expression.where,"unresolved inherent member projection reached checked HIR");
 });
}

} // namespace on1x::inherent

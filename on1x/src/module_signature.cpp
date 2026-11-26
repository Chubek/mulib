#include <on1x/module_signature.hpp>
#include <algorithm>

namespace on1x {
namespace {
[[noreturn]] void mismatch(const InterfaceMember& member,const Function& definition,const Type& actual){
 const auto& at=definition.where;
 auto expected=type_name(member.signature),found=type_name(actual);
 throw Error(member.where,"interface signature mismatch for '"+member.name+"': expected "+expected+
             ", found "+found+" in "+at.file+":"+std::to_string(at.line)+":"+std::to_string(at.column),
             "ON1X-INTERFACE-MISMATCH",expected,found);
}

void require_signature_type(const Type& type,const Location& where,
                            const std::map<std::string,std::size_t>& abstract_types,bool result=false){
 if(type.is_numeric()||type==Type::Bool||type==Type::String||type==Type::Char||
    (result&&type==Type::Void))return;
 if(type.is_tuple()){
  if(type.elements().size()>32)
   throw Error(where,"interface tuple width limit exceeded","ON1X-INTERFACE-LIMIT");
  for(const auto& item:type.elements())require_signature_type(item,where,abstract_types);
  return;
 }
 if(type.kind()==Type::Named){
  auto found=abstract_types.find(type.nominal());
  if(found==abstract_types.end())
   throw Error(where,"interface refers to undeclared abstract type '"+type.nominal()+"'",
               "ON1X-INTERFACE-TYPE");
  if(type.arguments().size()!=found->second)
   throw Error(where,"wrong type argument arity for abstract type '"+type.nominal()+"'",
               "ON1X-INTERFACE-TYPE");
  for(const auto& item:type.arguments())require_signature_type(item,where,abstract_types);
  return;
 }
 throw Error(where,"unsupported abstract module signature type: "+type_name(type),
             "ON1X-INTERFACE-SHAPE");
}
}

SealedModuleSignature verify_module_signature(const ModuleSignature& signature,
                                             const std::vector<const Unit*>& original_fragments,
                                             std::string_view declared_module,
                                             const std::filesystem::path& canonical_implementation){
 if(!signature.module||*signature.module!=declared_module||!canonical_implementation.is_absolute()||
    original_fragments.empty())
  throw Error(signature.where,"abstract module name must match its original module implementation",
              "ON1X-INTERFACE-MODULE",signature.module.value_or(""),std::string(declared_module));
 if(signature.types.empty()&&signature.members.empty())
  throw Error(signature.where,"abstract module requires at least one declaration","ON1X-INTERFACE-SHAPE");
 if(signature.types.size()+signature.members.size()>256)
  throw Error(signature.where,"interface member limit exceeded","ON1X-INTERFACE-LIMIT");
 std::map<std::string,std::vector<const NominalDeclaration*>> nominals;
 std::map<std::string,std::vector<const Function*>> definitions;
 for(const auto* fragment:original_fragments){
  if(!fragment)
   throw Error(signature.where,"missing original module fragment","ON1X-INTERFACE-MODULE");
  for(const auto& nominal:fragment->nominals)nominals[nominal.name].push_back(&nominal);
  for(const auto& function:fragment->functions)definitions[function.name].push_back(&function);
 }

 SealedModuleSignature sealed;
 std::map<std::string,std::size_t> abstract_types;
 for(const auto& specification:signature.types){
  if(!abstract_types.emplace(specification.name,specification.arity).second)
   throw Error(specification.where,"duplicate interface type: "+specification.name,
               "ON1X-INTERFACE-DUPLICATE");
  auto found=nominals.find(specification.name);
  if(found==nominals.end()||found->second.size()!=1||
     (found->second.front()->kind!=Type::Record&&found->second.front()->kind!=Type::Sum))
   throw Error(specification.where,"expected one local nominal definition of abstract type '"+
               specification.name+"'","ON1X-INTERFACE-TYPE");
  const auto& nominal=*found->second.front();
  if(nominal.parameters.size()!=specification.arity)
   throw Error(specification.where,"abstract type arity mismatch for '"+specification.name+"'",
               "ON1X-INTERFACE-TYPE",std::to_string(specification.arity),
               std::to_string(nominal.parameters.size()));
  sealed.types.emplace(specification.name,AbstractTypeIdentity{
       canonical_implementation,*signature.module,specification.name});
 }

 std::set<std::string> members;
 for(const auto& member:signature.members){
  if(!members.insert(member.name).second)
   throw Error(member.where,"duplicate interface member: "+member.name,"ON1X-INTERFACE-DUPLICATE");
  TypeFormOptions options;options.allow_named=true;options.allow_foralls=false;
  auto form=check_type_form(member.signature,options);
  if(!form||!member.signature.is_function())
   throw Error(member.where,"invalid abstract module function signature"+
               (form?std::string{}:": "+form.message),
               form.status==TypeFormStatus::Limit?"ON1X-INTERFACE-LIMIT":"ON1X-INTERFACE-SHAPE");
  if(member.signature.parameters().size()>32)
   throw Error(member.where,"interface parameter limit exceeded","ON1X-INTERFACE-LIMIT");
  for(const auto& parameter:member.signature.parameters())
   require_signature_type(parameter,member.where,abstract_types);
  require_signature_type(member.signature.result(),member.where,abstract_types,true);

  auto found=definitions.find(member.name);
  if(found==definitions.end()||found->second.size()!=1||found->second.front()->external||
     found->second.front()->extension_generated||found->second.front()->prototype||
     found->second.front()->kernel_link_only||!found->second.front()->kernel_symbol.empty())
   throw Error(member.where,"expected one local On1x definition of '"+member.name+
               "' (externs and re-exports do not satisfy interfaces)","ON1X-INTERFACE-MEMBER");
  const auto& definition=*found->second.front();
  if(definition.result.has_holes()||
     std::any_of(definition.params.begin(),definition.params.end(),[](const auto& p){return p.type.has_holes();}))
   throw Error(definition.where,"interface member '"+member.name+
               "' requires a fully annotated signature (no inferred types)","ON1X-INTERFACE-ANNOTATION");
  if(takes_borrows(definition)||definition.procedure||!definition.type_parameters.empty()||
     !definition.constraints.empty())
   throw Error(definition.where,"borrowed, procedure and generic declarations are not supported in abstract module signatures",
               "ON1X-INTERFACE-UNSUPPORTED");
  std::vector<Type> parameters;parameters.reserve(definition.params.size());
  for(const auto& parameter:definition.params)parameters.push_back(parameter.type);
  auto actual=Type::function(std::move(parameters),definition.result);
  if(member.signature!=actual)mismatch(member,definition,actual);
 }
 return sealed;
}

SealedModuleSignature verify_module_signature(const ModuleSignature& signature,const Unit& original,
                                             std::string_view declared_module,
                                             const std::filesystem::path& canonical_implementation){
 return verify_module_signature(signature,std::vector<const Unit*>{&original},declared_module,
                                canonical_implementation);
}

void require_executable_module_signature(const ModuleSignature& signature){
 if(!signature.types.empty())
  throw Error(signature.types.front().where,"abstract types are declaration-only: opaque module types cannot enter executable HIR",
              "ON1X-INTERFACE-UNSUPPORTED");
}
}

#include <on1x/module_signature.hpp>
#include <algorithm>

namespace on1x {
namespace {
std::string callable_name(const Type& type,bool procedure){
 auto name=type_name(type);
 if(procedure)name.replace(0,2,"proc");
 return name;
}

[[noreturn]] void mismatch(const ModuleSignatureMember& member,const Function& definition,const Type& actual){
 const auto& at=definition.where;
 auto expected=callable_name(member.signature,member.procedure);
 auto found=callable_name(actual,definition.procedure);
 throw Error(member.where,"interface signature mismatch for '"+member.name+"': expected "+expected+
             ", found "+found+" in "+at.file+":"+std::to_string(at.line)+":"+std::to_string(at.column),
             "ON1X-INTERFACE-MISMATCH",expected,found);
}

bool scalar(const Type& t){
 return t.is_numeric()||t==Type::Bool||t==Type::String||t==Type::Char;
}
void require_form(const Type& type,const Location& where,std::size_t variables=0){
 TypeFormOptions options;
 options.allow_named=true;options.allow_foralls=false;options.bound_variables=variables;
 auto form=check_type_form(type,options);
 if(!form)throw Error(where,"invalid module interface type: "+form.message,
                      form.status==TypeFormStatus::Limit?"ON1X-INTERFACE-LIMIT":"ON1X-INTERFACE-SHAPE");
}
void visit_node(const Location& where,std::size_t& nodes,std::size_t depth){
 if(!nodes||depth>128)
  throw Error(where,"signature type expansion budget exceeded","ON1X-INTERFACE-LIMIT");
 --nodes;
}
template<class Transform>
std::vector<Type> transform_types(const std::vector<Type>& types,Transform transform){
 std::vector<Type> result;result.reserve(types.size());
 for(const auto& t:types)result.push_back(transform(t));
 return result;
}

// Signature parameters range over closed scalar/tuple types. In particular,
// an include cannot capture a type name from another signature's scope.
bool closed_argument(const Type& t){
 return scalar(t)||(t.is_tuple()&&t.elements().size()<=32&&
                    std::all_of(t.elements().begin(),t.elements().end(),closed_argument));
}

Type substitute(const Type& t,const std::vector<Type>& arguments,const Location& where,
                std::size_t& nodes,std::size_t depth=0){
 visit_node(where,nodes,depth);
 const auto child=[&](const Type& part){return substitute(part,arguments,where,nodes,depth+1);};
 if(t.kind()==Type::Bound){
  if(t.generic_identity()>=arguments.size())
   throw Error(where,"unbound signature type parameter","ON1X-INTERFACE-TYPE");
  // Charge each occurrence, not only the shared Type handle.
  return substitute(arguments[t.generic_identity()],{},where,nodes,depth+1);
 }
 if(t.kind()==Type::Named)return Type::named(t.nominal(),transform_types(t.arguments(),child));
 if(t.is_tuple())return Type::tuple(transform_types(t.elements(),child));
 if(t.is_function())return Type::function(transform_types(t.parameters(),child),child(t.result()));
 return t;
}

struct TypeMember {
 std::size_t arity=0;
 const Type* equation=nullptr;
};
using TypeMembers=std::map<std::string,TypeMember>;

// Normalize aliases without ever walking through nominal layout, even for
// transparent ascription. Named leaves retain their provider-local spelling
// and must be interpreted with the verified provider identity.
Type normalize(const Type& t,const TypeMembers& types,const Location& where,
               std::set<std::string>& active,std::size_t& nodes,std::size_t depth,bool result){
 visit_node(where,nodes,depth);
 if(scalar(t)||(result&&t==Type::Void))return t;
 const auto child=[&](const Type& part){return normalize(part,types,where,active,nodes,depth+1,false);};
 if(t.is_tuple()){
  if(t.elements().size()>32)
   throw Error(where,"interface tuple width limit exceeded","ON1X-INTERFACE-LIMIT");
  return Type::tuple(transform_types(t.elements(),child));
 }
 if(t.kind()==Type::Named){
  auto found=types.find(t.nominal());
  if(found==types.end())
   throw Error(where,"interface refers to undeclared type '"+t.nominal()+"'","ON1X-INTERFACE-TYPE");
  if(t.arguments().size()!=found->second.arity)
   throw Error(where,"wrong type argument arity for '"+t.nominal()+"'","ON1X-INTERFACE-TYPE");
  if(!found->second.equation)return Type::named(t.nominal(),transform_types(t.arguments(),child));
  if(!active.insert(t.nominal()).second)
   throw Error(where,"cyclic type equation for '"+t.nominal()+"'","ON1X-INTERFACE-CYCLE");
  auto value=child(*found->second.equation);
  active.erase(t.nominal());return value;
 }
 throw Error(where,"unsupported module interface type: "+type_name(t),"ON1X-INTERFACE-SHAPE");
}
Type normalize_equation(const Type& t,const TypeMembers& types,const Location& where,
                        std::size_t& remaining){
 std::set<std::string> active;
 auto nodes=std::min<std::size_t>(16384,remaining);
 const auto initial=nodes;
 auto equation=normalize(t,types,where,active,nodes,0,false);
 remaining-=initial-nodes;
 return equation;
}
Type normalize_callable(const Type& callable,const TypeMembers& types,const Location& where){
 std::set<std::string> active;std::size_t nodes=16384;
 auto parameters=transform_types(callable.parameters(),[&](const Type& type){
  return normalize(type,types,where,active,nodes,1,false);
 });
 return Type::function(std::move(parameters),normalize(callable.result(),types,where,active,nodes,1,true));
}

class SignatureExpansion {
 const ModuleSignature& source;
 ModuleSignature flat;
 std::map<std::string,const NamedSignature*> definitions;
 std::map<std::string,std::size_t> types;
 std::set<std::string> members,active,reached;
 std::vector<SignatureSharing> sharing;
 std::size_t remaining=256,nodes=16384;

 void spend(const Location& where){
  if(!remaining)throw Error(where,"signature expansion limit exceeded","ON1X-INTERFACE-LIMIT");
  --remaining;
 }
 void include(const SignatureReference& reference,const std::vector<Type>& bindings){
  spend(reference.where);
  auto found=definitions.find(reference.name);
  if(found==definitions.end())
   throw Error(reference.where,"unknown signature '"+reference.name+"'","ON1X-INTERFACE-SIGNATURE");
  if(!active.insert(reference.name).second)
   throw Error(reference.where,"recursive signature inclusion of '"+reference.name+"'","ON1X-INTERFACE-CYCLE");
  if(active.size()>32)
   throw Error(reference.where,"signature inclusion depth exceeded","ON1X-INTERFACE-LIMIT");
  reached.insert(reference.name);
  const auto& definition=*found->second;
  if(reference.arguments.size()!=definition.parameters.size())
   throw Error(reference.where,"wrong argument count for signature '"+reference.name+"'","ON1X-INTERFACE-ARITY");
  const auto arguments=transform_types(reference.arguments,[&](const Type& argument){
   require_form(argument,reference.where,bindings.size());
   auto closed=substitute(argument,bindings,reference.where,nodes);
   if(!closed_argument(closed))
    throw Error(reference.where,"signature arguments must be closed scalar or tuple types","ON1X-INTERFACE-UNSUPPORTED");
   return closed;
  });
  const auto instantiate=[&](const Type& t,const Location& where){
   require_form(t,where,arguments.size());return substitute(t,arguments,where,nodes);
  };
  for(const auto& reference:definition.includes)include(reference,arguments);
  for(auto spec:definition.types){
   spend(spec.where);
   if(!types.emplace(spec.name,flat.types.size()).second)
    throw Error(spec.where,"duplicate included interface type: "+spec.name,"ON1X-INTERFACE-DUPLICATE");
   if(spec.manifest)spec.manifest=instantiate(*spec.manifest,spec.where);
   flat.types.push_back(std::move(spec));
  }
  for(auto member:definition.members){
   spend(member.where);
   if(!members.insert(member.name).second)
    throw Error(member.where,"duplicate included interface member: "+member.name,"ON1X-INTERFACE-DUPLICATE");
   member.signature=instantiate(member.signature,member.where);
   flat.members.push_back(std::move(member));
  }
  for(auto equation:definition.sharing){
   spend(equation.where);equation.equation=instantiate(equation.equation,equation.where);
   sharing.push_back(std::move(equation));
  }
  active.erase(reference.name);
 }

public:
 explicit SignatureExpansion(const ModuleSignature& signature):source(signature){}
 ModuleSignature run(){
  if(!source.module||!source.ascription||!source.types.empty()||!source.members.empty())
   throw Error(source.where,"invalid named module ascription","ON1X-INTERFACE-SHAPE");
  if(source.definitions.size()>64)
   throw Error(source.where,"named signature limit exceeded","ON1X-INTERFACE-LIMIT");
  for(const auto& def:source.definitions){
   if(!definitions.emplace(def.name,&def).second)
    throw Error(def.where,"duplicate named signature: "+def.name,"ON1X-INTERFACE-DUPLICATE");
   if(def.parameters.size()>32)
    throw Error(def.where,"signature parameter limit exceeded","ON1X-INTERFACE-LIMIT");
  }
  flat.where=source.where;flat.module=source.module;flat.seal=source.seal;
  include(*source.ascription,{});
  for(const auto& [name,def]:definitions){
   if(!reached.contains(name))
    throw Error(def->where,"unused signature definition '"+name+"'","ON1X-INTERFACE-SIGNATURE");
  }
  for(auto& equation:sharing){
   auto found=types.find(equation.member);
   if(found==types.end())
    throw Error(equation.where,"sharing requires a declared type '"+equation.member+"'","ON1X-INTERFACE-TYPE");
   auto& member=flat.types[found->second];
   if(member.arity||member.manifest)
    throw Error(equation.where,"sharing requires one unrefined monomorphic type '"+equation.member+"'",
                "ON1X-INTERFACE-TYPE");
   member.manifest=std::move(equation.equation);
  }
  return std::move(flat);
 }
};

class CopyRepresentation {
 const std::map<std::string,std::vector<const NominalDeclaration*>>& declarations;
 const Location& where;
 std::set<std::string> checking,complete;
 std::size_t nodes=16384;

 [[noreturn]] void unsupported(const std::string& message)const{
  throw Error(where,message,"ON1X-INTERFACE-UNSUPPORTED");
 }
 void field(const Type& type,std::size_t depth){
  visit_node(where,nodes,depth);
  if(scalar(type)&&type!=Type::String)return;
  if(type.kind()==Type::Named&&type.arguments().empty()){
   nominal(type.nominal(),depth+1);return;
  }
  if(type.is_tuple()||type.kind()==Type::Record||type.kind()==Type::Sum||
     (type.kind()==Type::Array&&type.extent()<=256&&type.elements().size()==1)){
   for(const auto& element:type.elements())field(element,depth+1);
   return;
  }
  unsupported("opaque executable types require a closed copy-value representation; found "+type_name(type));
 }
public:
 CopyRepresentation(const std::map<std::string,std::vector<const NominalDeclaration*>>& declarations,
                    const Location& where):declarations(declarations),where(where){}
 void nominal(const std::string& name,std::size_t depth=0){
  visit_node(where,nodes,depth);
  if(complete.contains(name))return;
  auto found=declarations.find(name);
  if(found==declarations.end()||found->second.size()!=1)
   unsupported("opaque representation requires a unique local nominal '"+name+"'");
  const auto& declaration=*found->second.front();
  if(!declaration.parameters.empty()||(declaration.kind!=Type::Record&&declaration.kind!=Type::Sum))
   unsupported("opaque executable types require monomorphic record or enum declarations");
  if(!checking.insert(name).second)
   unsupported("recursive opaque executable representation for '"+name+"'");
  for(const auto& type:declaration.fields)field(type,depth+1);
  checking.erase(name);complete.insert(name);
 }
};
}

ModuleSignature resolve_module_signature(const ModuleSignature& signature){
 if(!signature.ascription&&signature.definitions.empty())return signature;
 return SignatureExpansion(signature).run();
}

SealedModuleSignature verify_module_signature(const ModuleSignature& signature,
                                              const std::vector<const Unit*>& original_fragments,
                                              std::string_view declared_module,
                                              const std::filesystem::path& canonical_implementation){
 if(signature.ascription||!signature.definitions.empty())
  return verify_module_signature(resolve_module_signature(signature),original_fragments,declared_module,
                                 canonical_implementation);
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
 std::map<std::string,const SignatureTypeAlias*> aliases;
 std::map<std::string,Type> normalized_aliases;
 for(const auto* fragment:original_fragments){
  if(!fragment)
   throw Error(signature.where,"missing original module fragment","ON1X-INTERFACE-MODULE");
  for(const auto& nominal:fragment->nominals)nominals[nominal.name].push_back(&nominal);
  for(const auto& function:fragment->functions)definitions[function.name].push_back(&function);
  for(const auto& alias:fragment->signature_type_aliases){
   if(!aliases.emplace(alias.name,&alias).second)
    throw Error(alias.where,"duplicate original type equation: "+alias.name,"ON1X-INTERFACE-DUPLICATE");
   if(aliases.size()>256)
    throw Error(alias.where,"original type equation limit exceeded","ON1X-INTERFACE-LIMIT");
  }
 }

 SealedModuleSignature sealed;
 TypeMembers expected_types,actual_types;
 if(!aliases.empty()){
  for(const auto& [name,declarations]:nominals)
   if(declarations.size()==1)
    actual_types.emplace(name,TypeMember{declarations.front()->parameters.size()});
  for(const auto& [name,alias]:aliases){
   if(nominals.contains(name))
    throw Error(alias->where,"type equation conflicts with original nominal '"+name+"'",
                "ON1X-INTERFACE-DUPLICATE");
   require_form(alias->equation,alias->where);
   actual_types.emplace(name,TypeMember{0,&alias->equation});
  }
  std::size_t remaining=65536;
  for(const auto& [name,alias]:aliases)
   normalized_aliases.emplace(name,normalize_equation(alias->equation,actual_types,alias->where,remaining));
 }
 for(const auto& specification:signature.types){
  if(!expected_types.emplace(specification.name,TypeMember{
        specification.arity,specification.manifest?&*specification.manifest:nullptr}).second)
   throw Error(specification.where,"duplicate interface type: "+specification.name,
               "ON1X-INTERFACE-DUPLICATE");
  if(specification.manifest){
   if(specification.arity)
    throw Error(specification.where,"manifest type equations must be monomorphic","ON1X-INTERFACE-UNSUPPORTED");
   require_form(*specification.manifest,specification.where);
   continue;
  }
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
  if(signature.seal==SignatureSeal::Opaque)
   sealed.types.emplace(specification.name,AbstractTypeIdentity{
      canonical_implementation,*signature.module,specification.name});
  else
   sealed.transparent_types.emplace(specification.name,TransparentTypeIdentity{
      canonical_implementation,*signature.module,specification.name});
 }
 std::size_t remaining=65536;
 for(const auto& specification:signature.types){
  if(!specification.manifest)continue;
  auto expected=normalize_equation(*specification.manifest,expected_types,specification.where,remaining);
  auto actual=normalized_aliases.find(specification.name);
  if(actual==normalized_aliases.end())
   throw Error(specification.where,"expected one local type equation for '"+specification.name+"'",
               "ON1X-INTERFACE-TYPE");
  if(expected!=actual->second)
   throw Error(specification.where,"interface type equation mismatch for '"+specification.name+"'",
               "ON1X-INTERFACE-MISMATCH",type_name(expected),type_name(actual->second));
  sealed.manifest_types.emplace(specification.name,std::move(expected));
 }

 std::set<std::string> members;
 for(const auto& member:signature.members){
  if(!members.insert(member.name).second)
   throw Error(member.where,"duplicate interface member: "+member.name,"ON1X-INTERFACE-DUPLICATE");
  require_form(member.signature,member.where);
  if(!member.signature.is_function()||
     (member.procedure&&member.signature.result()!=Type::Void))
   throw Error(member.where,"invalid module interface callable","ON1X-INTERFACE-SHAPE");
  if(member.signature.parameters().size()>32)
   throw Error(member.where,"interface parameter limit exceeded","ON1X-INTERFACE-LIMIT");
  auto expected=normalize_callable(member.signature,expected_types,member.where);

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
  if(takes_borrows(definition)||!definition.type_parameters.empty()||
     !definition.constraints.empty())
   throw Error(definition.where,"borrowed and generic declarations are not supported in abstract module signatures",
               "ON1X-INTERFACE-UNSUPPORTED");
  std::vector<Type> parameters;parameters.reserve(definition.params.size());
  for(const auto& parameter:definition.params)parameters.push_back(parameter.type);
  auto actual=Type::function(std::move(parameters),definition.result);
  if(!aliases.empty()){
   auto resolved=normalize_callable(actual,actual_types,definition.where);
   if(member.procedure!=definition.procedure||expected!=resolved){
    auto obligation=member;obligation.signature=std::move(expected);
    mismatch(obligation,definition,resolved);
   }
  }else if(member.procedure!=definition.procedure||expected!=actual)mismatch(member,definition,actual);
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
 if(signature.ascription||!signature.definitions.empty()){
  require_executable_module_signature(resolve_module_signature(signature));return;
 }
 for(const auto& type:signature.types)
  if(type.manifest||signature.seal==SignatureSeal::Transparent)
   throw Error(type.where,"manifest and transparent type views are declaration-only","ON1X-INTERFACE-UNSUPPORTED");
 if(!signature.types.empty())
  throw Error(signature.types.front().where,"abstract types are declaration-only: opaque module types cannot enter executable HIR",
              "ON1X-INTERFACE-UNSUPPORTED");
}

std::set<std::string> admit_executable_opaque_signature(
    const ModuleSignature& signature,const SealedModuleSignature& verified,
    const std::vector<const Unit*>& originals,std::string_view declared_module,
    const std::filesystem::path& canonical_implementation){
 if(signature.ascription||!signature.definitions.empty())
  return admit_executable_opaque_signature(resolve_module_signature(signature),verified,originals,
                                           declared_module,canonical_implementation);
 if(signature.types.empty()){
  require_executable_module_signature(signature);
  return {};
 }
 if(!signature.module||*signature.module!=declared_module||!canonical_implementation.is_absolute()||originals.empty())
  throw Error(signature.where,"opaque permit requires its canonical original provider","ON1X-INTERFACE-MODULE");
 if(!verified.manifest_types.empty())
  throw Error(signature.where,"manifest type views are declaration-only","ON1X-INTERFACE-UNSUPPORTED");
 std::map<std::string,std::vector<const NominalDeclaration*>> declarations;
 for(const auto* original:originals){
  if(!original)
   throw Error(signature.where,"opaque permit requires original module fragments","ON1X-INTERFACE-MODULE");
  if(!original->signature_type_aliases.empty())
   throw Error(original->signature_type_aliases.front().where,"module type equations are declaration-only",
               "ON1X-INTERFACE-UNSUPPORTED");
  for(const auto& declaration:original->nominals)declarations[declaration.name].push_back(&declaration);
 }
 if(signature.seal==SignatureSeal::Transparent){
  if(!verified.types.empty()||verified.transparent_types.size()!=signature.types.size())
   throw Error(signature.where,"transparent permit does not match the verified signature","ON1X-INTERFACE-MODULE");
  for(const auto& member:signature.types){
   auto identity=verified.transparent_types.find(member.name);
   if(identity==verified.transparent_types.end()||
      identity->second!=TransparentTypeIdentity{canonical_implementation,*signature.module,member.name})
    throw Error(member.where,"transparent permit requires the original provider","ON1X-INTERFACE-MODULE");
   const auto found=declarations.find(member.name);
   if(found==declarations.end()||found->second.size()!=1)
    throw Error(member.where,"transparent permit requires one local declaration","ON1X-INTERFACE-TYPE");
   const auto& nominal=*found->second.front();
   if(member.arity||member.manifest||!nominal.parameters.empty()||nominal.kind!=Type::Record||
      !std::all_of(nominal.fields.begin(),nominal.fields.end(),[](const Type& t){return scalar(t)&&t!=Type::String;}))
    throw Error(member.where,"executable transparent types require monomorphic records with scalar copy fields",
                "ON1X-INTERFACE-UNSUPPORTED");
  }
  return {}; // Ordinary provider-qualified nominal, never install an opaque seal.
 }
 if(!verified.transparent_types.empty())
  throw Error(signature.where,"opaque permit contains transparent identities","ON1X-INTERFACE-MODULE");
 std::set<std::string> names;
 for(const auto& member:signature.types){
  if(member.manifest||member.arity)
   throw Error(member.where,"opaque executable type members must be abstract and monomorphic",
               "ON1X-INTERFACE-UNSUPPORTED");
  auto found=verified.types.find(member.name);
  if(found==verified.types.end()||
     found->second!=AbstractTypeIdentity{canonical_implementation,*signature.module,member.name}||
     !names.insert(member.name).second)
   throw Error(member.where,"opaque permit does not match the verified provider identity","ON1X-INTERFACE-MODULE");
  CopyRepresentation(declarations,member.where).nominal(member.name);
 }
 if(names.size()!=verified.types.size())
  throw Error(signature.where,"opaque permit does not match the verified signature","ON1X-INTERFACE-MODULE");
 return names;
}
}

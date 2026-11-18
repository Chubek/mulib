#include <on1x/extensions.hpp>
#include "extensions_registry.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace on1x::extensions {
namespace {
using namespace meta;
[[noreturn]] void reject(const Location& at,const std::string& message){
 throw Error(at,"ON1X-CAT-EVIDENCE: "+message,"ON1X-CAT-EVIDENCE");
}
SExpr loc(const Location& at){
 return list({symbol("Loc"),string(at.file),symbol(std::to_string(at.offset)),
              symbol(std::to_string(at.line)),symbol(std::to_string(at.column))});
}
bool scalar(const Type& type){return type.is_numeric()||type==Type::Bool;}
bool hex_digest(const std::string& value){
 return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
Type signature(const Function& function){
 std::vector<Type> params;for(const auto& param:function.params)params.push_back(param.type);
 return Type::function(std::move(params),function.result);
}
// Generic binder substitution, deliberately restricted to a first-order scalar
// interface. No protocol names, instance search or Category grammar live here.
Type instantiate(const Type& scheme,const std::vector<Type>& arguments,const Location& at){
 if(!scheme.is_forall()||scheme.generic_identity()!=arguments.size()||
    arguments.empty()||arguments.size()>16||!scheme.body().is_function())
  reject(at,"invalid interface binder count or member scheme");
 for(const auto& arg:arguments)if(!scalar(arg))reject(at,"non-scalar evidence argument");
 auto leaf=[&](const Type& type,bool result){
  if(type.kind()==Type::Bound){
   if(type.generic_identity()>=arguments.size())reject(at,"unbound interface type");
   return arguments[type.generic_identity()];
  }
  if(!scalar(type)&&!(result&&type==Type::Void))reject(at,"unsupported interface type");
  return type;
 };
 std::vector<Type> params;
 if(scheme.body().parameters().size()>16)reject(at,"member arity budget exhausted");
 for(const auto& p:scheme.body().parameters())params.push_back(leaf(p,false));
 return Type::function(std::move(params),leaf(scheme.body().result(),true));
}
template<class T> std::vector<const T*> sorted(const std::vector<T>& values){
 std::vector<const T*> result;for(const auto& value:values)result.push_back(&value);
 std::sort(result.begin(),result.end(),[](auto a,auto b){return a->name<b->name;});
 return result;
}
std::vector<const ExtensionBinding*> bindings(const ExtensionImplementation& impl){
 std::vector<const ExtensionBinding*> result;for(const auto& b:impl.bindings)result.push_back(&b);
 std::sort(result.begin(),result.end(),[](auto a,auto b){return a->member<b->member;});
 return result;
}
using Functions=std::map<std::string,const Function*>;
Functions functions(const Unit& unit){
 Functions result;
 for(const auto& f:unit.functions)if(!result.emplace(f.name,&f).second)reject(f.where,"duplicate function");
 return result;
}
void forwarding(const Function& selector,const Function& target){
 auto fail=[&](){reject(selector.where,"selector no longer forwards to its declared implementation: "+selector.name);};
 if(selector.body.size()!=1)fail();
 const auto& statement=selector.body.front();
 if(statement.kind!=(selector.result==Type::Void?Stmt::Kind::Expr:Stmt::Kind::Return)||!statement.value)fail();
 const auto& call=*statement.value;
 if(call.kind!=Expr::Kind::Call||!call.callee||call.callee->kind!=Expr::Kind::Variable||
    call.callee->reference!=Expr::Reference::Function||call.callee->text!=target.name||
    call.callee->type!=signature(target)||call.type!=selector.result||
    call.children.size()!=selector.params.size())fail();
 for(std::size_t i=0;i<call.children.size();++i){
  if(!call.children[i])fail();
  const auto& arg=*call.children[i];
  if(arg.kind!=Expr::Kind::Variable||arg.reference!=Expr::Reference::Local||
     arg.text!=selector.params[i].name||arg.type!=selector.params[i].type)fail();
 }
}
SExpr requests(const ExtensionEvidence& evidence,bool contracts){
 std::vector<SExpr> result;
 if(contracts){
  for(const auto* c:sorted(evidence.contracts)){
   std::vector<SExpr> members;
   for(const auto* m:sorted(c->members))members.push_back(list({symbol("EvidenceMember"),string(m->name),encode_type(m->scheme),loc(m->where)}));
   result.push_back(list({symbol("EvidenceContract"),string(c->name),list(std::move(members)),loc(c->where)}));
  }
 }else{
  for(const auto* impl:sorted(evidence.implementations)){
   std::vector<SExpr> args,entries;
   for(const auto& a:impl->arguments)args.push_back(encode_type(a));
   for(const auto* b:bindings(*impl))entries.push_back(list({symbol("EvidenceBinding"),string(b->member),string(b->function_id),string(b->selector_id),loc(b->where)}));
   result.push_back(list({symbol("EvidenceImplementation"),string(impl->name),string(impl->contract),
     list(std::move(args)),symbol(impl->canonical?"true":"false"),list(std::move(entries)),loc(impl->where)}));
  }
 }
 return list(std::move(result));
}
SExpr choices(const ExtensionEvidence& evidence,const Functions& fs){
 std::vector<SExpr> result;
 for(const auto* impl:sorted(evidence.implementations))for(const auto* b:bindings(*impl))
  result.push_back(list({symbol("EvidenceChoice"),string(impl->name),string(b->member),
    string(b->function_id),string(b->selector_id),encode_type(signature(*fs.at(b->function)))}));
 return list(std::move(result));
}
}

void verify_evidence(const Unit& unit){
 if(unit.extension_evidence.empty()){
  for(const auto& f:unit.functions)if(f.extension_generated)reject(f.where,"generated function is absent from evidence: "+f.name);
  return;
 }
 auto fs=functions(unit);
 std::set<std::pair<std::string,std::string>> modules;
 std::set<std::string> assigned,identities;
 std::size_t count=0;
 if(unit.extension_evidence.size()>1024||unit.source_digests.size()>1024)reject({"<evidence>"},"module budget exhausted");
 for(const auto& [origin,hash]:unit.source_digests)
  if(origin.empty()||!hex_digest(hash))reject({origin},"invalid source fingerprint");
 for(const auto& evidence:unit.extension_evidence){
  const Location at{evidence.origin};
  if(!modules.emplace(evidence.origin,evidence.owner).second)reject(at,"duplicate module evidence");
  if(evidence.owner.empty()||!unit.source_digests.contains(evidence.origin)||
     !hex_digest(evidence.compiler_hash)||!hex_digest(evidence.binary_hash)||
     !hex_digest(evidence.schema_hash)||!hex_digest(evidence.registry_hash))
   reject(at,"missing evidence provenance");
  if(evidence.contracts.size()+evidence.implementations.size()>64)reject(at,"declaration budget exhausted");
  std::map<std::string,const ExtensionContract*> contracts;
  for(const auto& c:evidence.contracts){
   if(c.where.file!=evidence.origin||!contracts.emplace(c.name,&c).second||c.members.empty()||c.members.size()>16)
    reject(c.where,"duplicate/empty interface or incorrect module origin");
   std::set<std::string> members;std::size_t arity=0;
   for(const auto& m:c.members){
    if(m.where.file!=evidence.origin||!members.insert(m.name).second)reject(m.where,"duplicate member or incorrect module origin");
    if(!m.scheme.is_forall()||!m.scheme.generic_identity()||m.scheme.generic_identity()>16)reject(m.where,"invalid interface scheme");
    if(arity&&arity!=m.scheme.generic_identity())reject(m.where,"inconsistent interface binder count");
    arity=m.scheme.generic_identity();
    (void)instantiate(m.scheme,std::vector<Type>(arity,Type::I64),m.where);
   }
  }
  std::set<std::string> names;std::set<std::pair<std::string,std::vector<Type>>> heads;
  for(const auto& impl:evidence.implementations){
   if(impl.where.file!=evidence.origin||!names.insert(impl.name).second)reject(impl.where,"duplicate implementation or incorrect module origin");
   auto hit=contracts.find(impl.contract);
   if(hit==contracts.end())reject(impl.where,"interface is not defined in this module; imported evidence is unsupported");
   if(impl.canonical&&!heads.emplace(impl.contract,impl.arguments).second)reject(impl.where,"overlapping canonical evidence");
   const auto& c=*hit->second;
   if(impl.bindings.size()!=c.members.size())reject(impl.where,"incomplete implementation");
   std::set<std::string> seen;
   for(const auto& b:impl.bindings){
    if(++count>8192)reject(b.where,"evidence binding budget exhausted");
    if(b.where.file!=evidence.origin||!seen.insert(b.member).second)reject(b.where,"duplicate binding or incorrect module origin");
    auto member=std::find_if(c.members.begin(),c.members.end(),[&](const auto& m){return m.name==b.member;});
    if(member==c.members.end())reject(b.where,"unknown interface member");
    const auto expected=instantiate(member->scheme,impl.arguments,b.where);
    for(const auto& [name,id]:{std::pair{b.function,b.function_id},std::pair{b.selector,b.selector_id}}){
     auto f=fs.find(name);
     if(f==fs.end()||!f->second->extension_generated||f->second->external||f->second->prototype||
        f->second->where.file!=evidence.origin||!assigned.insert(name).second)
      reject(b.where,"missing, reused or nonlocal evidence function: "+name);
     if(!id.starts_with(evidence.origin+"::function::")||id.size()<=evidence.origin.size()+12||!identities.insert(id).second)
      reject(b.where,"invalid evidence function identity");
     if(signature(*f->second)!=expected)reject(b.where,"member type mismatch: "+impl.name+"."+b.member+" requires "+type_name(expected));
     for(const auto& p:f->second->params)if(p.pattern)reject(b.where,"pattern parameters are not admitted evidence");
    }
    forwarding(*fs.at(b.selector),*fs.at(b.function));
   }
  }
 }
 // Erasing a sideband batch while leaving generated code cannot waive admission.
 for(const auto& f:unit.functions)if(f.extension_generated&&!assigned.contains(f.name))
  reject(f.where,"generated function is absent from evidence: "+f.name);
}

meta::SExpr evidence_snapshot(const Unit& unit,const ExtensionEvidence& evidence){
 if(std::none_of(unit.extension_evidence.begin(),unit.extension_evidence.end(),
                 [&](const auto& retained){return &retained==&evidence;}))
  reject({evidence.origin},"snapshot requires a host-retained evidence batch");
 verify_evidence(unit);auto fs=functions(unit);std::map<std::string,Type> typed;
 for(const auto& impl:evidence.implementations)for(const auto& b:impl.bindings){
  typed.emplace(b.function_id,signature(*fs.at(b.function)));
  typed.emplace(b.selector_id,signature(*fs.at(b.selector)));
 }
 std::vector<SExpr> facts,sources;
 for(const auto& [name,type]:typed)facts.push_back(list({symbol("TypedFunction"),string(name),encode_type(type)}));
 for(const auto& [origin,hash]:unit.source_digests)sources.push_back(list({string(origin),string(hash)}));
 auto snapshot=list({symbol("EvidenceSnapshot"),symbol("1"),string(""),string(evidence.origin),
   requests(evidence,true),requests(evidence,false),list(std::move(facts))});
 auto world=list({symbol("on1x.evidence-world/v1"),string(on1x_extension_host),
   string(evidence.owner),string(evidence.version),string(evidence.compiler_hash),
   string(evidence.registry_hash),string(evidence.binary_hash),string(evidence.schema_hash),
   list(std::move(sources)),snapshot});
 snapshot.items[2]=string(digest(write_sexpr(world)));
 base_schema().validate(snapshot,"EvidenceInput");
 return snapshot;
}

void admit_evidence_plan(const Unit& unit,const ExtensionEvidence& evidence,const meta::SExpr& plan){
 base_schema().validate(plan,"EvidenceOutput");
 const auto snapshot=evidence_snapshot(unit,evidence);
 if(plan.items[0].text!="EvidencePlan"||plan.items[2].text!=snapshot.items[2].text)
  reject({evidence.origin},"stale or foreign typed snapshot");
 // No AST/typed-fact edits or novel evidence are authorized by a response.
 if(write_sexpr(plan.items[3])!=write_sexpr(choices(evidence,functions(unit))))
  reject({evidence.origin},"missing, reordered or forged evidence choice");
}
}

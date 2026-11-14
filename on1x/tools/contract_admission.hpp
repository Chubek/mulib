#pragma once
#include "contract_json.hpp"
#include <exoml/CDecl.hpp>
#include <exoml/Contract.hpp>
#include <algorithm>
#include <filesystem>
#include <set>
namespace on1x::contracts {
namespace cdecl=moonstone::exoml::cdecl;
namespace contract=moonstone::exoml::contract;
struct Rejected : std::runtime_error {using std::runtime_error::runtime_error;};
inline void require(bool okay,const std::string& why){if(!okay)throw Rejected(why);}
inline std::string text(const json::Value& value){require(value.kind=='s'&&value.text.find('\0')==std::string::npos,"expected NUL-free JSON string");return value.text;}
inline void object(const json::Value& value,std::initializer_list<const char*> keys){
 require(value.kind=='o',"expected contract object");std::set<std::string> known;for(auto key:keys)known.insert(key);
 for(const auto& [key,child]:value.object){(void)child;require(known.contains(key),"unknown contract key: "+key);}
}
inline void fields(const json::Value& value,std::initializer_list<const char*> strings,std::initializer_list<const char*> booleans,std::initializer_list<const char*> indices={}){
 for(auto field:strings)if(value.has(field))text(value.at(field));
 for(auto field:booleans)if(value.has(field))require(value.at(field).kind=='b',std::string("expected boolean: ")+field);
 for(auto field:indices)if(value.has(field)&&value.at(field).kind!='n')json::natural(value.at(field));
}
inline void array(const json::Value& value,std::size_t maximum=4096){require(value.kind=='a'&&value.array.size()<=maximum,"invalid/oversized contract array");}
inline void lifetime(const json::Value& value){
 object(value,{"origin","parameter_index","region_name"});auto origin=text(value.at("origin"));fields(value,{"region_name"},{},{"parameter_index"});
 if(origin=="parameter"||origin=="caller_buffer")require(value.has("parameter_index")&&value.at("parameter_index").kind!='n',"borrow origin requires an explicit parameter_index");
}
inline void provenance(const json::Value& value){
 object(value,{"header","revision","source_location","author","review_status","evidence_method","toolchain_fingerprint"});
 for(auto key:{"header","author","review_status","evidence_method"})require(!text(value.at(key)).empty(),std::string("missing contract provenance: ")+key);
 fields(value,{"revision","toolchain_fingerprint"},{});
 if(value.has("source_location")){const auto& loc=value.at("source_location");object(loc,{"file","line","column","end_line","end_column"});fields(loc,{"file"},{},{"line","column","end_line","end_column"});}
}
inline void validate_shape(const json::Value& root){
 object(root,{"schema_version","library","headers","functions","bundle_provenance","state"});
 require(json::natural(root.at("schema_version"))==2,"unsupported contract schema version");require(!text(root.at("library")).empty(),"missing contract library");text(root.at("state"));
 array(root.at("headers"),128);require(!root.at("headers").array.empty(),"missing contract headers");for(const auto& h:root.at("headers").array)text(h);
 provenance(root.at("bundle_provenance"));array(root.at("functions"));require(!root.at("functions").array.empty(),"empty contract bundle");
 for(const auto& f:root.at("functions").array){
  object(f,{"c_name","return","parameters","errors","effects","concurrency","callbacks","destructors","transfers","invalidations","provenance"});
  require(!text(f.at("c_name")).empty(),"missing contract function");provenance(f.at("provenance"));
  const auto& r=f.at("return");object(r,{"kind","nullable","lifetime","destroy_with","allocation_source","deallocator_family","nullable_means_success","alias_of"});text(r.at("kind"));require(r.at("nullable").kind=='b',"return nullable must be explicit");fields(r,{"destroy_with","allocation_source","deallocator_family","alias_of"},{"nullable_means_success"});if(r.has("lifetime"))lifetime(r.at("lifetime"));
  // ExoML's canonical serializer omits an empty parameter vector. A nonzero
  // declaration still fails the count/index checks below; no scalar is guessed.
  json::Value empty;empty.kind='a';const auto& parameters=f.has("parameters")?f.at("parameters"):empty;array(parameters,128);
  for(const auto& p:parameters.array){
   object(p,{"index","name","nullable","ownership","lifetime","length_parameter","count_parameter","encoding","alignment","transfer_branch","transfer_predicate"});
   json::natural(p.at("index"));text(p.at("name"));text(p.at("ownership"));require(p.at("nullable").kind=='b',"parameter nullable must be explicit");
   fields(p,{"encoding","transfer_branch","transfer_predicate"},{},{"length_parameter","count_parameter","alignment"});if(p.has("lifetime"))lifetime(p.at("lifetime"));
  }
  const auto& e=f.at("errors");object(e,{"kind","sentinel_value","out_valid_on_failure","cleanup_required_on_failure","error_parameter"});text(e.at("kind"));fields(e,{"sentinel_value","error_parameter"},{"out_valid_on_failure","cleanup_required_on_failure"});
  const auto& effects=f.at("effects");object(effects,{"effects","unknown"});require(effects.at("unknown").kind=='b',"effects.unknown must be explicit");array(effects.at("effects"),16);for(const auto& eff:effects.at("effects").array)text(eff);
  const auto& c=f.at("concurrency");object(c,{"thread_safety","registration_thread_affine","signal_safe","callback_origin","blocking_behavior","suspension_safe"});text(c.at("thread_safety"));require(!text(c.at("blocking_behavior")).empty(),"blocking behavior must be explicit");require(c.at("suspension_safe").kind=='b',"suspension safety must be explicit");fields(c,{"callback_origin"},{"registration_thread_affine","signal_safe"});
  if(f.has("callbacks")){array(f.at("callbacks"),128);for(const auto& cb:f.at("callbacks").array){object(cb,{"signature","mode","user_data_parameter","lifetime","unregister_function","reentrant","may_throw"});text(cb.at("signature"));text(cb.at("mode"));fields(cb,{"unregister_function"},{"reentrant","may_throw"},{"user_data_parameter"});if(cb.has("lifetime"))lifetime(cb.at("lifetime"));}}
  if(f.has("destructors")){array(f.at("destructors"),128);for(const auto& d:f.at("destructors").array){object(d,{"function_name","context_parameter","allocator_family","idempotent","null_safe","thread_affine","may_fail"});text(d.at("function_name"));fields(d,{"allocator_family"},{"idempotent","null_safe","thread_affine","may_fail"},{"context_parameter"});}}
  if(f.has("transfers")){array(f.at("transfers"),128);for(const auto& t:f.at("transfers").array){object(t,{"branch","from_owner","to_owner","predicate"});for(auto key:{"branch","from_owner","to_owner","predicate"})text(t.at(key));}}
  if(f.has("invalidations")){array(f.at("invalidations"),128);for(const auto& i:f.at("invalidations").array){object(i,{"kind","description"});text(i.at("kind"));text(i.at("description"));}}
 }
}
inline std::map<std::string,const cdecl::Declaration*> declarations(const cdecl::TranslationUnit& unit){
 std::map<std::string,const cdecl::Declaration*> out;
 for(const auto& d:unit.declarations)if(d.kind==cdecl::DeclKind::Function){
  auto [it,inserted]=out.emplace(d.name,&d);require(inserted||it->second->type==d.type,"conflicting C declarations: "+d.name);
 }
 return out;
}
inline void evidence(const cdecl::TranslationUnit& unit){
 require(unit.version==cdecl::schema_version&&unit.complete,"incomplete/unsupported CDecl discovery");
 require(!unit.header.empty()&&!unit.revision.empty()&&unit.fingerprint.size()==64&&unit.fingerprint.find_first_not_of("0123456789abcdef")==std::string::npos,"missing/invalid CDecl header/revision/compiler fingerprint");
 require(!unit.target.compiler.empty()&&!unit.target.triple.empty()&&unit.target.pointer_bits,"missing CDecl compiler/target identity");
 require(!unit.inputs.empty(),"missing CDecl input fingerprints");
 std::set<std::string> paths;
 for(const auto& input:unit.inputs)require(!input.path.empty()&&paths.insert(input.path).second&&input.sha256.size()==64&&input.sha256.find_first_not_of("0123456789abcdef")==std::string::npos,"invalid/duplicate CDecl input fingerprint");
 require(paths.contains(unit.header),"CDecl input inventory omits its header");
 for(const auto& diagnostic:unit.diagnostics)require(diagnostic.severity!=moonstone::Severity::Error&&diagnostic.severity!=moonstone::Severity::Fatal,"CDecl carries discovery errors");
}
inline void validate(const cdecl::TranslationUnit& unit,const contract::ContractBundle& bundle,bool pinned){
 evidence(unit);auto index=declarations(unit);
 auto revision=[&](const contract::ContractProvenance& p){require(p.revision.empty()||p.revision==unit.revision,"contract revision drift");require(p.toolchain_fingerprint.empty()||p.toolchain_fingerprint==unit.fingerprint,"contract compiler fingerprint drift");require(std::filesystem::path(p.header).filename()==std::filesystem::path(unit.header).filename(),"contract header drift");if(pinned)require(p.header==unit.header&&p.revision==unit.revision&&p.toolchain_fingerprint==unit.fingerprint,"contract is not pinned to this CDecl identity");};
 revision(bundle.bundle_provenance);
 auto has_header=[&](const std::string& name){return std::find(bundle.headers.begin(),bundle.headers.end(),name)!=bundle.headers.end();};
 require(has_header(unit.header)||(!pinned&&has_header(std::filesystem::path(unit.header).filename().string())),"contract header inventory drift");
 for(const auto& fn:bundle.functions){
  revision(fn.provenance);auto it=index.find(fn.c_name);require(it!=index.end(),"contract declaration missing: "+fn.c_name);const auto& decl=*it->second;
  require(fn.parameters.size()==decl.parameters.size(),"contract parameter count drift: "+fn.c_name);
  std::set<std::uint32_t> used;
  for(const auto& p:fn.parameters){require(p.index<decl.parameters.size()&&used.insert(p.index).second,"duplicate/out-of-range parameter index: "+fn.c_name);require(p.name==decl.parameters[p.index].name,"contract parameter name drift: "+fn.c_name);}
  auto lifetime=[&](const contract::LifetimeSpec& l){if(l.origin==contract::LifetimeOrigin::Parameter||l.origin==contract::LifetimeOrigin::CallerBuffer)require(l.parameter_index<fn.parameters.size(),"out-of-range borrow origin: "+fn.c_name);if(l.origin==contract::LifetimeOrigin::ExplicitRegion)require(!l.region_name.empty(),"missing explicit lifetime region: "+fn.c_name);};
  if(fn.return_contract.lifetime)lifetime(*fn.return_contract.lifetime);
  for(const auto& p:fn.parameters){if(p.lifetime)lifetime(*p.lifetime);if(p.transfer_branch&&*p.transfer_branch==contract::TransferBranch::Conditional)require(!p.transfer_predicate.empty(),"missing conditional transfer predicate: "+fn.c_name);}
  for(const auto& cb:fn.callbacks){if(cb.lifetime)lifetime(*cb.lifetime);if(cb.user_data_parameter)require(*cb.user_data_parameter<fn.parameters.size(),"out-of-range callback user data: "+fn.c_name);}
 }
 auto diagnostics=contract::validate_contract(bundle,unit);
 if(!diagnostics.empty()){std::string message;for(const auto& d:diagnostics)message+=moonstone::format(d)+"\n";throw Rejected(message);}
}
inline contract::ContractBundle pin(const cdecl::TranslationUnit& unit,contract::ContractBundle bundle){
 validate(unit,bundle,false);auto index=declarations(unit);
 auto stamp=[&](contract::ContractProvenance& p){p.header=unit.header;p.revision=unit.revision;p.toolchain_fingerprint=unit.fingerprint;p.review_status=contract::ReviewStatus::Unreviewed;};
 stamp(bundle.bundle_provenance);bundle.headers={unit.header};bundle.state=contract::ContractState::Annotated;
 for(auto& fn:bundle.functions){stamp(fn.provenance);fn.provenance.source_location=index.at(fn.c_name)->source;}
 validate(unit,bundle,true);return bundle;
}
}

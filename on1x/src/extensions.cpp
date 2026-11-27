#include <on1x/extensions.hpp>
#include <on1x/meta.hpp>
#include <mulib-sha.h>
#include "extensions_registry.hpp"
#include <fstream>
#include <algorithm>
#include <charconv>
#include <tuple>

namespace on1x::extensions {
namespace {
namespace fs=std::filesystem;
using namespace meta;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-CAT-PPX: "+text,"ON1X-CAT-PPX");}
std::string tag(const SExpr& n){return n.kind==SExpr::Kind::List&&!n.items.empty()?n.items[0].text:"";}
std::vector<SExpr> registry(){
 auto tree=read_sexpr(on1x_extension_registry,"<bundled-extension-registry>");
 if(tag(tree)!="on1x.extensions"||tree.items.size()!=3||tree.items[1].text!="1")fail(tree.where,"invalid bundled registry");
 return tree.items[2].items;
}
SExpr feature(const std::string& name,const Location& at){
 for(const auto& item:registry())if(item.items.size()>1&&item.items[1].text==name)return item;
 throw Error(at,"ON1X-CAT-ENABLE: unknown extension "+name,"ON1X-CAT-ENABLE");
}
std::string hash(const fs::path& path){
 std::ifstream in(path,std::ios::binary);if(!in)fail({path.string()},"cannot read bundled artifact");
 mulib_sha_ctx ctx;if(!mulib_sha_init(&ctx,MULIB_SHA256))fail({path.string()},"SHA256 initialization failed");
 std::size_t count=0;char bytes[65536];
 while(in){in.read(bytes,sizeof bytes);auto n=static_cast<std::size_t>(in.gcount());
  if(n>128*1024*1024-count)fail({path.string()},"bundled artifact exceeds 128 MiB budget");
  count+=n;
  if(!mulib_sha_update(&ctx,bytes,n))fail({path.string()},"SHA256 update failed");
 }
 if(!in.eof())fail({path.string()},"artifact read failed");
 unsigned char digest[32];if(!mulib_sha_final(&ctx,digest,sizeof digest))fail({path.string()},"SHA256 finalization failed");
 std::string result;constexpr char hex[]="0123456789abcdef";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
struct Resolved {fs::path program,schema;std::string binary_hash,schema_hash;};
std::string module_key(const std::string& origin){
 unsigned char digest[32];if(!mulib_sha256(origin.data(),origin.size(),digest))fail({origin},"module fingerprint failed");
 std::string out;constexpr char hex[]="0123456789abcdef";for(auto b:digest){out+=hex[b>>4];out+=hex[b&15];}return out;
}
Resolved resolve(const Options& options,const SExpr& entry){
 auto name=entry.items.at(1).text;
 if(tag(entry)!="feature"||entry.items.size()!=6)fail({"<extensions>"},name+" is unavailable: "+entry.items.at(2).text);
#if !ON1X_BUILD_NATIVE_PPX
 fail(entry.where,"bundled native extensions disabled by ON1X_BUILD_NATIVE_PPX=OFF");
#endif
 auto basename=entry.items[2].text;
 if(basename.empty()||basename.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=std::string::npos)fail(entry.where,"invalid registry executable basename");
 if(!entry.items[5].items.empty())fail(entry.where,"unsupported extension dependencies");
#if !defined(__unix__) && !defined(__APPLE__)
 fail(entry.where,"native Category execution requires the POSIX subprocess transport");
#endif
 if(options.directory.empty())fail(entry.where,"no trusted native extension directory for this compiler");
 auto root=fs::absolute(options.directory).lexically_normal();
 Resolved out{root/basename,root/"category.schema.json",{}, {}};
 std::error_code ec;
 for(const auto& path:{out.program,out.schema,fs::path(out.program.string()+".pin")}){
  if(!fs::is_regular_file(path,ec)||fs::is_symlink(path,ec))fail({path.string()},"missing/non-regular bundled artifact (no PATH or Python fallback)");
 }
 auto pin=read_sexpr(read_source_file(out.program.string()+".pin"),out.program.string()+".pin");
 if(tag(pin)!="on1x.extension-pin"||pin.items.size()!=6||pin.items[1].text!="1"||pin.items[2].text!=name||pin.items[3].text!=entry.items[3].text)fail(pin.where,"incompatible binary pin");
 out.binary_hash=hash(out.program);out.schema_hash=hash(out.schema);
 if(pin.items[4].kind!=SExpr::Kind::String||pin.items[5].kind!=SExpr::Kind::String||pin.items[4].text!=out.binary_hash||pin.items[5].text!=out.schema_hash)fail(pin.where,"binary/schema digest mismatch");
 std::ifstream binary(out.program,std::ios::binary);char magic[2]={};binary.read(magic,2);
 if(magic[0]=='#'&&magic[1]=='!')fail({out.program.string()},"bundled Category processors must be native executables");
 return out;
}
 SExpr loc(const Location& at){return list({symbol("Loc"),string(at.file),symbol(std::to_string(at.offset)),symbol(std::to_string(at.line)),symbol(std::to_string(at.column))});}
 using SourcePoint=std::tuple<std::size_t,std::size_t,std::size_t>;
 void validate_provenance(const SExpr& node,std::string_view origin,const std::set<SourcePoint>& source){
  if(node.kind!=SExpr::Kind::List)return;
  if(!node.items.empty()&&node.items.front().kind==SExpr::Kind::Symbol&&node.items.front().text=="Loc"){
   // The envelope schema has already checked the types, arity and bounds.
   auto coordinate=[&](std::size_t field){
    const auto& text=node.items[field].text;std::size_t value=0;
    std::from_chars(text.data(),text.data()+text.size(),value);return value;
   };
   if(node.items[1].text!=origin||!source.contains({coordinate(2),coordinate(3),coordinate(4)}))
    fail(node.where,"response location was not supplied by its source carriers");
   return;
  }
  for(const auto& child:node.items)validate_provenance(child,origin,source);
 }
}
std::string digest(const std::string& bytes){return module_key(bytes);}
std::string canonical(const std::string& name,const Location& at){(void)feature(name,at);return name;}
void select(std::set<std::string>& into,const std::string& names,const Location& at){
 std::size_t start=0;
 for(;;){auto end=names.find(',',start);auto name=names.substr(start,end==std::string::npos?end:end-start);
  into.insert(canonical(name,at));if(end==std::string::npos)break;start=end+1;
 }
}
std::string describe(const Options& options,const std::string& name){
 std::string out="on1x.extensions/v1 (host "+std::string(on1x_extension_host)+"; native PPXs; no source executable authorization)\n";
 auto entries=name.empty()?registry():std::vector<SExpr>{feature(name,{"<command-line>"})};
 for(const auto& entry:entries){out+=entry.items[1].text+": ";
  try{auto resolved=resolve(options,entry);out+=entry.items[4].text+", version "+entry.items[3].text+", native "+resolved.program.string()+", sha256 "+resolved.binary_hash+", schema "+resolved.schema_hash+", dependencies none, "+(options.disabled.contains(entry.items[1].text)?"execution disabled by CLI":"execution permitted on POSIX host")+"\n";}
  catch(const Error& error){out+="UNAVAILABLE: "+std::string(error.what())+"\n";}
 }
 return out;
}
void expand(Unit& unit,const Options& options,const std::string& origin){
 std::set<std::string> active=options.enabled;
 for(const auto& item:unit.features){
  if(options.disabled.contains(item.name))throw Error(item.where,"ON1X-CAT-ENABLE: explicitly disabled extension "+item.name,"ON1X-CAT-ENABLE");
  active.insert(item.name);
 }
 for(const auto& name:active){
  if(options.disabled.contains(name))throw Error({origin},"ON1X-CAT-ENABLE: enabled/disabled conflict: "+name,"ON1X-CAT-ENABLE");
  auto entry=feature(name,{origin});
  if(tag(entry)!="feature")throw Error({origin},"ON1X-CAT-ENABLE: "+name+" is unimplemented: "+entry.items[2].text,"ON1X-CAT-ENABLE");
 }
 for(const auto& carrier:unit.extensions)if(!active.contains(carrier.owner))
  throw Error(carrier.where,"ON1X-CAT-ENABLE: carrier owner is not active: "+carrier.owner,"ON1X-CAT-ENABLE");
 if(!options.execute)return;
 std::vector<Function> generated_functions;
 std::set<std::string> names_in_scope;
 for(const auto& f:unit.functions)names_in_scope.insert(f.name);
 for(const auto& name:active){
   auto entry=feature(name,{origin});auto resolved=resolve(options,entry);
   std::vector<SExpr> carriers,names;
   std::set<SourcePoint> source;
   auto input_point=[&](const Location& at){
    if(at.file!=origin)fail(at,"source carrier location does not match its module origin");
    source.emplace(at.offset,at.line,at.column);
   };
  for(const auto& f:unit.functions)names.push_back(string(f.name));
  for(const auto& c:unit.extensions)if(c.owner==name){
    input_point(c.where);input_point(c.end);
    std::vector<SExpr> tokens;for(const auto& t:c.tokens){
     input_point(t.where);
     tokens.push_back(list({symbol("SourceToken"),string(t.text),loc(t.where)}));
    }
   carriers.push_back(list({symbol("DeclarationCarrier"),string(name),string(c.source),loc(c.where),loc(c.end),list(std::move(tokens))}));
  }
  if(carriers.empty())continue;
  auto packet=list({symbol("ModuleSyntax"),symbol("1"),string(origin),list(std::move(names)),list(std::move(carriers)),string(module_key(origin))});
  auto schema=base_schema();schema.validate(packet,"ExtensionInput");
   auto wire=run_transformer(resolved.program.string(),write_sexpr(packet),options.timeout_ms,name,{{"ON1X_PPX_SCHEMA",resolved.schema.string()},{"ON1X_EXTENSION_PHASE","syntax-v1"}});
  auto result=read_sexpr(wire,"<native-extension:"+name+">");schema.validate(result,"ExtensionOutput");
    if(tag(result)!="ModuleEvidenceExpansion"||result.items[2].text!=origin)fail({origin},"response origin/envelope mismatch (checked evidence required)");
   validate_provenance(result,origin,source);
  // Facts are reporting data only, never privileged typing or ownership proofs.
  for(const auto& fact:result.items[4].items)if(fact.items[1].text!=name||fact.items.back().items[1].text!=origin)fail({origin},"response fact owner/source mismatch");
  if(hash(resolved.program)!=resolved.binary_hash||hash(resolved.schema)!=resolved.schema_hash)fail({origin},"bundled artifact changed during expansion");
   const auto& tree=result.items[3];
   if(!tree.items[2].items.empty()){
    for(const auto& decl:tree.items[2].items){
     if(tag(decl)!="Function"&&tag(decl)!="Procedure")
      fail(decl.where,"native declaration profile admits only generated function/procedure definitions");
    }
    auto generated=decode(tree);
   if(unit.functions.size()+generated_functions.size()+generated.functions.size()>2048)fail({origin},"generated function budget exhausted");
    for(auto& f:generated.functions){
     if(f.external||!f.attributes.empty())fail(f.where,"generated extern/attribute not admitted");
     if(!names_in_scope.insert(f.name).second)fail(f.where,"generated declaration collides with an existing function: "+f.name);
     f.extension_generated=true;generated_functions.push_back(std::move(f));
    }
   }
   ExtensionEvidence evidence;
   evidence.owner=name;evidence.origin=origin;evidence.version=entry.items[3].text;
   evidence.program=resolved.program.string();evidence.schema=resolved.schema.string();
   evidence.binary_hash=resolved.binary_hash;evidence.schema_hash=resolved.schema_hash;
   evidence.registry_hash=digest(on1x_extension_registry);evidence.timeout_ms=options.timeout_ms;
#if defined(__linux__)
   evidence.compiler_hash=hash("/proc/self/exe");
#else
   fail({origin},"checked evidence requires an audited compiler image identity");
#endif
   if(result.items[5].items.size()+result.items[6].items.size()>64)fail({origin},"evidence declaration budget exhausted");
   for(const auto& c:result.items[5].items){
    ExtensionContract contract;contract.name=c.items[1].text;contract.where=decode_source_location(c.items[3]);
    if(c.items[2].items.size()>16)fail(contract.where,"evidence member budget exhausted");
    for(const auto& m:c.items[2].items)contract.members.push_back({m.items[1].text,decode_type(m.items[2]),decode_source_location(m.items[3])});
    evidence.contracts.push_back(std::move(contract));
   }
   for(const auto& w:result.items[6].items){
    ExtensionImplementation impl;impl.name=w.items[1].text;impl.contract=w.items[2].text;
    impl.canonical=w.items[4].text=="true";impl.where=decode_source_location(w.items[6]);
    if(w.items[3].items.size()>16||w.items[5].items.size()>16)fail(impl.where,"evidence argument/member budget exhausted");
    for(const auto& a:w.items[3].items)impl.arguments.push_back(decode_type(a));
    for(const auto& b:w.items[5].items){
     ExtensionBinding binding{b.items[1].text,b.items[2].text,b.items[3].text,{},{},decode_source_location(b.items[4])};
     schema.validate(string(binding.function),"Name");schema.validate(string(binding.selector),"Name");
     binding.function_id=origin+"::function::"+binding.function;
     binding.selector_id=origin+"::function::"+binding.selector;
     impl.bindings.push_back(std::move(binding));
    }
    evidence.implementations.push_back(std::move(impl));
   }
   unit.extension_evidence.push_back(std::move(evidence));
 }
 for(auto& f:generated_functions)unit.functions.push_back(std::move(f));
 unit.extensions.clear();
}
std::string evidence_plan(const Unit& unit){
 verify_evidence(unit);
 std::vector<const ExtensionEvidence*> ordered;
 for(const auto& evidence:unit.extension_evidence)ordered.push_back(&evidence);
 std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return std::tie(a->origin,a->owner)<std::tie(b->origin,b->owner);});
 std::vector<SExpr> reports,sources;
 for(const auto& [origin,hash]:unit.source_digests)
  sources.push_back(list({symbol("SourceDigest"),string(origin),string(hash)}));
 for(const auto* evidence:ordered){
  Options options;options.directory=fs::path(evidence->program).parent_path();
  auto entry=feature(evidence->owner,{evidence->origin});auto resolved=resolve(options,entry);
  if(resolved.program!=evidence->program||resolved.schema!=evidence->schema||
     resolved.binary_hash!=evidence->binary_hash||resolved.schema_hash!=evidence->schema_hash||
     entry.items[3].text!=evidence->version||digest(on1x_extension_registry)!=evidence->registry_hash)
   fail({evidence->origin},"extension changed between syntax and evidence phases");
  auto snapshot=evidence_snapshot(unit,*evidence);
  auto wire=run_transformer(resolved.program.string(),write_sexpr(snapshot),evidence->timeout_ms,evidence->owner,
    {{"ON1X_PPX_SCHEMA",resolved.schema.string()},{"ON1X_EXTENSION_PHASE","evidence-v1"}});
  auto plan=read_sexpr(wire,"<native-evidence:"+evidence->owner+">");
  admit_evidence_plan(unit,*evidence,plan);
  if(hash(resolved.program)!=evidence->binary_hash||hash(resolved.schema)!=evidence->schema_hash)
   fail({evidence->origin},"bundled artifact changed during evidence planning");
  reports.push_back(list({symbol("CheckedModule"),string(evidence->owner),string(evidence->version),
    string(evidence->program),string(evidence->binary_hash),string(evidence->schema_hash),
    string(evidence->compiler_hash),string(evidence->registry_hash),std::move(snapshot),std::move(plan)}));
 }
 return write_sexpr(list({symbol("on1x.category-plan"),symbol("1"),string(on1x_extension_host),
   symbol("well_typed"),list(std::move(reports)),list(std::move(sources))}));
}
}

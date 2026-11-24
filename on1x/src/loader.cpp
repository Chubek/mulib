#include <on1x/loader.hpp>
#include <on1x/ffi_hir.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/discovery.hpp>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace on1x {
namespace ffi {
// Only the loader, after exact import, mode and target resolution, can mint
// the HIR reference. Checkers, rewrites and wire readers may never mint one.
class HirResolver {
public:
 static HirForeign resolve(const Descriptor& descriptor,ForeignMode mode){
  return HirForeign(descriptor,mode);
 }
};
}
namespace {
namespace fs=std::filesystem;
struct Module;
struct Symbol {
 Module* owner;
 std::string name;
 bool external=false;
 bool operator==(const Symbol& other) const {
  return name==other.name&&external==other.external&&(owner==other.owner||external);
 }
};
struct Names {
 std::map<std::string,Symbol> values,syntax,types,cliches;
 std::map<std::string,Module*> modules;
};
struct Module {
 fs::path path;
 std::string source;
 Unit unit;
 Names names;
 std::vector<Module*> quoted;
  std::vector<Module*> dependencies;
  std::map<std::string,KernelImport> kernels;
 std::size_t id=0;
  bool active=true,legacy=false,implicit=false;
  std::optional<SharedKind> shared;
};
bool regular(const fs::path& path){std::error_code ec;return fs::is_regular_file(path,ec);}
bool directory(const fs::path& path){std::error_code ec;return fs::is_directory(path,ec);}
fs::path canonical(const fs::path& path,const Location& at){
 std::error_code ec;auto result=fs::canonical(path,ec);
 if(ec)throw Error(at,"cannot resolve import '"+path.string()+"': "+ec.message());
 return result;
}
fs::path module_path(std::string name){std::replace(name.begin(),name.end(),'.',fs::path::preferred_separator);return name;}
fs::path package(const Module& m){return directory(m.path)?m.path:m.path.parent_path();}

// The sole compatibility profile with an executable resource ABI. Its
// opaque owners and companion calls are never available to safe imports.
constexpr std::string_view ipc_kernel_module="mulib.ipc_framed";
constexpr std::string_view ipc_kernel_target="x86_64-linux-gnu";
void require_kernel_import_policy(const KernelImport& request){
 if(request.module==ipc_kernel_module&&!request.is_unsafe)
  throw Error(request.where,"ON1X-KERNEL-NOT-ADMITTED: "+request.module+
               " is unsafe-only; use unsafe import foreign stdffi.ipc_framed",
              "ON1X-KERNEL-NOT-ADMITTED");
}

class Loader {
 using Scope=std::set<std::string>;
  const LoaderOptions& options;
  fs::path entry_directory;
  std::vector<fs::path> standard_roots;
  std::map<fs::path,SharedKind> shared_sources;
 std::map<fs::path,std::unique_ptr<Module>> modules;
 std::vector<Module*> order,stack,preludes;
  std::set<std::string> tokens,globals,type_globals,cliche_globals;
 std::string prefix="on1x_module_";
 std::size_t bytes=0,bindings=0,edges=0;
 bool implicit=false;
  KernelImportRequests* inspection;
  const Module* kernel_scope=nullptr;
  bool unsafe_context=false;
  std::size_t kernel_requests=0;
  std::map<std::string,Function> kernel_calls;
  static void append_parameters(Function& f,const std::vector<Type>& parameters){
   for(std::size_t i=0;i<parameters.size();++i){
    Param parameter;parameter.name="arg"+std::to_string(i);
    parameter.type=parameters[i];parameter.where=f.where;
    f.params.push_back(std::move(parameter));
   }
  }
  std::string kernel_type_prefix(bool history)const{return prefix+(history?"kernel_history_":"kernel_pricing_");}
  Type kernel_result(bool history)const {
   auto p=kernel_type_prefix(history);
   return Type::named(p+"Result",{Type::named(p+(history?"Forecast":"Decision")),Type::named(p+"Error")});
  }
  bool kernel_reference(Expr& e,const Scope& values,bool direct){
   if(!kernel_scope||e.kind!=Expr::Kind::Variable)return false;
   const auto dot=e.text.find('.');
   const auto alias=e.text.substr(0,dot);
   auto found=kernel_scope->kernels.find(alias);
   if(found==kernel_scope->kernels.end()||values.contains(alias))return false;
   if(dot==std::string::npos||!direct){
    throw Error(e.where,"ON1X-KERNEL-DIRECT: kernel exports must be called directly","ON1X-KERNEL-DIRECT");
   }
    const std::string_view member=std::string_view(e.text).substr(dot+1);
    const auto& request=found->second;
     auto* descriptor=request.module==ipc_kernel_module?nullptr:ffi::find_descriptor(request.module,member);
    if(descriptor&&descriptor->schema_version!=2)descriptor=nullptr;
    // Keep the old reviewed driver/stub subject unchanged. The direct unsafe
    // Calls through an unsafe alias keep their checked descriptor and lexical
    // unsafe requirement even in a mixed legacy reviewed build.
    const auto* scalar=!options.ffi_stage&&request.module==legacy_scalar_module&&
     (!descriptor||(options.kernel_stage&&!request.is_unsafe))?ffi::find_member(request.module,member):nullptr;
    if(scalar)descriptor=nullptr;
    const auto symbol=scalar?scalar->symbol:
     (options.ffi_stage?std::string_view{}:legacy_kernel_member(request.module,member));
    if(!descriptor&&symbol.empty()){
     throw Error(e.where,"ON1X-KERNEL-MEMBER: no ABI-verified export "+e.text,"ON1X-KERNEL-MEMBER");
    }
    const bool unsafe=request.is_unsafe;
    if(unsafe&&!unsafe_context)
     throw Error(e.where,"ON1X-KERNEL-CONTEXT: "+e.text+" requires unsafe { ... }","ON1X-KERNEL-CONTEXT");
    if(descriptor){
     if(descriptor->target.triple!=options.foreign_target)
      throw Error(e.where,"ON1X-FFI-TARGET: unsupported foreign target for "+e.text,"ON1X-FFI-TARGET");
     const auto mode=unsafe?(options.ffi_stage?ffi::ForeignMode::UnsafeLinkOnly:ffi::ForeignMode::Unsafe):
      ffi::ForeignMode::LinkOnly;
     e.foreign=ffi::HirResolver::resolve(*descriptor,mode);
     auto name=ffi::foreign_hir_name(e.foreign);
     if(!kernel_calls.contains(name)){
      Function f;f.name=name;f.where=e.where;f.external=true;f.foreign=e.foreign;
      f.result=ffi::surface_type(descriptor->surface_result);
      std::vector<Type> params;
      for(const auto& parameter:descriptor->parameters)params.push_back(ffi::surface_type(parameter.surface));
      append_parameters(f,params);
      ffi::verify_foreign_declaration(f,options.foreign_target);
      kernel_calls.emplace(name,std::move(f));
     }
     e.text=std::move(name);
     return true;
    }
    const bool history=history_kernel_symbol(symbol),pricing=pricing_kernel_symbol(symbol),ipc=ipc_kernel_symbol(symbol);
    auto name=(unsafe?"@kernel_":"@kernel_scalar_")+std::string(symbol);
    if(!kernel_calls.contains(name)){
     Function f;f.name=name;f.where=e.where;
     f.result=ipc?ipc_kernel_result(symbol):history||pricing?kernel_result(history):Type{Type::I64};f.external=true;
     f.kernel_symbol=symbol;f.kernel_link_only=!unsafe;
     auto params=ipc?ipc_kernel_parameters(symbol):
        history?history_kernel_parameters(symbol):pricing?pricing_kernel_parameters(symbol,kernel_type_prefix(false)):
       std::vector<Type>(scalar->arity,Type::I64);
    append_parameters(f,params);
    kernel_calls.emplace(name,std::move(f));
   }
   e.text=std::move(name);
   return true;
  }
 void explicit_module(Module* m){
  if(!m->implicit)return;
  m->implicit=false;for(auto* child:m->dependencies)explicit_module(child);
 }
 void charge(const Location& at){if(++bindings>262144)throw Error(at,"module binding limit exceeded");}
 template<class T> void insert(std::map<std::string,T>& into,const std::string& name,const T& value,const Location& at){
  auto [it,added]=into.emplace(name,value);
  if(!added&&it->second!=value)throw Error(at,"conflicting import or declaration: "+name);
  if(added)charge(at);
 }
 bool module_bound(const Names& names,const std::string& alias)const{
  for(const auto& [key,module]:names.modules){(void)module;if(key==alias||key.starts_with(alias+"."))return true;}
  return false;
 }
 void value_alias(const Names& names,const std::string& alias,const Location& at){
  if(module_bound(names,alias))throw Error(at,"import alias conflicts with module: "+alias);
 }
  void merge(Names& into,const Names& from,const Location& at){
  for(const auto& [name,symbol]:from.values){value_alias(into,name,at);insert(into.values,name,symbol,at);}
   for(const auto& [name,symbol]:from.syntax){value_alias(into,name,at);insert(into.syntax,name,symbol,at);}
    for(const auto& [name,symbol]:from.types){value_alias(into,name,at);insert(into.types,name,symbol,at);}
    for(const auto& [name,symbol]:from.cliches){value_alias(into,name,at);insert(into.cliches,name,symbol,at);}
  for(const auto& [name,m]:from.modules)bind_module(into,name,m,at);
 }
  fs::path candidate(const fs::path& base,const Location& at){
  auto file=base;file+=".on1x";auto init=base/"__init__.on1x";
  if(regular(file)&&regular(init))throw Error(at,"ambiguous module: both "+file.string()+" and "+init.string());
  if(regular(file))return file;
  if(regular(init))return init;
  if(directory(base))return base; // Namespace package, with no executable initializer.
   return {};
  }
   fs::path standard(const std::string& module,const Location& at){
    for(const auto& root:standard_roots){
     auto first=candidate(root/module_path(module),at);
     // Compatibility with older installed extension trees; the canonical MuLIB
     // package now lives inside stdlib, not in this legacy sibling directory.
     auto second=candidate(root.parent_path()/"std"/module_path(module),at);
     if(!first.empty()&&!second.empty())throw Error(at,"ambiguous standard module: "+module);
     if(!first.empty()||!second.empty())return first.empty()?second:first;
    }
    return {};
  }
 fs::path resolve(const Import& imp,const Module& parent){
  if(imp.quoted)return parent.path.parent_path()/imp.module;
  auto relative=module_path(imp.module);
  if(imp.relative){
   auto base=package(parent);
   if(imp.relative>options.max_depth)throw Error(imp.where,"relative import depth limit exceeded");
   for(std::size_t i=1;i<imp.relative;++i){
    if(base==base.parent_path())throw Error(imp.where,"relative import escapes filesystem root");
    base=base.parent_path();
   }
   if(imp.module.empty())return regular(base/"__init__.on1x")?base/"__init__.on1x":base;
   auto found=candidate(base/relative,imp.where);if(!found.empty())return found;
   throw Error(imp.where,"module not found: "+std::string(imp.relative,'.')+imp.module+" (in "+base.string()+")");
  }
  if(imp.module=="std"||imp.module.starts_with("std.")){
    if(standard_roots.empty())throw Error(imp.where,"standard library root is not configured");
     auto found=imp.module=="std"?standard_roots.front():standard(imp.module.substr(4),imp.where);
   if(!found.empty()&&fs::exists(found))return found;
   throw Error(imp.where,"standard module not found: "+imp.module);
  }
  std::vector<fs::path> roots{entry_directory};roots.insert(roots.end(),options.paths.begin(),options.paths.end());
   roots.insert(roots.end(),standard_roots.begin(),standard_roots.end());
  std::string searched;
   for(const auto& root:roots){
    auto found=discovery::library(root,imp.module,imp.where);
    if(found.empty())found=candidate(root/relative,imp.where);
    if(!found.empty())return found;
   if(!searched.empty())searched+=", ";
   searched+=root.string();
  }
  throw Error(imp.where,"module not found: "+imp.module+" (searched "+searched+")");
 }
  void bind_module(Names& names,const std::string& alias,Module* target,const Location& at){
   auto first=alias.substr(0,alias.find('.'));
    if(names.values.contains(first)||names.syntax.contains(first)||names.types.contains(first)||
       names.cliches.contains(first))throw Error(at,"module alias conflicts with declaration: "+first);
  for(const auto& [other,m]:names.modules){
   (void)m;
   if(other!=alias&&(alias.starts_with(other+".")||other.starts_with(alias+".")))
    throw Error(at,"overlapping module aliases: "+alias+" and "+other);
  }
  insert(names.modules,alias,target,at);
 }
 Module* visit(const fs::path& input,const Location& at,bool allow_directory=true){
  auto path=canonical(input,at);
  if(auto hit=modules.find(path);hit!=modules.end()){
   if(hit->second->active){
    std::string chain;for(const auto* m:stack)chain+=m->path.string()+" -> ";
    throw Error(at,"import cycle: "+chain+path.string());
   }
   if(!implicit)explicit_module(hit->second.get());
   return hit->second.get();
  }
  if(modules.size()>=options.max_modules)throw Error(at,"module count limit exceeded");
  if(stack.size()>=options.max_depth)throw Error(at,"import recursion limit exceeded");
  if(!regular(path)&&!(allow_directory&&directory(path)))throw Error(at,"import is not a regular source file: "+path.string());
  auto storage=std::make_unique<Module>();auto* m=storage.get();m->path=path;m->id=modules.size();
  m->implicit=implicit;
  modules.emplace(path,std::move(storage));stack.push_back(m);
  if(regular(path)){
     auto replacement=options.source_overrides.find(path);
     m->source=replacement==options.source_overrides.end()?read_source_file(path.string()):replacement->second;
    if(m->source.size()>options.max_bytes-bytes)throw Error(at,"total imported source byte limit exceeded");
    bytes+=m->source.size();auto scanned=scan(m->source,path.string());
   for(const auto& t:scanned)tokens.insert(t.text);
      m->unit=parse(scanned,{options.extensions.enabled.begin(),options.extensions.enabled.end()});
      if(options.standalone_guest){
       const auto& u=m->unit;
       if(!u.imports.empty()||!u.module_imports.empty()||!u.kernel_imports.empty()||
          !u.ppx.empty()||!u.features.empty()||!u.extensions.empty())
        throw Error({path.string()},"standalone ArchVirt source cannot import or activate extensions");
       for(const auto& f:u.functions)
        if(!f.attributes.empty()||(f.external&&(f.name!="on1x_archvirt_read_i64"||
           !f.params.empty()||f.result!=Type::I64)))
         throw Error(f.where,"standalone ArchVirt source permits only the guest i64 input extern and no attributes");
      }
       if((!m->unit.cliches.empty()||!m->unit.cliche_implementations.empty()||
           !m->unit.inherent_implementations.empty())&&
          (!m->unit.ppx.empty()||!m->unit.extensions.empty()||!m->unit.features.empty()))
        throw Error(!m->unit.cliches.empty()?m->unit.cliches.front().where:
                    !m->unit.cliche_implementations.empty()?
                     m->unit.cliche_implementations.front().where:
                     m->unit.inherent_implementations.front().where,
                    "intrinsic cliche/impl declarations cannot cross legacy Category or Unit-v1 PPX expansion",
                    !m->unit.inherent_implementations.empty()?"ON1X-IMPL-UNSUPPORTED":
                                                               "ON1X-CLICHE-UNSUPPORTED");
       if(!options.interfaces.empty()){
       const auto reject=[](const Location& where){
        throw Error(where,"interface verification does not execute PPX or declaration extensions",
                    "ON1X-INTERFACE-UNSUPPORTED");
       };
       if(!m->unit.ppx.empty())reject(m->unit.ppx.front().where);
       if(!m->unit.features.empty())reject(m->unit.features.front().where);
        if(!m->unit.extensions.empty())reject(m->unit.extensions.front().where);
         if(!m->unit.cliches.empty())reject(m->unit.cliches.front().where);
         if(!m->unit.cliche_implementations.empty())reject(m->unit.cliche_implementations.front().where);
         if(!m->unit.inherent_implementations.empty())
          throw Error(m->unit.inherent_implementations.front().where,
                      "interface verification does not transport intrinsic impl declarations",
                      "ON1X-IMPL-UNSUPPORTED");
      }
      if(!inspection){
       for(const auto& request:m->unit.kernel_imports)require_kernel_import_policy(request);
       if(!options.kernel_c11){
        if(!m->unit.kernel_imports.empty()&&m->unit.kernel_imports.front().module==ipc_kernel_module)
         throw Error(m->unit.kernel_imports.front().where,
                     "ON1X-KERNEL-UNSAFE-UNSUPPORTED: mulib.ipc_framed requires the static KernelBind adapter (--kernel-bind)",
                     "ON1X-KERNEL-UNSAFE-UNSUPPORTED");
        require_no_kernel_imports(m->unit);
       }
      }
     if(options.kernel_c11){
       if(!m->unit.ppx.empty()||!m->unit.macros.empty()||!m->unit.extensions.empty()||
          !m->unit.features.empty()||!m->unit.cliches.empty()||
          !m->unit.cliche_implementations.empty()||!m->unit.inherent_implementations.empty())
        throw Error(at,"ON1X-KERNEL-PROFILE: this C11 profile does not execute source macros, PPX or extensions","ON1X-KERNEL-PROFILE");
       for(const auto& request:m->unit.kernel_imports){
           // A scalar catalogue row cannot stand in for the checked resource
           // ABI, even for an unsafe request. Use the exact companion profile.
           if(request.module==ipc_kernel_module){
            if(options.ffi_stage||request.dynlib)
             throw Error(request.where,"ON1X-FFI-ABI: "+request.module+
                          " has no generated or dynlib resource ABI; use the static KernelBind adapter",
                         "ON1X-FFI-ABI");
            if(options.foreign_target!=ipc_kernel_target)
             throw Error(request.where,"ON1X-FFI-TARGET: unsupported resource target for "+request.module,
                         "ON1X-FFI-TARGET");
            continue;
           }
           const auto descriptors=ffi::foreign_descriptors();
          const auto registered=std::find_if(descriptors.begin(),descriptors.end(),[&](const auto& descriptor){
           return descriptor.schema_version==2&&descriptor.module==request.module;
          });
          if(registered!=descriptors.end()&&registered->target.triple!=options.foreign_target)
           throw Error(request.where,"ON1X-FFI-TARGET: unsupported foreign target for "+request.module,"ON1X-FFI-TARGET");
          const bool legacy=!options.ffi_stage&&(request.is_unsafe?
            request.module==legacy_scalar_module:
           options.kernel_stage&&(request.module==legacy_scalar_module||request.module=="mulib.histprice"||request.module=="mulib.priceopt"));
          const bool supported=legacy||
           (registered!=descriptors.end()&&(request.is_unsafe||options.kernel_stage||options.ffi_stage));
         if(!supported){
        Unit denied;denied.kernel_imports.push_back(request);require_no_kernel_imports(denied);
       }
      }
      for(const auto& f:m->unit.functions){
       if(f.external)
        throw Error(f.where,"ON1X-KERNEL-EXTERN: raw externs cannot be mixed with this KernelBind profile","ON1X-KERNEL-EXTERN");
       if(!f.attributes.empty())
        throw Error(f.where,"ON1X-KERNEL-PROFILE: attributes are outside this C11 profile","ON1X-KERNEL-PROFILE");
      }
     }
    for(auto& carrier:m->unit.extensions){
     if(carrier.end.offset-carrier.where.offset>262144)throw Error(carrier.where,"ON1X-CAT-SYNTAX: carrier byte budget exhausted","ON1X-CAT-SYNTAX");
      carrier.source=m->source.substr(carrier.where.offset,carrier.end.offset-carrier.where.offset);
    }
    extensions::expand(m->unit,options.extensions,path.string());
     m->unit.source_digests.emplace(path.string(),extensions::digest(m->source));
  }
    if(auto found=shared_sources.find(path);found!=shared_sources.end()){
     m->shared=found->second;
     for(const auto& op:shared_intrinsics)
      if(shared_module(op.kind)==*m->shared)
       insert(m->names.values,std::string(op.spelling),Symbol{m,std::string(op.identity),true},at);
    }
    for(const auto& d:m->unit.nominals)insert(m->names.types,d.name,Symbol{m,d.name},d.where);
    for(const auto& c:m->unit.cliches)insert(m->names.cliches,c.name,Symbol{m,c.name},c.where);
   for(const auto& f:m->unit.functions){
   if(is_reserved_intrinsic_name(f.name))throw Error(f.where,"intrinsic name cannot be redefined: "+f.name);
   // PPX cfg may select between declarations with the same name. Retain both
   // AST declarations; the normal post-PPX checker rejects remaining duplicates.
   insert(m->names.values,f.name,Symbol{m,f.name,f.external},f.where);
  }
  for(const auto& macro:m->unit.macros){
   if(m->names.syntax.contains(macro.name))throw Error(macro.where,"duplicate macro: "+macro.name);
   insert(m->names.syntax,macro.name,Symbol{m,macro.name},macro.where);
  }
  for(const auto& imp:m->unit.module_imports){
   if(++edges>16384)throw Error(imp.where,"import edge limit exceeded");
   // `from . import child` addresses a directory, not its currently-loading initializer.
   auto target_path=resolve(imp,*m);
   Module* target=nullptr;
   bool self_package=imp.relative&&imp.module.empty()&&canonical(target_path,imp.where)==m->path;
   if(self_package)target=m;else target=visit(target_path,imp.where,!imp.quoted);
   if(target!=m)m->dependencies.push_back(target);
   if(imp.quoted){m->quoted.push_back(target);merge(m->names,target->names,imp.where);continue;}
   if(!imp.selective){bind_module(m->names,imp.alias.empty()?imp.module:imp.alias,target,imp.where);continue;}
   for(const auto& selection:imp.names){
    const auto& name=selection.name;const auto& alias=selection.alias.empty()?name:selection.alias;
    bool found=false;
    if(!self_package){
     if(auto it=target->names.values.find(name);it!=target->names.values.end()){value_alias(m->names,alias,imp.where);insert(m->names.values,alias,it->second,imp.where);found=true;}
       if(auto it=target->names.syntax.find(name);it!=target->names.syntax.end()){value_alias(m->names,alias,imp.where);insert(m->names.syntax,alias,it->second,imp.where);found=true;}
       if(auto it=target->names.types.find(name);it!=target->names.types.end()){value_alias(m->names,alias,imp.where);insert(m->names.types,alias,it->second,imp.where);found=true;}
       if(auto it=target->names.cliches.find(name);it!=target->names.cliches.end()){value_alias(m->names,alias,imp.where);insert(m->names.cliches,alias,it->second,imp.where);found=true;}
     if(auto it=target->names.modules.find(name);it!=target->names.modules.end()){bind_module(m->names,alias,it->second,imp.where);found=true;}
    }
    if(found)continue;
    if(!directory(target->path)&&target->path.filename()!="__init__.on1x")throw Error(imp.where,"module '"+imp.module+"' has no export '"+name+"'");
     auto child=candidate(package(*target)/name,imp.where);
      for(const auto& root:standard_roots)
       if(directory(root)&&directory(target->path)&&target->path==canonical(root,imp.where)){child=standard(name,imp.where);break;}
    if(child.empty())throw Error(imp.where,"module '"+imp.module+"' has no export or submodule '"+name+"'");
    auto* imported=visit(child,imp.where);m->dependencies.push_back(imported);
    bind_module(m->names,alias,imported,imp.where);
   }
  }
   if((inspection||options.kernel_c11)&&regular(path)){
   std::set<std::string> aliases;
   for(auto& request:m->unit.kernel_imports){
     if(++edges>16384||++kernel_requests>1024)
     throw Error(request.where,"kernel import inspection limit exceeded");
     if(!aliases.insert(request.alias).second||module_bound(m->names,request.alias)||
       m->names.values.contains(request.alias)||m->names.types.contains(request.alias)||
       m->names.syntax.contains(request.alias)||m->names.cliches.contains(request.alias)){
      throw Error(request.where,"conflicting kernel import alias: "+request.alias,"ON1X-KERNEL-ALIAS");
     }
     if(inspection)inspection->requests.push_back(std::move(request));
     else m->kernels.emplace(request.alias,std::move(request));
   }
    if(inspection)inspection->source_digests.merge(m->unit.source_digests);
  }
  stack.pop_back();m->active=false;order.push_back(m);return m;
 }
 void legacy(Module* m){if(m->legacy)return;m->legacy=true;for(auto* child:m->quoted)legacy(child);}
  std::string name(const Symbol& s)const{
   if(s.owner->shared)
    if(auto nominal=shared_export_nominal(*s.owner->shared,s.name);!nominal.empty())
     return std::string(nominal);
   return s.external||s.owner->legacy?s.name:prefix+std::to_string(s.owner->id)+"_"+s.name;
  }
 void bind(Scope& scope,const Pattern& p){pattern_bindings(p,[&](const Pattern& name){scope.insert(name.name);});}
 void parameters(Scope& scope,const std::vector<Param>& ps){for(const auto& p:ps){if(p.pattern)bind(scope,*p.pattern);else scope.insert(p.name);}}
  void reference(std::string& text,const Location& at,const Names& names,const Scope& locals,bool syntax,bool type=false){
  auto dot=text.find('.');auto first=text.substr(0,dot);
  if(locals.contains(first)){
   if(dot!=std::string::npos)throw Error(at,"qualified reference is shadowed by local binding: "+first);
   return;
  }
   const auto& symbols=type?names.types:syntax?names.syntax:names.values;
  if(auto it=symbols.find(text);it!=symbols.end()){text=name(it->second);return;}
  if(dot!=std::string::npos){
   // Longest bound prefix, followed by explicitly imported/re-exported modules.
   for(auto cut=text.rfind('.');cut!=std::string::npos;cut=cut?text.rfind('.',cut-1):std::string::npos){
    auto it=names.modules.find(text.substr(0,cut));if(it==names.modules.end())continue;
    std::string member=text.substr(cut+1);auto* module=it->second;
    while(true){
     const auto& exports=type?module->names.types:syntax?module->names.syntax:module->names.values;
     if(auto symbol=exports.find(member);symbol!=exports.end()){text=name(symbol->second);return;}
     bool descended=false;
     for(auto split=member.rfind('.');split!=std::string::npos;split=split?member.rfind('.',split-1):std::string::npos){
      auto child=module->names.modules.find(member.substr(0,split));if(child==module->names.modules.end())continue;
      module=child->second;member=member.substr(split+1);descended=true;break;
     }
     if(!descended)break;
    }
    throw Error(at,"module has no export: "+text);
   }
   throw Error(at,"unknown module qualifier: "+text);
  }
  if(names.modules.contains(text))throw Error(at,"module is not a first-class value: "+text);
   if(!type&&globals.contains(text)&&!is_intrinsic_name(text))throw Error(at,"name is not imported in this module: "+text);
   if(type&&type_globals.contains(text))throw Error(at,"type is not imported in this module: "+text);
   // Unknown unqualified names remain for the ordinary checker, or a PPX which
   // generates globals (derive/test). The loader never infers types or effects.
   }
    void cliche_reference(std::string& text,const Location& at,const Names& names){
    if(auto found=names.cliches.find(text);found!=names.cliches.end()){
     text=name(found->second);return;
    }
    if(auto dot=text.find('.');dot!=std::string::npos){
     for(auto cut=text.rfind('.');cut!=std::string::npos;
         cut=cut?text.rfind('.',cut-1):std::string::npos){
      auto module=names.modules.find(text.substr(0,cut));if(module==names.modules.end())continue;
      std::string member=text.substr(cut+1);auto* owner=module->second;
      while(true){
       if(auto found=owner->names.cliches.find(member);found!=owner->names.cliches.end()){
        text=name(found->second);return;
       }
       bool descended=false;
       for(auto split=member.rfind('.');split!=std::string::npos;
           split=split?member.rfind('.',split-1):std::string::npos){
        auto child=owner->names.modules.find(member.substr(0,split));
        if(child==owner->names.modules.end())continue;
        owner=child->second;member=member.substr(split+1);descended=true;break;
       }
       if(!descended)break;
      }
      throw Error(at,"module has no cliche export: "+text,"ON1X-CLICHE-IMPORT");
     }
     throw Error(at,"unknown module qualifier in cliche: "+text,"ON1X-CLICHE-IMPORT");
    }
    if(cliche_globals.contains(text))
     throw Error(at,"cliche is not imported in this module: "+text,"ON1X-CLICHE-IMPORT");
     // Preserve a source-level unknown name for the located cliche diagnostic.
    }
    std::optional<std::string> named_reference(const std::string& text,const Names& names,
                                               bool cliche)const{
     const auto direct=[&](const Module* module,const std::string& key)
       ->std::optional<std::string>{
      const auto& symbols=cliche?module->names.cliches:module->names.types;
      if(auto found=symbols.find(key);found!=symbols.end())return name(found->second);
      return {};
     };
     const auto& local=cliche?names.cliches:names.types;
     if(auto found=local.find(text);found!=local.end())return name(found->second);
     if(text.find('.')==std::string::npos)return {};
     for(auto cut=text.rfind('.');cut!=std::string::npos;
         cut=cut?text.rfind('.',cut-1):std::string::npos){
      auto module=names.modules.find(text.substr(0,cut));if(module==names.modules.end())continue;
      auto* owner=module->second;std::string member=text.substr(cut+1);
      while(true){
       if(auto found=direct(owner,member))return found;
       bool descended=false;
       for(auto split=member.rfind('.');split!=std::string::npos;
           split=split?member.rfind('.',split-1):std::string::npos){
        auto child=owner->names.modules.find(member.substr(0,split));
        if(child==owner->names.modules.end())continue;
        owner=child->second;member=member.substr(split+1);descended=true;break;
       }
       if(!descended)break;
      }
     }
     return {};
    }
  Type type_reference(const Type& t,const Location& at,const Names& names){
    if(t.is_own())return Type::own(type_reference(t.owned(),at,names));
    if(t.kind()==Type::Named){
     auto text=t.nominal();
     const auto dot=text.find('.');
     auto found=kernel_scope?kernel_scope->kernels.find(text.substr(0,dot)):std::map<std::string,KernelImport>::const_iterator{};
     if(dot!=std::string::npos&&kernel_scope&&found!=kernel_scope->kernels.end()){
      auto member=text.substr(dot+1);
      const bool history=found->second.module=="mulib.histprice", pricing=found->second.module=="mulib.priceopt";
         if(found->second.module==ipc_kernel_module&&member=="Result")text=ipc_kernel_result_name;
         else if(found->second.module==ipc_kernel_module&&member=="Session")text=ipc_kernel_session;
       else if(!((history||pricing)&&(member=="Error"||member=="Result"))&&!(history&&member=="Forecast")&&
          !(pricing&&(member=="DemandKind"||member=="Objective"||member=="DemandModel"||member=="Constraints"||member=="Decision")))
        throw Error(at,"ON1X-KERNEL-MEMBER: no kernel type "+text,"ON1X-KERNEL-MEMBER");
       else text=kernel_type_prefix(history)+member;
     }else reference(text,at,names,{},false,true);
    std::vector<Type> args;for(const auto& a:t.arguments())args.push_back(type_reference(a,at,names));
    return Type::named(std::move(text),std::move(args));
   }
    if(t.is_forall())return Type::for_all(t.generic_identity(),type_reference(t.body(),at,names));
    if(t.is_code())return Type::code(type_reference(t.code_result(),at,names));
    if(t.kind()==Type::Channel)
     return Type::channel(type_reference(t.csp_payload(),at,names),
                          t.sender_cardinality(),t.receiver_cardinality());
    if(t.kind()==Type::Sender)
     return Type::sender(type_reference(t.csp_payload(),at,names));
    if(t.kind()==Type::Receiver)
     return Type::receiver(type_reference(t.csp_payload(),at,names));
    if(t.is_function()){std::vector<Type> args;for(const auto& p:t.parameters())args.push_back(type_reference(p,at,names));return Type::function(std::move(args),type_reference(t.result(),at,names));}
   if(t.is_tuple()||t.is_data()){
    std::vector<Type> fields;for(const auto& f:t.elements())fields.push_back(type_reference(f,at,names));
    return t.is_tuple()?Type::tuple(std::move(fields)):Type::data(t.kind(),std::move(fields),t.labels(),t.extent());
   }
    return t;
   }
   void constraint(ClicheConstraint& item,const Names& names){
    cliche_reference(item.name,item.where,names);
    for(auto& argument:item.arguments)argument=type_reference(argument,item.where,names);
   }
   void constraints(std::vector<ClicheConstraint>& items,const Names& names){
    for(auto& item:items)constraint(item,names);
   }
 void macro(Macro& m,const Names& names,const Scope& values,const Scope& syntax){
  auto pattern=[&](auto&& self,MacroParameter& p)->void{
   if(p.literal)expression(*p.literal,names,values,syntax);
   for(auto& child:p.elements)self(self,child);
   if(p.tail)self(self,*p.tail);
  };
  for(auto& rule:m.rules){
   for(auto& p:rule.parameters)pattern(pattern,p);
   if(rule.tail)pattern(pattern,*rule.tail);
   if(rule.expression)expression(*rule.expression,names,values,syntax);
   body(rule.body,names,values,syntax);
  }
 }
   void expression(Expr& e,const Names& names,const Scope& values,const Scope& syntax){
    e.result=type_reference(e.result,e.where,names);
    for(auto& p:e.parameters)p.type=type_reference(p.type,p.where,names);
    for(auto& parameter:e.type_parameters)constraints(parameter.bounds,names);
    constraints(e.constraints,names);
    for(auto& argument:e.cliche_arguments)argument=type_reference(argument,e.where,names);
    for(auto& argument:e.member_type_arguments)argument=type_reference(argument,e.where,names);
     if(e.kind==Expr::Kind::ClicheMember){
      auto cliche=named_reference(e.text,names,true);
      auto nominal=named_reference(e.text,names,false);
      if(cliche&&nominal)
       throw Error(e.where,"ambiguous '::' owner is both a cliche and nominal type: "+e.text,
                   "ON1X-IMPL-AMBIGUOUS");
      if(nominal){
       if(!e.member_type_arguments.empty())
        throw Error(e.where,"explicit inherent member type arguments are not supported; infer them from the call",
                    "ON1X-IMPL-UNSUPPORTED");
       e.kind=Expr::Kind::InherentMember;
       e.result=Type::named(*nominal,std::move(e.cliche_arguments));
       e.text=std::move(e.cliche_member);
       e.cliche_arguments.clear();e.cliche_member.clear();
      }else cliche_reference(e.text,e.where,names);
     }
     if(e.kind==Expr::Kind::InherentMember&&e.result==Type::Void&&e.children.size()==1&&
        e.children[0]&&e.children[0]->kind==Expr::Kind::Variable){
      const auto& spelling=e.children[0]->text;
      const auto first=spelling.substr(0,spelling.find('.'));
      if(!values.contains(first)&&!names.values.contains(spelling))
       if(auto nominal=named_reference(spelling,names,false)){
       e.result=Type::named(*nominal);e.children.clear();
      }
     }
    if(e.kind==Expr::Kind::Variable){(void)kernel_reference(e,values,false);reference(e.text,e.where,names,values,false);}
   if(options.kernel_c11&&(e.kind==Expr::Kind::SyntaxLet||e.kind==Expr::Kind::MacroCall||
      e.kind==Expr::Kind::Quote||e.kind==Expr::Kind::Run||e.kind==Expr::Kind::Splice||e.kind==Expr::Kind::Lift))
    throw Error(e.where,"ON1X-KERNEL-PROFILE: staging and local syntax are outside this C11 profile","ON1X-KERNEL-PROFILE");
  if(e.kind==Expr::Kind::MacroCall){
   auto dot=e.text.find('.');
   if(dot!=std::string::npos&&values.contains(e.text.substr(0,dot)))throw Error(e.where,"qualified reference is shadowed by local binding: "+e.text.substr(0,dot));
   reference(e.text,e.where,names,syntax,true);
  }
  if(e.kind==Expr::Kind::SyntaxLet){
   auto definitions=syntax;
   if(e.text=="letrec_syntax")for(const auto& m:e.syntax_definitions)definitions.insert(m.name);
   for(auto& m:e.syntax_definitions)macro(m,names,values,definitions);
   auto inner=syntax;for(const auto& m:e.syntax_definitions)inner.insert(m.name);
   for(auto& child:e.children)expression(*child,names,values,inner);
   return;
  }
     if(e.kind==Expr::Kind::Lambda){auto inner=values;parameters(inner,e.parameters);
      auto saved=unsafe_context;unsafe_context=false;body(e.body,names,inner,syntax);unsafe_context=saved;return;}
    if(e.kind==Expr::Kind::UnsafeBlock){
     if(!options.kernel_c11)
      throw Error(e.where,"ON1X-KERNEL-PROFILE: unsafe blocks require KernelBind C11 mode","ON1X-KERNEL-PROFILE");
     if(e.children.size()!=1||!e.children[0])throw Error(e.where,"malformed unsafe expression");
     auto saved=unsafe_context;unsafe_context=true;
     auto inner=body(e.body,names,values,syntax);
     expression(*e.children[0],names,inner,syntax);
     unsafe_context=saved;return;
    }
   if(e.kind==Expr::Kind::PatternMatch){expression(*e.children.at(0),names,values,syntax);auto inner=values;if(!e.text.empty())inner.insert(e.text);for(std::size_t i=1;i<e.children.size();++i)expression(*e.children[i],names,inner,syntax);return;}
   if(e.kind==Expr::Kind::Case){auto inner=values;for(const auto& p:e.patterns)bind(inner,p);expression(*e.children.at(0),names,inner,syntax);body(e.body,names,inner,syntax);
    for(const auto& s:e.body)if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(inner,*s.pattern);else inner.insert(s.name);}
    expression(*e.children.at(1),names,inner,syntax);return;}
   if(e.callee&&!kernel_reference(*e.callee,values,true))expression(*e.callee,names,values,syntax);
  if(e.syntax_tail)expression(*e.syntax_tail,names,values,syntax);
  for(auto& child:e.children)expression(*child,names,values,syntax);
  body(e.body,names,values,syntax);
 }
  Scope body(std::vector<Stmt>& statements,const Names& names,const Scope& outer,const Scope& syntax){
  auto values=outer;
    for(auto& s:statements){
     s.declared=type_reference(s.declared,s.where,names);
     if((s.kind==Stmt::Kind::Break||s.kind==Stmt::Kind::Continue)&&!s.value&&s.label.empty()){
     std::string spelling=s.kind==Stmt::Kind::Break?"break":"continue";
     if(values.contains(spelling)||names.values.contains(spelling)){
      s.kind=Stmt::Kind::Expr;s.value=std::make_unique<Expr>();s.value->kind=Expr::Kind::Variable;s.value->text=spelling;s.value->where=s.where;
     }
    }
     if(options.kernel_c11&&(s.kind==Stmt::Kind::TaskGroup||s.kind==Stmt::Kind::Defer))
      throw Error(s.where,"ON1X-KERNEL-PROFILE: task groups and defer are outside this C11 profile","ON1X-KERNEL-PROFILE");
     if(s.unsafe_block&&!options.kernel_c11)
      throw Error(s.where,"ON1X-KERNEL-PROFILE: unsafe blocks require KernelBind C11 mode","ON1X-KERNEL-PROFILE");
     if(s.value)expression(*s.value,names,values,syntax);
   if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(values,*s.pattern);else values.insert(s.name);}
    if(s.kind==Stmt::Kind::Assign)reference(s.name,s.where,names,values,false);
    auto inner=values;if(s.kind==Stmt::Kind::Range)inner.insert(s.name);
     auto saved=unsafe_context;unsafe_context=unsafe_context||s.unsafe_block;
      body(s.body,names,inner,syntax);unsafe_context=saved;
     body(s.otherwise,names,values,syntax);
   }
   return values;
  }
  std::vector<InterfaceObligation> bind_interfaces(){
   std::set<fs::path> seen;
   std::vector<InterfaceObligation> obligations;
   for(const auto& path:options.interfaces){
    Location where{path.string()};
    if(path.extension()!=".on1xi"||!regular(path))
     throw Error(where,"expected a regular .on1xi file","ON1X-INTERFACE-FILE");
    auto origin=canonical(path,where);
    if(!seen.insert(origin).second)
     throw Error(where,"duplicate interface file","ON1X-INTERFACE-DUPLICATE");
    if(origin.extension()!=".on1xi")
     throw Error(where,"interface must resolve to a .on1xi file","ON1X-INTERFACE-FILE");
    where.file=origin.string();
    auto implementation=origin;implementation.replace_extension(".on1x");
    // Never resolve by search precedence, import alias, or re-export. The
    // same-stem file itself must have been selected in this loaded graph.
    auto it=modules.find(implementation);
    if(it==modules.end()||!regular(implementation))
     throw Error(where,"matching implementation is not in the loaded graph: "+implementation.string(),
                 "ON1X-INTERFACE-IMPLEMENTATION");
    auto& module=*it->second;
    auto source=read_source_file(where.file);
    if(source.size()>options.max_bytes-bytes)
     throw Error(where,"total source and interface byte limit exceeded","ON1X-INTERFACE-LIMIT");
    bytes+=source.size();
    auto members=parse_interface(scan(source,where.file));
    std::map<std::string,std::vector<const Function*>> definitions;
    for(const auto& f:module.unit.functions)definitions[f.name].push_back(&f);
    for(auto& member:members){
     auto definition=definitions.find(member.name);
     if(definition==definitions.end()||definition->second.size()!=1||definition->second.front()->external)
      throw Error(member.where,"expected one local On1x definition of '"+member.name+"' in "+implementation.string()+
                  " (externs and re-exports do not satisfy interfaces)","ON1X-INTERFACE-MEMBER");
     const auto& f=*definition->second.front();
     if(f.result.has_holes()||std::any_of(f.params.begin(),f.params.end(),[](const auto& p){return p.type.has_holes();}))
      throw Error(f.where,"interface member '"+member.name+"' requires a fully annotated signature (no inferred types)",
                  "ON1X-INTERFACE-ANNOTATION");
     auto resolved=name({&module,f.name});
     obligations.push_back({std::move(member),f.where,std::move(resolved)});
    }
   }
   return obligations;
  }
public:
   explicit Loader(const LoaderOptions& opts,KernelImportRequests* inspect=nullptr)
    :options(opts),standard_roots(standard_library_roots(opts)),inspection(inspect){
     for(const auto& root:standard_roots)
      for(const auto& [kind,file]:shared_modules){
       std::error_code ec;
       auto path=fs::canonical(root/"container"/file,ec);
       if(!ec)shared_sources.emplace(std::move(path),kind);
      }
    }
 LoadedUnit run(const fs::path& input){
   if(!inspection&&options.ffi_stage&&(!options.kernel_c11||options.kernel_stage))
    throw Error({input.string()},"FFI staging requires the direct C11 profile and excludes legacy staging",
                "ON1X-KERNEL-PROFILE");
   if(options.interfaces.size()>max_interface_files)
    throw Error({input.string()},"interface file count limit exceeded","ON1X-INTERFACE-LIMIT");
   if(!options.interfaces.empty()&&(inspection||options.kernel_c11||!options.extensions.enabled.empty()))
    throw Error({input.string()},"interface verification cannot run with KernelBind or declaration extensions",
                "ON1X-INTERFACE-UNSUPPORTED");
  entry_directory=canonical(input,{input.string()}).parent_path();
  if(options.prelude){
    auto path=standard("pervasive",{"<prelude>"});
    if(path.empty())throw Error({input.string()},"automatic prelude requires --stdlib or --no-prelude (pervasive.on1x not found)");
    implicit=true;preludes.push_back(visit(path,{"<prelude>"},false));implicit=false;
  }
  for(const auto& path:options.preludes)preludes.push_back(visit(path,{"<prelude>"},false));
   auto* entry=visit(input,{input.string()},false);
   if(inspection)return {};
   legacy(entry);
  // Pick an identifier prefix absent from every token, including local binders.
  while(std::any_of(tokens.begin(),tokens.end(),[&](const auto& t){return t.starts_with(prefix);}))prefix+="m_";
  Names flat;for(auto* m:order)if(m->legacy)merge(flat,m->names,{m->path.string()});
    for(auto* m:order)for(const auto& f:m->unit.functions)globals.insert(name({m,f.name,f.external}));
    for(auto* m:order)for(const auto& d:m->unit.nominals)type_globals.insert(name({m,d.name}));
    for(auto* m:order)for(const auto& c:m->unit.cliches)cliche_globals.insert(name({m,c.name}));
  LoadedUnit result;std::map<std::string,const Function*> externs;std::set<std::string> lazy;
   result.interfaces=bind_interfaces();
  // Reserve storage so pointers used for identical raw-extern admission stay valid.
   std::size_t count=0;
   for(auto* m:order){
    count+=m->unit.functions.size();
    for(const auto& implementation:m->unit.cliche_implementations)
     count+=implementation.members.size();
    for(const auto& implementation:m->unit.inherent_implementations)
     count+=implementation.members.size();
   }
   if(count>16384)throw Error({input.string()},"loaded function count limit exceeded");
   result.unit.functions.reserve(count);
   for(auto* m:order){
    kernel_scope=m;
    auto names=m->legacy?flat:m->names;
     for(const auto& [alias,request]:m->kernels)if(module_bound(names,alias)||names.values.contains(alias)||
      names.syntax.contains(alias)||names.types.contains(alias)||names.cliches.contains(alias))
      throw Error(request.where,"conflicting kernel import alias: "+alias,"ON1X-KERNEL-ALIAS");
   for(auto it=preludes.rbegin();it!=preludes.rend();++it){
    auto* prelude=*it;
    for(const auto& [key,symbol]:prelude->names.values)if(!names.values.contains(key)&&!module_bound(names,key))insert(names.values,key,symbol,{m->path.string()});
     for(const auto& [key,symbol]:prelude->names.syntax)if(!names.syntax.contains(key)&&!module_bound(names,key))insert(names.syntax,key,symbol,{m->path.string()});
      for(const auto& [key,symbol]:prelude->names.types)if(!names.types.contains(key)&&!module_bound(names,key))insert(names.types,key,symbol,{m->path.string()});
      for(const auto& [key,symbol]:prelude->names.cliches)if(!names.cliches.contains(key)&&!module_bound(names,key))insert(names.cliches,key,symbol,{m->path.string()});
   }
     for(auto& d:m->unit.nominals){
      for(auto& field:d.fields)field=type_reference(field,d.where,names);
      d.name=name({m,d.name});result.unit.nominals.push_back(std::move(d));
     }
     for(auto& c:m->unit.cliches){
      constraints(c.constraints,names);
      for(auto& parameter:c.type_parameters)constraints(parameter.bounds,names);
      for(auto& member:c.members){
       for(auto& parameter:member.type_parameters)constraints(parameter.bounds,names);
       constraints(member.constraints,names);
       for(auto& parameter:member.params)parameter.type=type_reference(parameter.type,parameter.where,names);
       member.result=type_reference(member.result,member.where,names);
      }
      c.name=name({m,c.name});result.unit.cliches.push_back(std::move(c));
     }
      for(auto& implementation:m->unit.cliche_implementations){
      cliche_reference(implementation.name,implementation.where,names);
      for(auto& parameter:implementation.type_parameters)constraints(parameter.bounds,names);
      for(auto& argument:implementation.arguments)
       argument=type_reference(argument,implementation.where,names);
      constraints(implementation.constraints,names);
      for(auto& member:implementation.members){
       auto& function=member.function;
       for(auto& parameter:function.type_parameters)constraints(parameter.bounds,names);
       constraints(function.constraints,names);
       for(auto& parameter:function.params)
        parameter.type=type_reference(parameter.type,parameter.where,names);
       function.result=type_reference(function.result,function.where,names);
       member.value_type=type_reference(member.value_type,member.where,names);
       Scope locals;parameters(locals,function.params);
       body(function.body,names,locals,{});
       if(member.value)expression(*member.value,names,{}, {});
      }
       result.unit.cliche_implementations.push_back(std::move(implementation));
      }
      for(std::size_t implementation_index=0;
          implementation_index<m->unit.inherent_implementations.size();
          ++implementation_index){
       auto& implementation=m->unit.inherent_implementations[implementation_index];
       for(auto& parameter:implementation.type_parameters)
        constraints(parameter.bounds,names);
       implementation.target=type_reference(implementation.target,
                                             implementation.where,names);
       for(std::size_t member_index=0;member_index<implementation.members.size();
           ++member_index){
        auto& member=implementation.members[member_index];
         member.runtime_name="@inherent_"+
                             std::to_string(result.unit.inherent_implementations.size())+"_"+
                             std::to_string(member_index);
        for(auto& parameter:member.type_parameters)
         constraints(parameter.bounds,names);
        auto& function=member.function;
        for(auto& parameter:function.type_parameters)
         constraints(parameter.bounds,names);
        constraints(function.constraints,names);
        for(auto& parameter:function.params)
         parameter.type=type_reference(parameter.type,parameter.where,names);
        function.result=type_reference(function.result,function.where,names);
        member.value_type=type_reference(member.value_type,member.where,names);
        Scope locals;parameters(locals,function.params);
        body(function.body,names,locals,{});
        if(member.value)expression(*member.value,names,locals,{});
       }
       result.unit.inherent_implementations.push_back(std::move(implementation));
      }
      for(auto& f:m->unit.functions){
      f.result=type_reference(f.result,f.where,names);for(auto& p:f.params)p.type=type_reference(p.type,p.where,names);
      for(auto& parameter:f.type_parameters)constraints(parameter.bounds,names);
      constraints(f.constraints,names);
    Scope locals;parameters(locals,f.params);body(f.body,names,locals,{});
    for(auto& attr:f.attributes)expression(*attr,names,locals,{});
    f.name=name({m,f.name,f.external});
    if(m->implicit)lazy.insert(f.name);
    if(!m->legacy)result.internal_functions.insert(f.name);
    if(f.external){
     auto [it,added]=externs.emplace(f.name,&f);
     if(!added){
      std::vector<Type> left,right;
      for(const auto& p:f.params)left.push_back(p.type);
      for(const auto& p:it->second->params)right.push_back(p.type);
      if(left!=right||f.result!=it->second->result||extern_parameter_types(f)!=extern_parameter_types(*it->second)||!f.attributes.empty()||!it->second->attributes.empty())
       throw Error(f.where,"conflicting raw extern declarations: "+f.name);
      continue;
     }
     result.unit.functions.push_back(std::move(f));it->second=&result.unit.functions.back();
    }else result.unit.functions.push_back(std::move(f));
   }
   for(auto& definition:m->unit.macros){macro(definition,names,{},{});definition.name=name({m,definition.name});result.unit.macros.push_back(std::move(definition));}
    for(auto& registration:m->unit.ppx)result.unit.ppx.push_back(std::move(registration));
     for(auto& carrier:m->unit.extensions)result.unit.extensions.push_back(std::move(carrier));
     for(auto& feature:m->unit.features)result.unit.features.push_back(std::move(feature));
     for(auto& directive:m->unit.scrutiny)result.unit.scrutiny.push_back(std::move(directive));
    for(auto& evidence:m->unit.extension_evidence){
     for(auto& implementation:evidence.implementations)for(auto& binding:implementation.bindings){
      binding.function=name({m,binding.function});
      binding.selector=name({m,binding.selector});
     }
     result.unit.extension_evidence.push_back(std::move(evidence));
    }
    result.unit.source_digests.insert(m->unit.source_digests.begin(),m->unit.source_digests.end());
     if(regular(m->path)){
      result.dependencies.push_back(m->path);
      result.dependency_sources.push_back(m->source);
     }
   }
     bool history_types=false,pricing_types=false,ipc_types=false;
    for(const auto* m:order)for(const auto& [alias,request]:m->kernels){
     (void)alias;
     if(!history_types&&request.module=="mulib.histprice"){
      history_types=true;append_kernel_nominals(result.unit,kernel_type_prefix(true),true,request.where);
     }
      if(!pricing_types&&request.module=="mulib.priceopt"){
       pricing_types=true;append_kernel_nominals(result.unit,kernel_type_prefix(false),false,request.where);
      }
       if(!ipc_types&&request.module==ipc_kernel_module){
       ipc_types=true;append_ipc_kernel_nominals(result.unit,request.where);
      }
    }
    if(kernel_calls.size()>16384-count)throw Error({input.string()},"loaded function count limit exceeded");
    for(auto& [name,f]:kernel_calls){(void)name;result.unit.functions.push_back(std::move(f));}
  // Automatic pervasive bindings are demand-loaded. This preserves the existing
  // annotated-only checking path and keeps unused generic helpers out of PPX/MIR.
  // Explicit imports and --prelude files always retain/check their definitions.
  std::map<std::string,std::set<std::string>> calls;
  auto collect=[&](auto&& self,const Expr& e,std::set<std::string>& refs)->void{
   if(e.kind==Expr::Kind::Variable)refs.insert(e.text);
   if(e.callee)self(self,*e.callee,refs);
   if(e.syntax_tail)self(self,*e.syntax_tail,refs);
   for(const auto& child:e.children)self(self,*child,refs);
   auto statements=[&](auto&& walk,const std::vector<Stmt>& body)->void{
    for(const auto& s:body){if(s.value)self(self,*s.value,refs);walk(walk,s.body);walk(walk,s.otherwise);}
   };statements(statements,e.body);
   for(const auto& m:e.syntax_definitions)for(const auto& r:m.rules){if(r.expression)self(self,*r.expression,refs);statements(statements,r.body);}
  };
  auto statements=[&](auto&& self,const std::vector<Stmt>& body,std::set<std::string>& refs)->void{
   for(const auto& s:body){if(s.value)collect(collect,*s.value,refs);self(self,s.body,refs);self(self,s.otherwise,refs);}
  };
  std::set<std::string> needed;
   for(const auto& obligation:result.interfaces)needed.insert(obligation.resolved_name);
   for(const auto& evidence:result.unit.extension_evidence){
    for(const auto& implementation:evidence.implementations)for(const auto& binding:implementation.bindings){
     needed.insert(binding.function);needed.insert(binding.selector);
    }
   }
   for(const auto& f:result.unit.functions){
    auto& refs=calls[f.name];statements(statements,f.body,refs);for(const auto& a:f.attributes)collect(collect,*a,refs);
    if(!lazy.contains(f.name))needed.insert(f.name);
   }
   for(const auto& implementation:result.unit.cliche_implementations)
    for(const auto& member:implementation.members){
     statements(statements,member.function.body,needed);
     if(member.value)collect(collect,*member.value,needed);
    }
   for(const auto& implementation:result.unit.inherent_implementations)
    for(const auto& member:implementation.members){
     statements(statements,member.function.body,needed);
     if(member.value)collect(collect,*member.value,needed);
    }
   for(const auto& m:result.unit.macros)for(const auto& r:m.rules){if(r.expression)collect(collect,*r.expression,needed);statements(statements,r.body,needed);}
  std::vector<std::string> pending(needed.begin(),needed.end());
  while(!pending.empty()){
   auto current=std::move(pending.back());pending.pop_back();
   for(const auto& target:calls[current])if(needed.insert(target).second)pending.push_back(target);
  }
  std::erase_if(result.unit.functions,[&](const Function& f){return lazy.contains(f.name)&&!needed.contains(f.name);});
  return result;
 }
};
}
LoaderOptions default_loader_options(const std::string& executable){
  LoaderOptions options;
   fs::path binary=executable;
  if(!binary.has_parent_path())if(const char* path=std::getenv("PATH")){
   std::istringstream search(path);std::string dir;
   while(std::getline(search,dir,':'))if(!dir.empty()&&regular(fs::path(dir)/binary)){binary=fs::path(dir)/binary;break;}
  }
   std::error_code ec;auto resolved=fs::canonical(binary,ec);
   // Native authorization must not inherit argv[0], PATH, ON1X_STDLIB or CWD.
   // Other hosts need an audited executable-location API before auto-activation.
#if defined(__linux__)
   std::error_code self_error;auto self=fs::canonical("/proc/self/exe",self_error);
   if(!self_error){
    if(directory(self.parent_path()/"../share/on1x"))options.extensions.directory=self.parent_path()/"../libexec/on1x";
    else options.extensions.directory=self.parent_path()/"stdppx";
   }
#endif
  if(const char* root=std::getenv("ON1X_STDLIB");root&&*root)options.stdlib=root;
  else{
  if(!ec&&directory(resolved.parent_path()/"../share/on1x/stdlib"))options.stdlib=resolved.parent_path()/"../share/on1x/stdlib";
#ifdef ON1X_STDLIB_ROOT
  else options.stdlib=ON1X_STDLIB_ROOT;
#endif
 }
 discovery::environment(options.stdlib_paths,"ON1X_STDLIB_PATH");
 discovery::environment(options.paths,"ON1X_LIB_PATH");
#ifdef _WIN32
 // Keep the old alias's drive-letter/semicolon spelling. Only the new lists
 // mandate colons independently of the host.
 if(const char* path=std::getenv("ON1X_PATH")){
  std::istringstream search(path);std::string dir;
  while(std::getline(search,dir,';'))if(!dir.empty())options.paths.emplace_back(dir);
 }
#else
 discovery::environment(options.paths,"ON1X_PATH");
#endif
 return options;
}
std::vector<fs::path> standard_library_roots(const LoaderOptions& options){
 if(options.paths.size()+options.stdlib_paths.size()>256)throw Error({"<search-path>"},"library search path count limit exceeded");
 std::vector<fs::path> result;
 auto add=[&](const fs::path& path){
  if(path.empty())return;
  std::error_code ec;auto normalized=fs::weakly_canonical(path,ec);
  if(ec)normalized=path.lexically_normal();
  if(std::find(result.begin(),result.end(),normalized)==result.end())result.push_back(normalized);
 };
 if(options.stdlib_explicit){
  auto root=discovery::stdlib(options.stdlib);
  add(root.empty()?options.stdlib:root);
 }
 for(const auto& path:options.stdlib_paths){
  auto root=discovery::stdlib(path);
  if(!root.empty())add(root);
  else if(directory(path))throw Error({(path/"On1xfile").string()},"stdlib search directory requires an On1xfile manifest");
 }
 for(const auto& path:options.paths)add(discovery::stdlib(path));
 if(!options.stdlib.empty()&&!options.stdlib_explicit){
  auto root=discovery::stdlib(options.stdlib);
  add(root.empty()?options.stdlib:root); // Legacy --stdlib/ON1X_STDLIB compatibility.
 }
 return result;
}
LoadedUnit load_unit(const fs::path& input,const LoaderOptions& options){return Loader(options).run(input);}
KernelImportRequests inspect_kernel_imports(const fs::path& input,const LoaderOptions& options){
 auto inspect_options=options;
 inspect_options.extensions.execute=false;
 KernelImportRequests result;
 (void)Loader(inspect_options,&result).run(input);
 return result;
}
}

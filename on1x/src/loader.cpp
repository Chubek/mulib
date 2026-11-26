#include <on1x/loader.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/discovery.hpp>
#include <on1x/operator_scope.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace on1x {
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
 std::vector<Token> scanned;
 std::vector<Docstring> docs;
 OperatorDiscovery discovery;
 OperatorScope operator_exports,operator_visibility;
 // Direct dependencies that affect parsing, in discovery import order.
 std::vector<Module*> operator_targets;
 Unit unit;
 Names names;
 std::vector<Module*> quoted;
  std::vector<Module*> dependencies;
   std::map<std::string,KernelImport> kernels;
   std::map<std::string,ForeignImport> foreigns;
 std::size_t id=0;
   bool discovering=true,active=false,loaded=false,legacy=false,implicit=false;
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
void require_kernel_import_policy(const KernelImport& request){
 if(request.module==ipc_kernel_module&&!request.is_unsafe)
  throw Error(request.where,"ON1X-KERNEL-NOT-ADMITTED: "+request.module+
               " is unsafe-only; use unsafe import kernel mulib.ipc_framed",
              "ON1X-KERNEL-NOT-ADMITTED");
}

class Loader {
 using Scope=std::set<std::string>;
  const LoaderOptions& options;
  fs::path entry_directory;
  std::vector<fs::path> standard_roots;
  std::map<fs::path,SharedKind> shared_sources;
 std::map<fs::path,std::unique_ptr<Module>> modules;
  std::vector<Module*> order,stack,preludes,operator_preludes,discovery_stack;
  std::set<std::string> tokens,globals,type_globals,cliche_globals;
 std::string prefix="on1x_module_";
 std::size_t bytes=0,bindings=0,edges=0,discovery_edges=0,next_module_id=0;
  std::size_t operator_binding_bytes=0;
 bool implicit=false;
  KernelImportRequests* inspection;
   const Module* current_module=nullptr;
   bool unsafe_context=false;
   std::size_t kernel_requests=0;
   std::map<std::string,Function> kernel_calls;
   std::optional<std::vector<fs::path>> foreign_roots;
   std::map<std::string,stdffi::SelectedLibrary> foreign_libraries;
   std::map<std::pair<std::string,std::string>,Function> foreign_functions;
   std::set<std::string> foreign_function_names;
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
   bool foreign_reference(Expr& e,const Scope& locals){
    if(!current_module)return false;
    const auto dot=e.text.find('.');
    if(dot==std::string::npos)return false;
    const auto alias=e.text.substr(0,dot);
    const auto found=current_module->foreigns.find(alias);
    if(found==current_module->foreigns.end()||locals.contains(alias))return false;
     const auto& kernel=found->second.kernel;
     auto key=std::make_pair(kernel,e.text.substr(dot+1));
     auto function=foreign_functions.find(key);
     if(options.kernel_load_time){
      if(function==foreign_functions.end()){
       const auto arity=load_time_kernel_arity(key.second);
       if(!arity)
        throw Error(e.where,"no load-time stdffi export "+e.text,"ON1X-STDFFI-MEMBER");
       // Admit only a checked compiler-owned export, including in source-only
       // checks that never reach C11 lowering.
       (void)stdffi::load_time_pin(key.second,arity,e.where);
       Function f;
       f.name=load_time_kernel_function(key.second);
       f.where=e.where;
       f.result=Type::I64;
       f.external=true;
       f.kernel_symbol=std::string(load_time_kernel_prefix)+key.second;
       append_parameters(f,std::vector<Type>(arity,Type::I64));
       function=foreign_functions.emplace(std::move(key),std::move(f)).first;
      }
      e.text=function->second.name;
      return true;
     }
     if(function==foreign_functions.end()){
      const auto& library=foreign_libraries.at(kernel);
      auto materialized=library.materialize(key.second,e.where);
     if(!materialized)
      throw Error(e.where,"no portable stdffi export "+e.text+" in "+library.path().string(),"ON1X-STDFFI-MEMBER");
     // Internal identifiers are unspellable in source, and cannot designate a
     // function from the disjoint native KernelBind namespace.
     if(!materialized->name.starts_with("@stdffi_")||materialized->external||
        !materialized->kernel_symbol.empty()||materialized->kernel_link_only||
        materialized->prototype||materialized->body.empty()||
        !foreign_function_names.insert(materialized->name).second)
      throw Error(e.where,"invalid internal definition for stdffi export "+e.text,"ON1X-STDFFI-DEFINITION");
     function=foreign_functions.emplace(std::move(key),std::move(*materialized)).first;
    }
    e.text=function->second.name;
    return true;
   }
     void admit_foreign(const ForeignImport& request){
      if(options.kernel_load_time){
       if("stdffi."+request.kernel!=load_time_kernel_module||!stdffi::has_kernel(request.kernel))
        throw Error(request.where,"no supported load-time stdffi kernel stdffi."+request.kernel,
                    "ON1X-STDFFI-KERNEL");
       return;
      }
      if(foreign_libraries.contains(request.kernel))return;
     if(!foreign_roots){
      foreign_roots.emplace();
      discovery::environment(*foreign_roots,"MULIB_FOREIGNLIB_PATH");
      if(foreign_roots->empty())*foreign_roots=options.foreign_default_paths;
      if(foreign_roots->empty()){
#ifdef ON1X_BUILT_STDFFI_ROOT
        foreign_roots->emplace_back(ON1X_BUILT_STDFFI_ROOT);
#endif
      }
      if(foreign_roots->size()>256)throw Error(request.where,"foreign library search path count limit exceeded");
     }
     auto absent=[&]{
      throw Error(request.where,"no portable stdffi kernel stdffi."+request.kernel+
                  " (looked for "+request.kernel+".ffi in MULIB_FOREIGNLIB_PATH or the standard FFI directories)",
                  "ON1X-STDFFI-KERNEL");
     };
     // Source identifiers are single components; additionally cap filename
     // length so an overlong identifier never becomes a filesystem lookup.
     if(request.kernel.size()>128)absent();
     const auto recipe=request.kernel+".ffi", catalogue="mulib-"+recipe;
     for(const auto& root:*foreign_roots)
      for(const auto* file:{&recipe,&catalogue}){
       auto path=root/(*file);
       std::error_code ec;
       auto status=fs::status(path,ec);
       if(ec==std::errc::no_such_file_or_directory||ec==std::errc::not_a_directory)continue;
       if(ec)throw Error(request.where,"cannot inspect stdffi file '"+path.string()+"': "+ec.message(),"ON1X-STDFFI-SPEC");
       if(!fs::exists(status))continue;
       if(!fs::is_regular_file(status))
        throw Error(request.where,"stdffi file is not a regular file: "+path.string(),"ON1X-STDFFI-SPEC");
       foreign_libraries.emplace(request.kernel,stdffi::SelectedLibrary::load(path,request.kernel,request.where));
       return;
      }
     absent();
    }
   bool kernel_reference(Expr& e,const Scope& values,bool direct){
    if(!current_module||e.kind!=Expr::Kind::Variable)return false;
    const auto dot=e.text.find('.');
    const auto alias=e.text.substr(0,dot);
    auto found=current_module->kernels.find(alias);
    if(found==current_module->kernels.end()||values.contains(alias))return false;
    if(dot==std::string::npos||!direct){
     throw Error(e.where,"ON1X-KERNEL-DIRECT: kernel exports must be called directly","ON1X-KERNEL-DIRECT");
    }
    const std::string_view member=std::string_view(e.text).substr(dot+1);
    const auto& request=found->second;
    const auto symbol=legacy_kernel_member(request.module,member);
    if(symbol.empty()){
     throw Error(e.where,"ON1X-KERNEL-MEMBER: no ABI-verified export "+e.text,"ON1X-KERNEL-MEMBER");
    }
    const bool unsafe=request.is_unsafe;
    if(unsafe&&!unsafe_context)
     throw Error(e.where,"ON1X-KERNEL-CONTEXT: "+e.text+" requires unsafe { ... }","ON1X-KERNEL-CONTEXT");
    const bool history=history_kernel_symbol(symbol),pricing=pricing_kernel_symbol(symbol),ipc=ipc_kernel_symbol(symbol);
    auto name=(unsafe?"@kernel_":"@kernel_scalar_")+std::string(symbol);
    if(!kernel_calls.contains(name)){
     Function f;f.name=name;f.where=e.where;
     f.result=ipc?ipc_kernel_result(symbol):history||pricing?kernel_result(history):Type{Type::I64};f.external=true;
     f.kernel_symbol=symbol;f.kernel_link_only=!unsafe;
     auto params=ipc?ipc_kernel_parameters(symbol):
        history?history_kernel_parameters(symbol):pricing?pricing_kernel_parameters(symbol,kernel_type_prefix(false)):
       std::vector<Type>(clamp_kernel_arity(symbol),Type::I64);
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
   fs::path candidate(const fs::path& base,const Location& at,bool strict=false){
   auto file=base;file+=".on1x";auto init=base/"__init__.on1x";
   const bool has_file=regular(file),has_init=regular(init);
   const bool has_directory=(strict||(!has_file&&!has_init))&&directory(base);
   if(strict){
    // Falling back to no operator prelude is safe only for an absent module,
    // not for an invalid file, package, dangling symlink or filesystem error.
    const auto require_kind=[&](const fs::path& path,bool expected){
     if(expected)return;
     std::error_code ec;
     const auto status=fs::symlink_status(path,ec);
     if(ec==std::errc::no_such_file_or_directory||ec==std::errc::not_a_directory)return;
     if(ec)throw Error(at,"cannot inspect standard operator module '"+path.string()+"': "+ec.message());
     if(fs::exists(status))throw Error(at,"invalid standard operator module path: "+path.string());
    };
    require_kind(file,has_file);require_kind(base,has_directory);require_kind(init,has_init);
    if(has_directory&&!has_file&&!has_init)
     throw Error(at,"standard operator module has no initializer: "+init.string());
   }
   if(has_file&&has_init)throw Error(at,"ambiguous module: both "+file.string()+" and "+init.string());
   if(has_file)return file;
   if(has_init)return init;
   if(has_directory)return base; // Namespace package, with no executable initializer.
   return {};
  }
  bool import_alias_conflicts(const Names& names,const std::string& alias)const{
   return module_bound(names,alias)||names.values.contains(alias)||names.types.contains(alias)||
          names.syntax.contains(alias)||names.cliches.contains(alias);
  }
   fs::path standard(const std::string& module,const Location& at){
    for(const auto& root:standard_roots){
      const bool operators=module=="operators";
      auto first=candidate(root/module_path(module),at,operators);
     // Compatibility with older installed extension trees; the canonical MuLIB
     // package now lives inside stdlib, not in this legacy sibling directory.
      auto second=candidate(root.parent_path()/"std"/module_path(module),at,operators);
      if(!first.empty()&&!second.empty()){
       if(first==second||canonical(first,at)==canonical(second,at))return first;
       throw Error(at,"ambiguous standard module: "+module);
      }
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
  bool wants_operators(const Import& imp)const{
   return imp.quoted||(imp.selective&&std::any_of(imp.names.begin(),imp.names.end(),
                          [](const ImportName& n){return n.is_operator;}));
  }
  bool self_package(const Import& imp,const Module& parent,const fs::path& target)const{
   return imp.relative&&imp.module.empty()&&canonical(target,imp.where)==parent.path;
  }
  void add_operator(OperatorScope& into,const OperatorBinding& binding,const Location& at){
   if(!into.add(binding,at))return;
   charge(at);
   constexpr std::size_t max_operator_binding_bytes=16*1024*1024;
   const auto size=sizeof(binding)+binding.identity.size()+binding.header.where.file.size()+
                    2*binding.header.lexeme.size()+binding.header.lexeme2.size();
   if(size>max_operator_binding_bytes-operator_binding_bytes)
    throw Error(at,"module operator scope byte limit exceeded","ON1X-OPERATOR-LIMIT");
   operator_binding_bytes+=size;
  }
  static bool same_import(const Import& a,const Import& b){
   return a.where.file==b.where.file&&a.where.offset==b.where.offset&&a.module==b.module&&
          a.alias==b.alias&&a.relative==b.relative&&a.quoted==b.quoted&&a.selective==b.selective&&
          std::equal(a.names.begin(),a.names.end(),b.names.begin(),b.names.end(),
                     [](const ImportName& x,const ImportName& y){
                      return x.name==y.name&&x.alias==y.alias&&x.is_operator==y.is_operator;
                     });
  }
  // Read immutable source and discover only declarations/imports. In this
  // phase no function body is parsed and no declaration extension runs.
  Module* metadata(const fs::path& input,const Location& at,bool allow_directory=true){
   auto path=canonical(input,at);
   const bool source=regular(path);
   if(!source&&!(allow_directory&&directory(path)))
    throw Error(at,"import is not a regular source file: "+path.string());
   if(auto hit=modules.find(path);hit!=modules.end()){
    if(hit->second->discovering){
     std::string chain;for(const auto* m:discovery_stack)chain+=m->path.string()+" -> ";
     throw Error(at,"import cycle: "+chain+path.string());
    }
    return hit->second.get();
   }
   if(modules.size()>=options.max_modules)throw Error(at,"module count limit exceeded");
   if(stack.size()+discovery_stack.size()>=options.max_depth)
    throw Error(at,"import recursion limit exceeded");
   auto storage=std::make_unique<Module>();auto* m=storage.get();m->path=path;
   modules.emplace(path,std::move(storage));discovery_stack.push_back(m);
   if(source){
    auto replacement=options.source_overrides.find(path);
    m->source=replacement==options.source_overrides.end()?read_source_file(path.string()):replacement->second;
    if(m->source.size()>options.max_bytes-bytes)throw Error(at,"total imported source byte limit exceeded");
    bytes+=m->source.size();m->scanned=scan(m->source,path.string(),&m->docs);
    for(const auto& t:m->scanned)tokens.insert(t.text);
    m->discovery=discover_operators(m->scanned);
    if(m->discovery.imports.size()>16384-discovery_edges)throw Error(at,"import edge limit exceeded");
    discovery_edges+=m->discovery.imports.size();
    if(options.standalone_guest&&!m->discovery.imports.empty())
     throw Error(m->discovery.imports.front().where,"standalone ArchVirt source cannot import or activate extensions");
    for(const auto& header:m->discovery.operators)
     add_operator(m->operator_exports,{header,operator_identity(header)},header.where);
    m->operator_targets.resize(m->discovery.imports.size());
    for(std::size_t i=0;i<m->discovery.imports.size();++i){
     const auto& imp=m->discovery.imports[i];
     if(!wants_operators(imp))continue;
     auto target_path=resolve(imp,*m);
     auto* target=self_package(imp,*m,target_path)?nullptr:metadata(target_path,imp.where,!imp.quoted);
     m->operator_targets[i]=target;
     if(imp.quoted){
      target->operator_exports.each([&](const auto& binding){add_operator(m->operator_exports,binding,imp.where);});
      continue;
     }
     for(const auto& selection:imp.names){
      if(!selection.is_operator)continue;
      const auto* prefix=target?target->operator_exports.find(selection.name,OperatorPosition::Prefix):nullptr;
      const auto* trailing=target?target->operator_exports.trailing(selection.name):nullptr;
      if(!prefix&&!trailing)
       throw Error(imp.where,"module '"+imp.module+"' has no operator export '"+selection.name+"'",
                   "ON1X-OPERATOR-IMPORT");
      if(prefix)add_operator(m->operator_exports,*prefix,imp.where);
      if(trailing)add_operator(m->operator_exports,*trailing,imp.where);
     }
    }
   }
   discovery_stack.pop_back();m->discovering=false;return m;
  }
  Module* visit(const fs::path& input,const Location& at,bool allow_directory=true){
   return visit(metadata(input,at,allow_directory),at);
  }
   OperatorScope visible_operators(const Module& module,const Location& at){
    auto scope=module.operator_exports;
    for(auto it=operator_preludes.rbegin();it!=operator_preludes.rend();++it){
     (*it)->operator_exports.each([&](const auto& binding){
      const auto& header=binding.header;
      const auto* existing=header.position==OperatorPosition::Prefix?
       scope.find(header.lexeme,OperatorPosition::Prefix):scope.trailing(header.lexeme);
      if(!existing)add_operator(scope,binding,at);
     });
    }
    return scope;
   }
   Module* visit(Module* m,const Location& at){
   const auto& path=m->path;
   if(m->active){
    std::string chain;for(const auto* parent:stack)chain+=parent->path.string()+" -> ";
    throw Error(at,"import cycle: "+chain+path.string());
   }
   if(m->loaded){
    if(!implicit)explicit_module(m);
    return m;
   }
   if(stack.size()>=options.max_depth)throw Error(at,"import recursion limit exceeded");
   m->id=next_module_id++;m->implicit=implicit;m->active=true;stack.push_back(m);
   if(!m->scanned.empty()){
     m->operator_visibility=visible_operators(*m,at);
    m->unit=parse_with_operators(m->scanned,{options.extensions.enabled.begin(),options.extensions.enabled.end()},
                                 std::move(m->docs),m->operator_visibility);
    std::vector<Token>().swap(m->scanned);
    const auto module_operators=std::count_if(m->unit.operators.begin(),m->unit.operators.end(),
                                               [](const auto& op){return !op.block_local;});
    if(!std::equal(m->unit.module_imports.begin(),m->unit.module_imports.end(),
                  m->discovery.imports.begin(),m->discovery.imports.end(),same_import)||
       static_cast<std::size_t>(module_operators)!=m->discovery.operators.size())
     throw Error(at,"inconsistent operator discovery and parse","ON1X-OPERATOR-DISCOVERY");
    for(auto& op:m->unit.operators){
     if(op.block_local){
      op.function.name=op.identity=operator_identity(op.header);
      continue;
     }
     const auto* binding=m->operator_exports.find(op.header.lexeme,op.header.position);
     if(!binding||binding->identity!=operator_identity(op.header)||
        binding->header.lexeme2!=op.header.lexeme2||binding->header.arity!=op.header.arity||
        binding->header.precedence!=op.header.precedence||binding->header.tightness!=op.header.tightness||
        binding->header.binds!=op.header.binds)
      throw Error(op.header.where,"inconsistent operator header in discovery and parse","ON1X-OPERATOR-DISCOVERY");
     op.function.name=op.identity=binding->identity;
    }
       if(options.standalone_guest){
       const auto& u=m->unit;
        if(!u.imports.empty()||!u.module_imports.empty()||!u.kernel_imports.empty()||!u.foreign_imports.empty()||
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
        if(!options.kernel_c11&&!m->unit.kernel_imports.empty()){
         if(m->unit.kernel_imports.front().module==ipc_kernel_module)
         throw Error(m->unit.kernel_imports.front().where,
                     "ON1X-KERNEL-UNSAFE-UNSUPPORTED: mulib.ipc_framed requires the static KernelBind adapter (--kernel-bind)",
                     "ON1X-KERNEL-UNSAFE-UNSUPPORTED");
        require_no_kernel_imports(m->unit);
       }
       }
       if(options.kernel_load_time&&
          (!m->unit.ppx.empty()||!m->unit.extensions.empty()||!m->unit.features.empty()))
        throw Error({path.string()},"--kernel-load-time does not execute source PPX or declaration extensions",
                    "ON1X-KERNEL-PROFILE");
      if(options.kernel_c11){
       if(!m->unit.ppx.empty()||!m->unit.macros.empty()||!m->unit.extensions.empty()||
          !m->unit.features.empty()||!m->unit.cliches.empty()||
          !m->unit.cliche_implementations.empty()||!m->unit.inherent_implementations.empty())
        throw Error(at,"ON1X-KERNEL-PROFILE: this C11 profile does not execute source macros, PPX or extensions","ON1X-KERNEL-PROFILE");
       for(const auto& request:m->unit.kernel_imports){
        if(request.module==ipc_kernel_module){
         if(request.dynlib)
          throw Error(request.where,"ON1X-KERNEL-ABI: framed transport requires the static KernelBind adapter","ON1X-KERNEL-ABI");
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__LP64__)
         throw Error(request.where,"ON1X-KERNEL-ABI: framed transport requires native Linux x86-64 LP64","ON1X-KERNEL-ABI");
#endif
         continue;
        }
        const bool supported=request.is_unsafe?request.module==legacy_scalar_module:
         options.kernel_stage&&(request.module==legacy_scalar_module||request.module=="mulib.histprice"||request.module=="mulib.priceopt");
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
   for(std::size_t i=0;i<m->unit.module_imports.size();++i){
    const auto& imp=m->unit.module_imports[i];
    if(++edges>16384)throw Error(imp.where,"import edge limit exceeded");
    // `from . import child` addresses a directory, not its currently-loading initializer.
    auto* target=m->operator_targets[i];
    bool own_package=false;
    if(target)target=visit(target,imp.where);
    else{
     auto target_path=resolve(imp,*m);
     own_package=self_package(imp,*m,target_path);
     target=own_package?m:visit(target_path,imp.where,!imp.quoted);
    }
   if(target!=m)m->dependencies.push_back(target);
   if(imp.quoted){m->quoted.push_back(target);merge(m->names,target->names,imp.where);continue;}
    if(!imp.selective){bind_module(m->names,imp.alias.empty()?imp.module:imp.alias,target,imp.where);continue;}
    for(const auto& selection:imp.names){
     if(selection.is_operator)continue; // Already included in the operator export scope.
     const auto& name=selection.name;const auto& alias=selection.alias.empty()?name:selection.alias;
     bool found=false;
     if(!own_package){
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
   m->discovery={};
   std::vector<Module*>().swap(m->operator_targets);
   if(regular(path)){
    std::set<std::string> aliases;
    for(const auto& request:m->unit.kernel_imports){
      if(++edges>16384||++kernel_requests>1024)
      throw Error(request.where,"kernel import inspection limit exceeded");
      if(!aliases.insert(request.alias).second||import_alias_conflicts(m->names,request.alias)){
       throw Error(request.where,"conflicting kernel import alias: "+request.alias,"ON1X-KERNEL-ALIAS");
      }
      if(inspection)inspection->requests.push_back(request);
      else m->kernels.emplace(request.alias,request);
    }
    for(const auto& request:m->unit.foreign_imports){
     if(++edges>16384||++kernel_requests>1024)
      throw Error(request.where,"foreign and kernel import count limit exceeded");
     if(!aliases.insert(request.alias).second||import_alias_conflicts(m->names,request.alias))
      throw Error(request.where,"conflicting stdffi import alias: "+request.alias,"ON1X-STDFFI-ALIAS");
     // --kernel-imports describes only requests that may need native build
     // inputs. Portable definitions neither require linking nor participate in
     // that protocol; admission happens during ordinary source loading.
     if(!inspection){
      admit_foreign(request);
      m->foreigns.emplace(request.alias,request);
     }
    }
    if(inspection)inspection->source_digests.merge(m->unit.source_digests);
  }
   stack.pop_back();m->active=false;m->loaded=true;order.push_back(m);return m;
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
   if(current_module&&current_module->foreigns.contains(first)){
    if(dot==std::string::npos&&!type&&!syntax)
     throw Error(at,"stdffi module is not a first-class value: "+first,"ON1X-STDFFI-MEMBER");
    throw Error(at,"no portable stdffi "+std::string(type?"type ":syntax?"syntax ":"export ")+text,
                "ON1X-STDFFI-MEMBER");
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
      auto found=current_module?current_module->kernels.find(text.substr(0,dot)):std::map<std::string,KernelImport>::const_iterator{};
      if(dot!=std::string::npos&&current_module&&found!=current_module->kernels.end()){
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
     if(e.kind==Expr::Kind::Variable&&!foreign_reference(e,values)){
      (void)kernel_reference(e,values,false);reference(e.text,e.where,names,values,false);
     }
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
      if(s.syntax_operator){
       // A generated operator is a named, noncapturing callable. Resolve
       // definition-site modules/macros and its own parameters, not the
       // template's surrounding value aliases or any unsafe region.
       auto saved=unsafe_context;unsafe_context=false;
       function_body(s.syntax_operator->function,names,syntax);
       unsafe_context=saved;
       continue;
      }
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
    void function_body(Function& f,const Names& names,const Scope& syntax={}){
    f.result=type_reference(f.result,f.where,names);
    for(auto& p:f.params)p.type=type_reference(p.type,p.where,names);
    for(auto& parameter:f.type_parameters)constraints(parameter.bounds,names);
    constraints(f.constraints,names);
     Scope locals;parameters(locals,f.params);body(f.body,names,locals,syntax);
     for(auto& attr:f.attributes)expression(*attr,names,locals,syntax);
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
   struct PreludeModules {
    Module* pervasive=nullptr;
    Module* operators=nullptr;
    std::vector<Module*> configured;
   };
   PreludeModules discover_preludes(const fs::path& input){
    entry_directory=canonical(input,{input.string()}).parent_path();
    const Location at{"<prelude>"};
    PreludeModules result;
    if(options.prelude){
     auto pervasive=standard("pervasive",at);
     if(pervasive.empty())
      throw Error({input.string()},"automatic prelude requires --stdlib or --no-prelude (pervasive.on1x not found)");
     auto operators=standard("operators",at);
     // Older/custom standard libraries may ship only pervasive.on1x.
     if(!operators.empty()){
      result.operators=metadata(operators,at,false);
      operator_preludes.push_back(result.operators);
     }
     result.pervasive=metadata(pervasive,at,false);
     operator_preludes.push_back(result.pervasive);
    }
    for(const auto& path:options.preludes){
     auto* module=metadata(path,at,false);
     result.configured.push_back(module);
     operator_preludes.push_back(module);
    }
    return result;
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
   Unit parse_source(const fs::path& input){
    (void)discover_preludes(input);
    const Location at{input.string()};
    auto* entry=metadata(input,at,false);
    return parse_with_operators(entry->scanned,
            {options.extensions.enabled.begin(),options.extensions.enabled.end()},
            std::move(entry->docs),visible_operators(*entry,at));
   }
   LoadedUnit run(const fs::path& input){
    if(options.kernel_load_time&&
       (inspection||options.standalone_guest||options.kernel_c11||options.kernel_stage||
        !options.extensions.enabled.empty()))
     throw Error({input.string()},"load-time stdffi cannot be combined with KernelBind, guest or declaration extension profiles",
                 "ON1X-KERNEL-PROFILE");
    if(options.interfaces.size()>max_interface_files)
    throw Error({input.string()},"interface file count limit exceeded","ON1X-INTERFACE-LIMIT");
   if(!options.interfaces.empty()&&(inspection||options.kernel_c11||!options.extensions.enabled.empty()))
    throw Error({input.string()},"interface verification cannot run with KernelBind or declaration extensions",
                "ON1X-INTERFACE-UNSUPPORTED");
    const Location prelude_at{"<prelude>"};
    const auto discovered=discover_preludes(input);
    implicit=true;
    if(discovered.pervasive)preludes.push_back(visit(discovered.pervasive,prelude_at));
    if(discovered.operators)visit(discovered.operators,prelude_at); // Open only its operator exports.
    implicit=false;
    for(auto* module:discovered.configured)preludes.push_back(visit(module,prelude_at));
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
    // Macro expansion or PPX can introduce the first operator in a graph.
    // Freeze module ownership even when discovery found no operators.
    result.unit.operator_entry=entry->path.string();
   result.interfaces=bind_interfaces();
  // Reserve storage so pointers used for identical raw-extern admission stay valid.
   std::size_t count=0;
   for(auto* m:order){
     count+=m->unit.functions.size()+m->unit.operators.size();
    for(const auto& implementation:m->unit.cliche_implementations)
     count+=implementation.members.size();
    for(const auto& implementation:m->unit.inherent_implementations)
     count+=implementation.members.size();
   }
   if(count>16384)throw Error({input.string()},"loaded function count limit exceeded");
   result.unit.functions.reserve(count);
    for(auto* m:order){
     current_module=m;
     const auto scope_key=m->path.string();
      record_operator_scope(result.unit,m->operator_visibility,scope_key);
    for(auto& doc:m->unit.docstrings)result.unit.docstrings.push_back(std::move(doc));
    auto names=m->legacy?flat:m->names;
      for(const auto& [alias,request]:m->kernels)if(import_alias_conflicts(names,alias))
       throw Error(request.where,"conflicting kernel import alias: "+alias,"ON1X-KERNEL-ALIAS");
      for(const auto& [alias,request]:m->foreigns)if(import_alias_conflicts(names,alias))
       throw Error(request.where,"conflicting stdffi import alias: "+alias,"ON1X-STDFFI-ALIAS");
    for(auto it=preludes.rbegin();it!=preludes.rend();++it){
     auto* prelude=*it;
     const auto bound=[&](const std::string& key){return module_bound(names,key)||m->foreigns.contains(key);};
     for(const auto& [key,symbol]:prelude->names.values)if(!names.values.contains(key)&&!bound(key))insert(names.values,key,symbol,{m->path.string()});
      for(const auto& [key,symbol]:prelude->names.syntax)if(!names.syntax.contains(key)&&!bound(key))insert(names.syntax,key,symbol,{m->path.string()});
       for(const auto& [key,symbol]:prelude->names.types)if(!names.types.contains(key)&&!bound(key))insert(names.types,key,symbol,{m->path.string()});
       for(const auto& [key,symbol]:prelude->names.cliches)if(!names.cliches.contains(key)&&!bound(key))insert(names.cliches,key,symbol,{m->path.string()});
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
    for(auto& op:m->unit.operators){
     function_body(op.function,names);
     result.unit.operator_owners.emplace(op.identity,scope_key);
     result.internal_functions.insert(op.identity);
     if(m->implicit)result.unit.operator_lazy_functions.insert(op.identity);
     result.unit.operators.push_back(std::move(op));
    }
    for(auto& directive:m->unit.fixity_directives)result.unit.fixity_directives.push_back(std::move(directive));
    for(auto& f:m->unit.functions){
     function_body(f,names);
     f.name=name({m,f.name,f.external});
      result.unit.operator_owners.emplace(f.name,scope_key);
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
     if(kernel_calls.size()+foreign_functions.size()>16384-count)
      throw Error({input.string()},"loaded function count limit exceeded");
     for(auto& [name,f]:kernel_calls){(void)name;result.unit.functions.push_back(std::move(f));}
      // Portable imports supply ordinary definitions by default. In late mode
      // the same references resolve to body-free internal typed declarations.
     for(auto& [key,f]:foreign_functions){
      (void)key;
      result.internal_functions.insert(f.name);
      result.unit.functions.push_back(std::move(f));
     }
  // Automatic pervasive bindings are demand-loaded. This preserves the existing
  // annotated-only checking path and keeps unused generic helpers out of PPX/MIR.
  // Explicit imports and --prelude files always retain/check their definitions.
  std::map<std::string,std::set<std::string>> calls;
   auto collect=[&](auto&& self,const Expr& e,std::set<std::string>& refs)->void{
    if(e.kind==Expr::Kind::Variable)refs.insert(e.text);
    if(!e.operator_identity.empty())refs.insert(e.operator_identity);
   if(e.callee)self(self,*e.callee,refs);
   if(e.syntax_tail)self(self,*e.syntax_tail,refs);
   for(const auto& child:e.children)self(self,*child,refs);
   auto statements=[&](auto&& walk,const std::vector<Stmt>& body)->void{
     for(const auto& s:body){
      if(s.value)self(self,*s.value,refs);
      if(s.syntax_operator){
       for(const auto& attr:s.syntax_operator->function.attributes)self(self,*attr,refs);
       walk(walk,s.syntax_operator->function.body);
      }
      walk(walk,s.body);walk(walk,s.otherwise);
     }
   };statements(statements,e.body);
   for(const auto& m:e.syntax_definitions)for(const auto& r:m.rules){if(r.expression)self(self,*r.expression,refs);statements(statements,r.body);}
  };
  auto statements=[&](auto&& self,const std::vector<Stmt>& body,std::set<std::string>& refs)->void{
    for(const auto& s:body){
     if(s.value)collect(collect,*s.value,refs);
     if(s.syntax_operator){
      for(const auto& attr:s.syntax_operator->function.attributes)collect(collect,*attr,refs);
      self(self,s.syntax_operator->function.body,refs);
     }
     self(self,s.body,refs);self(self,s.otherwise,refs);
    }
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
    for(const auto& op:result.unit.operators){
     auto& refs=calls[op.identity];
     statements(statements,op.function.body,refs);
     for(const auto& attr:op.function.attributes)collect(collect,*attr,refs);
     if(!result.unit.operator_lazy_functions.contains(op.identity))needed.insert(op.identity);
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
   const auto reach=[&](std::vector<std::string> pending){
    while(!pending.empty()){
     auto current=std::move(pending.back());pending.pop_back();
     auto found=calls.find(current);
     if(found==calls.end())continue;
     for(const auto& target:found->second)if(needed.insert(target).second)pending.push_back(target);
    }
   };
   reach({needed.begin(),needed.end()});
   if(!result.unit.operator_lazy_functions.empty()){
    // Keep the complete automatically opened operator surface through PPX.
    // Those operators may be newly used by a transform, so also retain their
    // named helpers, marked as optional if no original root requires them.
    const auto original=needed;
    std::vector<std::string> pending;
    for(const auto& op:result.unit.operators)
     if(needed.insert(op.identity).second)pending.push_back(op.identity);
    reach(std::move(pending));
    for(const auto& candidate:lazy)
     if(needed.contains(candidate)&&!original.contains(candidate))
      result.unit.operator_lazy_functions.insert(candidate);
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
   if(!ec){
    const auto share=(resolved.parent_path()/"../share/on1x").lexically_normal();
    auto ffi=share/"stdffi/mulib";
    if(directory(ffi)||regular(share/"stdlib/On1xfile"))
      options.foreign_default_paths={ffi};
   }
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
 Unit parse_source_with_operator_imports(const fs::path& input,const std::string& source,
                                         const LoaderOptions& options){
  if(source.size()>max_source_bytes)
   throw Error({input.string()},"source exceeds 2 MiB limit","ON1X-LEX-LIMIT");
  auto snapshot=options;
  auto path=canonical(input,{input.string()});
  snapshot.source_overrides.insert_or_assign(path,source);
  snapshot.extensions.execute=false;
  return Loader(snapshot).parse_source(path);
 }
KernelImportRequests inspect_kernel_imports(const fs::path& input,const LoaderOptions& options){
 auto inspect_options=options;
 inspect_options.extensions.execute=false;
 KernelImportRequests result;
 (void)Loader(inspect_options,&result).run(input);
 return result;
}
}

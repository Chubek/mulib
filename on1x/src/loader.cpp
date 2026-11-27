#include <on1x/loader.hpp>
#include <on1x/asdl_module.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/discovery.hpp>
#include <on1x/module_visibility.hpp>
#include <on1x/module_signature.hpp>
#include <on1x/operator_scope.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <sstream>

namespace on1x {
namespace {
namespace fs=std::filesystem;
struct Module;
struct Symbol {
 Module* owner;
 std::string name;
 bool external=false;
 VisibilityRule visibility{};
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
 OperatorScope operator_exports,operator_imports,operator_visibility;
 // Direct dependencies that affect parsing, in discovery import order.
 std::vector<Module*> operator_targets;
 Unit unit;
 Names names;
 Names declarations; // Own declarations, excluding lexically imported names.
 std::optional<Names> signature_exports;
 std::vector<Module*> fragments;
 Module* group=nullptr;
 const LoaderOptions::ModuleMember* member=nullptr;
 fs::path package_identity;
 std::vector<Module*> quoted;
  std::vector<Module*> dependencies;
   std::map<std::string,KernelImport> kernels;
   std::map<std::string,ForeignImport> foreigns;
 std::size_t id=0;
   bool discovering=true,active=false,loaded=false,legacy=false,implicit=false;
 bool program=false,has_main=false,isolated=false,source_capability=false;
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
  using Member=LoaderOptions::ModuleMember;
  std::map<std::pair<fs::path,std::string>,std::unique_ptr<Module>> groups;
  std::map<fs::path,const Member*> enrolled;
  std::set<fs::path> known_packages,indexed_programs;
  std::map<std::string,fs::path> conventional_names;
  std::set<fs::path> verified_signatures;
  std::map<fs::path,fs::path> explicit_interfaces;
  std::size_t declaration_interface_members=0;
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
   static bool program_import(const Import& imp){return imp.kind==Import::Kind::Program;}
   static bool named_module(const Module& m){return m.discovery.module.has_value()||m.unit.module.has_value();}
   static const Names& exported(const Module& m){
    if(m.signature_exports)return *m.signature_exports;
    return named_module(m)||m.program||!m.fragments.empty()?m.declarations:m.names;
   }
   static bool valid_module_name(std::string_view text){
    const auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
    bool first=true;
    for(auto c:text){
     if(first){if(!alpha(c))return false;first=false;}
     else if(c=='.')first=true;
     else if(!alpha(c)&&!(c>='0'&&c<='9'))return false;
    }
    return !first;
   }
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
   void merge(Names& into,const Names& from,const Location& at,const Module* requester=nullptr){
   auto transfer=[&](auto& target,const auto& source){
    for(const auto& [name,symbol]:source){
     if(requester&&!accessible(*symbol.owner,*requester,symbol.visibility))continue;
     value_alias(into,name,at);insert(target,name,symbol,at);
    }
   };
   transfer(into.values,from.values);transfer(into.syntax,from.syntax);
   transfer(into.types,from.types);transfer(into.cliches,from.cliches);
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
   fs::path standard(const std::string& module,const Location& at,bool modern=false){
     for(const auto& root:standard_roots){
       const bool operators=module=="operators";
       auto first=candidate(root/module_path(module),at,operators);
     // Compatibility with older installed extension trees; the canonical MuLIB
     // package now lives inside stdlib, not in this legacy sibling directory.
       auto second=candidate(root.parent_path()/"std"/module_path(module),at,operators);
       if(modern){
        auto declared=direct_module(root,module_path(module),at);
        if(!declared.empty()){
         if(regular(first)||regular(second))throw Error(at,"ambiguous standard module: "+module);
         return declared;
        }
       }
      if(!first.empty()&&!second.empty()){
       if(first==second||canonical(first,at)==canonical(second,at))return first;
       throw Error(at,"ambiguous standard module: "+module);
      }
     if(!first.empty()||!second.empty())return first.empty()?second:first;
    }
     return {};
   }
  static bool contained(const fs::path& root,const fs::path& path){
   auto it=path.begin();
   for(auto part=root.begin();part!=root.end();++part,++it)
    if(it==path.end()||*it!=*part)return false;
   return it!=path.end();
  }
  const Member* member_for(const std::string& name,const Module& parent)const{
   if(auto scope=options.scoped_module_members.find(parent.package_identity);
      scope!=options.scoped_module_members.end()){
    auto found=scope->second.find(name);
    return found==scope->second.end()?nullptr:&found->second;
   }
   if(parent.package_identity==options.project_package){
    auto found=options.module_members.find(name);
    return found==options.module_members.end()?nullptr:&found->second;
   }
   return nullptr;
  }
  fs::path direct_module(const fs::path& root,const fs::path& suffix,const Location& at){
   const auto locate=[&](const fs::path& path)->fs::path{
    std::error_code ec;
    const auto status=fs::symlink_status(path,ec);
    if(ec==std::errc::no_such_file_or_directory||ec==std::errc::not_a_directory)return {};
    if(ec)throw Error(at,"cannot inspect module '"+path.string()+"': "+ec.message(),"ON1X-MODULE-RESOLUTION");
    if(!fs::exists(status))return {};
    auto origin=canonical(path,at);
    if(!regular(origin)||origin.extension()!=".on1xm"||!contained(canonical(root,at),origin))
     throw Error(at,"module source is not a regular .on1xm file in its search root: "+path.string(),
                 "ON1X-MODULE-RESOLUTION");
    return origin;
   };
   auto file=root/suffix;file+=".on1xm";
   auto source=locate(file),initializer=locate(root/suffix/"__init__.on1xm");
   if(!source.empty()&&!initializer.empty())
    throw Error(at,"ambiguous module: "+file.string(),"ON1X-MODULE-RESOLUTION");
   return source.empty()?initializer:source;
  }
  fs::path resolve_program(const Import& imp,const Module& parent){
   if(imp.relative||(imp.selective&&!imp.alias.empty()))
    throw Error(imp.where,"program imports cannot combine relative or namespace aliases with selectors","ON1X-PROGRAM-IMPORT");
   if(!options.project_package.empty()){
    if(parent.package_identity!=options.project_package)
     throw Error(imp.where,"project program imports require an admitted program name","ON1X-PROGRAM-IMPORT");
    if(imp.quoted){
     auto origin=canonical(parent.path.parent_path()/imp.module,imp.where);
     if(!indexed_programs.contains(origin))
      throw Error(imp.where,"program path is not in the project index: "+imp.module,"ON1X-PROGRAM-IMPORT");
     return origin;
    }
    auto found=options.program_members.find(imp.module);
    if(found==options.program_members.end())
     throw Error(imp.where,"program is not in the project index: "+imp.module,"ON1X-PROGRAM-IMPORT");
    return found->second;
   }
   if(imp.quoted)return parent.path.parent_path()/imp.module;
   if(!valid_module_name(imp.module))
    throw Error(imp.where,"invalid program name: "+imp.module,"ON1X-PROGRAM-IMPORT");
   auto suffix=module_path(imp.module);suffix+=".on1x";
   std::vector<fs::path> roots{entry_directory};
   roots.insert(roots.end(),options.paths.begin(),options.paths.end());
   for(const auto& root:roots){
    auto candidate=root/suffix;
    if(regular(candidate)){
     auto origin=canonical(candidate,imp.where);
     if(!contained(canonical(root,imp.where),origin))
      throw Error(imp.where,"program source escapes its search root","ON1X-PROGRAM-IMPORT");
     return origin;
    }
   }
   throw Error(imp.where,"program not found: "+imp.module,"ON1X-PROGRAM-IMPORT");
  }
  fs::path resolve(const Import& imp,const Module& parent){
   if(program_import(imp))return resolve_program(imp,parent);
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
      auto found=imp.module=="std"?standard_roots.front():standard(imp.module.substr(4),imp.where,true);
   if(!found.empty()&&fs::exists(found))return found;
   throw Error(imp.where,"standard module not found: "+imp.module);
  }
  std::vector<fs::path> roots{entry_directory};roots.insert(roots.end(),options.paths.begin(),options.paths.end());
   roots.insert(roots.end(),standard_roots.begin(),standard_roots.end());
  std::string searched;
    for(const auto& root:roots){
     auto found=discovery::library(root,imp.module,imp.where);
     if(found.empty())found=candidate(root/relative,imp.where);
     if(options.project_package.empty()){
      if(auto path=direct_module(root,relative,imp.where);!path.empty()){
       if(regular(found))throw Error(imp.where,"ambiguous module: "+imp.module);
       return path;
      }
     }
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
    return (imp.quoted&&!program_import(imp))||(imp.selective&&std::any_of(imp.names.begin(),imp.names.end(),
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
           a.alias==b.alias&&a.relative==b.relative&&a.quoted==b.quoted&&a.selective==b.selective&&a.kind==b.kind&&
          std::equal(a.names.begin(),a.names.end(),b.names.begin(),b.names.end(),
                     [](const ImportName& x,const ImportName& y){
                      return x.name==y.name&&x.alias==y.alias&&x.is_operator==y.is_operator;
                     });
  }
    ModuleProvenance provenance(const Module& m)const{
    ModuleProvenance result{m.path,m.package_identity,{}};
    const auto& header=m.unit.module?m.unit.module:m.discovery.module;
    if(header){
     std::istringstream name(header->name);
     std::string part;
     while(std::getline(name,part,'.'))result.module_components.push_back(std::move(part));
    }
    return result;
   }
   bool accessible(const Module& owner,const Module& requester,VisibilityRule rule)const{
    bool admitted=true;
    if(auto membership=enrolled.find(owner.path);membership!=enrolled.end()){
     admitted=false;
     const auto* scope=&options.module_members;
     if(auto found=options.scoped_module_members.find(requester.package_identity);
        found!=options.scoped_module_members.end())scope=&found->second;
     else if(requester.package_identity!=options.project_package)scope=nullptr;
     if(scope)admitted=std::any_of(scope->begin(),scope->end(),[&](const auto& entry){
      return entry.second.package==membership->second->package&&entry.second.declared_name==membership->second->declared_name;
     });
    }
    return module_visibility_access(rule,provenance(owner),provenance(requester),admitted)==ModuleAccess::Allowed;
   }
   void require_access(const Symbol& symbol,const Module& requester,const Location& at)const{
    if(!accessible(*symbol.owner,requester,symbol.visibility))
     throw Error(at,"module member is not visible: "+symbol.name,"ON1X-MODULE-VISIBILITY");
   }
   void validate_visibility(const Module& owner,VisibilityRule rule,const Location& at)const{
    if(validate_module_visibility(rule,provenance(owner))!=ModuleVisibilityIssue::None)
     throw Error(at,"invalid declaration visibility for this module scope","ON1X-MODULE-VISIBILITY");
   }
   bool operator_accessible(const OperatorBinding& binding,const Module& requester)const{
    auto owner=modules.find(fs::path(binding.header.where.file));
    return owner!=modules.end()&&accessible(*owner->second,requester,binding.header.visibility);
   }
   bool has_signature(const Module& module)const{
    if(!named_module(module))return false;
    auto interface=module.path;interface.replace_extension(".on1xi");
    return (module.member&&!module.member->interfaces.empty())||explicit_interfaces.contains(interface);
   }
   void verify_signatures(Module& target,const std::vector<fs::path>& input,const Location& at){
    if(input.empty())return;
    const auto& header=target.unit.module?target.unit.module:target.discovery.module;
    ModuleSignature signature;
    signature.where={input.front().string()};
    signature.module=header->name;
    for(const auto& origin:input){
     if(verified_signatures.size()>=max_interface_files)
      throw Error(at,"interface file count limit exceeded","ON1X-INTERFACE-LIMIT");
     if(!verified_signatures.insert(origin).second)
      throw Error({origin.string()},"signature belongs to more than one module","ON1X-INTERFACE-DUPLICATE");
     auto source=read_source_file(origin.string());
     if(source.size()>options.max_bytes-bytes)
      throw Error({origin.string()},"total source and interface byte limit exceeded","ON1X-INTERFACE-LIMIT");
     bytes+=source.size();
     auto fragment=parse_module_signature(scan(source,origin.string()));
     if(fragment.module!=signature.module)
      throw Error(fragment.where,"signature does not declare module '"+header->name+"'","ON1X-INTERFACE-IMPLEMENTATION");
     std::move(fragment.members.begin(),fragment.members.end(),std::back_inserter(signature.members));
     std::move(fragment.types.begin(),fragment.types.end(),std::back_inserter(signature.types));
    }
    std::vector<const Unit*> originals;
    for(auto* fragment:target.fragments)originals.push_back(&fragment->unit);
    if(originals.empty())originals.push_back(&target.unit);
    (void)verify_module_signature(signature,originals,header->name,target.path);
    if(!options.declaration_interfaces)require_executable_module_signature(signature);
    else declaration_interface_members+=signature.members.size()+signature.types.size();
    std::set<std::string> listed;
    for(const auto& declaration:signature.members)listed.insert(declaration.name);
    auto& visible=target.signature_exports.emplace();
    for(const auto& name:listed){
     auto item=target.declarations.values.find(name);
     if(item!=target.declarations.values.end())visible.values.insert(*item);
     }
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
    if(stack.size()+discovery_stack.size()>=options.max_depth){
     throw Error(at,"import recursion limit exceeded");
    }
    auto storage=std::make_unique<Module>();auto* m=storage.get();m->path=path;
    if(auto member=enrolled.find(path);member!=enrolled.end()){
     m->member=member->second;m->package_identity=m->member->package;
    }
     else for(const auto& root:known_packages)
      if(contained(root,path)&&root.native().size()>m->package_identity.native().size())
       m->package_identity=root;
   modules.emplace(path,std::move(storage));discovery_stack.push_back(m);
   if(source){
    auto replacement=options.source_overrides.find(path);
    m->source=replacement==options.source_overrides.end()?read_source_file(path.string()):replacement->second;
    if(m->source.size()>options.max_bytes-bytes)throw Error(at,"total imported source byte limit exceeded");
    bytes+=m->source.size();m->scanned=scan(m->source,path.string(),&m->docs);
    for(const auto& t:m->scanned)tokens.insert(t.text);
     m->discovery=discover_operators(m->scanned);
     if(path.extension()==".on1xm"&&!m->discovery.module)
      throw Error(at,"module source requires a module declaration: "+path.string(),"ON1X-MODULE-IDENTITY");
     if(m->discovery.module){
      if(auto member=enrolled.find(path);member!=enrolled.end()){
       if(member->second->declared_name!=m->discovery.module->name)
        throw Error(m->discovery.module->where,"module declaration does not match project membership: "+
                    member->second->declared_name,"ON1X-MODULE-IDENTITY");
      }else{
       auto [found,added]=conventional_names.emplace(m->discovery.module->name,path);
       if(!added&&found->second!=path)
        throw Error(m->discovery.module->where,"repeated module identity requires project membership: "+
                    m->discovery.module->name,"ON1X-MODULE-IDENTITY");
      }
     }
    if(m->discovery.imports.size()>16384-discovery_edges)throw Error(at,"import edge limit exceeded");
    discovery_edges+=m->discovery.imports.size();
    if(options.standalone_guest&&!m->discovery.imports.empty()){
     throw Error(m->discovery.imports.front().where,"standalone ArchVirt source cannot import or activate extensions");
    }
     for(const auto& header:m->discovery.operators){
      validate_visibility(*m,header.visibility,header.where);
      add_operator(m->operator_exports,{header,operator_identity(header)},header.where);
     }
    m->operator_targets.resize(m->discovery.imports.size());
    for(std::size_t i=0;i<m->discovery.imports.size();++i){
     const auto& imp=m->discovery.imports[i];
     if(!wants_operators(imp))continue;
      auto* target=import_metadata(imp,*m);
     m->operator_targets[i]=target;
      if(imp.quoted&&!program_import(imp)){
       target->operator_exports.each([&](const auto& binding){
        if(operator_accessible(binding,*m))
         add_operator(named_module(*m)?m->operator_imports:m->operator_exports,binding,imp.where);
       });
      continue;
     }
      for(const auto& selection:imp.names){
       if(!selection.is_operator)continue;
       if(program_import(imp))
        throw Error(imp.where,"program imports cannot select operators","ON1X-PROGRAM-IMPORT");
       const auto* prefix=target&&!has_signature(*target)?target->operator_exports.find(selection.name,OperatorPosition::Prefix):nullptr;
       const auto* trailing=target&&!has_signature(*target)?target->operator_exports.trailing(selection.name):nullptr;
       if(prefix&&!operator_accessible(*prefix,*m))prefix=nullptr;
       if(trailing&&!operator_accessible(*trailing,*m))trailing=nullptr;
       if(!prefix&&!trailing){
        throw Error(imp.where,"module '"+imp.module+"' has no operator export '"+selection.name+"'",
                    "ON1X-OPERATOR-IMPORT");
       }
       auto& scope=named_module(*m)?m->operator_imports:m->operator_exports;
       if(prefix)add_operator(scope,*prefix,imp.where);
       if(trailing)add_operator(scope,*trailing,imp.where);
     }
    }
   }
    discovery_stack.pop_back();m->discovering=false;return m;
   }
   Module* group_metadata(const Member& member,const Location& at){
    auto key=std::make_pair(member.package,member.declared_name);
    if(auto found=groups.find(key);found!=groups.end()){
     if(found->second->discovering)throw Error(at,"import cycle: "+member.declared_name);
     return found->second.get();
    }
    auto group=std::make_unique<Module>();
    auto* m=group.get();
    m->path=member.fragments.front();
    m->member=&member;
    m->package_identity=member.package;
    m->discovery.module=ModuleHeader{at,member.declared_name};
    groups.emplace(std::move(key),std::move(group));
    for(const auto& path:member.fragments){
     auto* fragment=metadata(path,at,false);
     const auto& header=fragment->unit.module?fragment->unit.module:fragment->discovery.module;
     if(!header||header->name!=member.declared_name)
      throw Error(at,"module fragment must declare '"+member.declared_name+"': "+path.string(),"ON1X-MODULE-IDENTITY");
     fragment->group=m;
     m->fragments.push_back(fragment);
     if(!has_signature(*fragment)&&member.interfaces.empty())
      fragment->operator_exports.each([&](const auto& binding){add_operator(m->operator_exports,binding,at);});
    }
    m->discovering=false;
    return m;
   }
   Module* import_metadata(const Import& imp,Module& parent){
    if(!imp.relative&&!imp.quoted&&!program_import(imp)&&imp.module!="std"&&!imp.module.starts_with("std."))
     if(auto member=member_for(imp.module,parent))return group_metadata(*member,imp.where);
    auto path=resolve(imp,parent);
    if((imp.relative||imp.quoted)&&!parent.package_identity.empty()){
     auto origin=canonical(path,imp.where);
     if(origin!=parent.package_identity&&!contained(parent.package_identity,origin))
      throw Error(imp.where,"relative or quoted import escapes the verified package","ON1X-MODULE-MEMBERSHIP");
    }
    if(self_package(imp,parent,path))return nullptr;
    auto* target=metadata(path,imp.where,!imp.quoted&&!program_import(imp));
    if(program_import(imp)){
     if(named_module(*target)||target->path.extension()!=".on1x")
      throw Error(imp.where,"program import requires a headerless .on1x source","ON1X-PROGRAM-IMPORT");
    }else if(indexed_programs.contains(target->path)){
     throw Error(imp.where,"an indexed program requires an explicit program import: "+imp.module,"ON1X-PROGRAM-IMPORT");
    }else if(named_module(*target)){
     if(imp.quoted||imp.relative||enrolled.contains(target->path)||
        (!options.project_package.empty()&&!imp.module.starts_with("std."))||
        (target->unit.module?target->unit.module:target->discovery.module)->name!=imp.module)
      throw Error(imp.where,"import does not match the canonical module identity: "+imp.module,"ON1X-MODULE-IDENTITY");
    }
    return target;
   }
    OperatorScope visible_operators(const Module& module,const Location& at){
     auto scope=module.operator_exports;
     module.operator_imports.each([&](const auto& binding){add_operator(scope,binding,at);});
     if(module.group)for(auto* sibling:module.group->fragments){
      if(sibling==&module)continue;
      sibling->operator_exports.each([&](const auto& binding){
       if(operator_accessible(binding,module))add_operator(scope,binding,at);
      });
     }
     for(auto it=operator_preludes.rbegin();it!=operator_preludes.rend();++it){
      if(has_signature(**it))continue;
      (*it)->operator_exports.each([&](const auto& binding){
       if(!operator_accessible(binding,module))return;
      const auto& header=binding.header;
      const auto* existing=header.position==OperatorPosition::Prefix?
       scope.find(header.lexeme,OperatorPosition::Prefix):scope.trailing(header.lexeme);
      if(!existing)add_operator(scope,binding,at);
     });
    }
     return scope;
    }
   void isolate(Module& module,const Location& at){
    if(module.has_main&&!module.program)
     throw Error(at,"program dependency with main needs an explicit program import: "+
                 module.path.string(),"ON1X-PROGRAM-IMPORT");
    if(module.source_capability)
     throw Error(at,"program import dependency requests source activation or native capabilities: "+
                 module.path.string(),"ON1X-PROGRAM-CAPABILITY");
    if(module.isolated)return;
    module.isolated=true;
    for(auto* dep:module.dependencies)isolate(*dep,at);
   }
   void depend(Module& requester,Module& target){
    requester.dependencies.push_back(&target);
   }
   Module* visit(Module* m,const Location& at,bool isolated=false,bool program=false){
    const auto& path=m->path;
   if(m->active){
    std::string chain;for(const auto* parent:stack)chain+=parent->path.string()+" -> ";
    throw Error(at,"import cycle: "+chain+path.string());
    }
    if(m->loaded){
     if(program!=m->program)
      throw Error(at,"program source cannot also be imported as a library: "+path.string(),"ON1X-PROGRAM-IMPORT");
     if(isolated)isolate(*m,at);
     if(!implicit)explicit_module(m);
     return m;
    }
   if(stack.size()>=options.max_depth)throw Error(at,"import recursion limit exceeded");
    m->id=next_module_id++;m->implicit=implicit;m->active=true;stack.push_back(m);
    m->program=program;
    if(!m->fragments.empty()){
     for(auto* fragment:m->fragments){
      visit(fragment,at,isolated);
      depend(*m,*fragment);
      const auto reject_duplicates=[&](const auto& prior,const auto& added){
       for(const auto& [name,symbol]:added)
        if(prior.contains(name))
         throw Error({symbol.owner->path.string()},"duplicate fragment declaration: "+name,"ON1X-MODULE-IDENTITY");
      };
      reject_duplicates(m->declarations.values,fragment->declarations.values);
      reject_duplicates(m->declarations.types,fragment->declarations.types);
      reject_duplicates(m->declarations.syntax,fragment->declarations.syntax);
      reject_duplicates(m->declarations.cliches,fragment->declarations.cliches);
      merge(m->declarations,fragment->declarations,at);
     }
     verify_signatures(*m,m->member->interfaces,at);
     stack.pop_back();m->active=false;m->loaded=true;m->isolated=isolated;
     return m;
    }
    if(!m->scanned.empty()){
     m->operator_visibility=visible_operators(*m,at);
     m->unit=parse_with_operators(m->scanned,{options.extensions.enabled.begin(),options.extensions.enabled.end()},
                                  std::move(m->docs),m->operator_visibility);
     check_asdl_module(m->unit);
     if(m->discovery.module.has_value()!=m->unit.module.has_value()||
        (m->unit.module&&m->unit.module->name!=m->discovery.module->name))
      throw Error(at,"inconsistent module identity in discovery and parse","ON1X-MODULE-IDENTITY");
     const auto mains=std::count_if(m->unit.functions.begin(),m->unit.functions.end(),
                                    [](const auto& f){return f.name=="main";});
     m->has_main=mains!=0;
     if(program&&(mains!=1||std::none_of(m->unit.functions.begin(),m->unit.functions.end(),
                                       [](const auto& f){
       return f.name=="main"&&!f.external&&f.params.empty()&&f.type_parameters.empty()&&
              f.constraints.empty()&&(f.result==Type::Void||f.result==Type::I64);
      })))
      throw Error(at,"program import requires exactly one local main() returning i64 or void: "+path.string(),
                  "ON1X-PROGRAM-IMPORT");
     if(m->unit.module&&mains)
      throw Error(at,"a declared module cannot define the program entry point main","ON1X-MODULE-IDENTITY");
     m->source_capability=!m->unit.ppx.empty()||!m->unit.features.empty()||!m->unit.extensions.empty()||
                          !m->unit.kernel_imports.empty()||!m->unit.foreign_imports.empty()||
                          std::any_of(m->unit.functions.begin(),m->unit.functions.end(),
                                      [](const auto& f){return f.external||!f.attributes.empty();});
     if(isolated)isolate(*m,at);
     if(program)std::erase_if(m->unit.functions,[](const auto& f){return f.name=="main";});
     if(m->unit.module&&(!m->unit.extensions.empty()||!m->unit.features.empty()))
      throw Error(at,"declaration extensions are not supported across a module boundary","ON1X-MODULE-EXTENSION");
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
        if(options.declaration_interfaces||!options.interfaces.empty()||has_signature(*m)){
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
     if(!isolated)extensions::expand(m->unit,options.extensions,path.string());
     m->unit.source_digests.emplace(path.string(),extensions::digest(m->source));
  }
    if(auto found=shared_sources.find(path);found!=shared_sources.end()){
     m->shared=found->second;
     for(const auto& op:shared_intrinsics)
      if(shared_module(op.kind)==*m->shared)
        insert(m->declarations.values,std::string(op.spelling),Symbol{m,std::string(op.identity),true},at);
    }
     for(const auto& d:m->unit.nominals){
      validate_visibility(*m,d.visibility,d.where);
      insert(m->declarations.types,d.name,Symbol{m,d.name,false,d.visibility},d.where);
     }
     for(const auto& c:m->unit.cliches){
      validate_visibility(*m,c.visibility,c.where);
      insert(m->declarations.cliches,c.name,Symbol{m,c.name,false,c.visibility},c.where);
     }
   for(const auto& f:m->unit.functions){
   if(is_reserved_intrinsic_name(f.name))throw Error(f.where,"intrinsic name cannot be redefined: "+f.name);
   // PPX cfg may select between declarations with the same name. Retain both
   // AST declarations; the normal post-PPX checker rejects remaining duplicates.
    validate_visibility(*m,f.visibility,f.where);
    if(named_module(*m)&&m->declarations.values.contains(f.name))
     throw Error(f.where,"duplicate module declaration: "+f.name,"ON1X-MODULE-IDENTITY");
    insert(m->declarations.values,f.name,Symbol{m,f.name,f.external,f.visibility},f.where);
  }
  for(const auto& macro:m->unit.macros){
    if(m->declarations.syntax.contains(macro.name))throw Error(macro.where,"duplicate macro: "+macro.name);
    validate_visibility(*m,macro.visibility,macro.where);
    insert(m->declarations.syntax,macro.name,Symbol{m,macro.name,false,macro.visibility},macro.where);
   }
    m->names=m->declarations;
    if(named_module(*m)){
     auto source_interface=path;source_interface.replace_extension(".on1xi");
     if(auto found=explicit_interfaces.find(source_interface);found!=explicit_interfaces.end()){
      if(m->member){
       const auto& interfaces=m->member->interfaces;
       if(std::find(interfaces.begin(),interfaces.end(),found->second)==interfaces.end())
        throw Error({found->second.string()},"explicit interface is not in module membership","ON1X-INTERFACE-MEMBERSHIP");
      }else verify_signatures(*m,{found->second},at);
     }
    }
   for(std::size_t i=0;i<m->unit.module_imports.size();++i){
    const auto& imp=m->unit.module_imports[i];
    if(++edges>16384)throw Error(imp.where,"import edge limit exceeded");
    // `from . import child` addresses a directory, not its currently-loading initializer.
     auto* target=m->operator_targets[i];
     if(!target)target=import_metadata(imp,*m);
     const bool own_package=!target;
     const bool explicit_program=program_import(imp);
     target=own_package?m:visit(target,imp.where,isolated||explicit_program,explicit_program);
     if(target!=m)depend(*m,*target);
     if(imp.quoted&&!explicit_program){m->quoted.push_back(target);merge(m->names,target->names,imp.where,m);continue;}
     if(!imp.selective){
      auto alias=imp.alias;
      if(alias.empty()){
       alias=explicit_program&&imp.quoted?target->path.stem().string():imp.module;
       if(explicit_program&&imp.quoted&&alias.find('.')!=std::string::npos)
        throw Error(imp.where,"program path requires an explicit module alias","ON1X-PROGRAM-IMPORT");
      }
      if(!valid_module_name(alias))throw Error(imp.where,"program path requires an explicit module alias","ON1X-PROGRAM-IMPORT");
      bind_module(m->names,alias,target,imp.where);continue;
     }
     Names selected;
     for(const auto& selection:imp.names){
     if(selection.is_operator)continue; // Already included in the operator export scope.
     const auto& name=selection.name;const auto& alias=selection.alias.empty()?name:selection.alias;
      bool found=false;
      if(!own_package){
       const auto& visible=exported(*target);
       auto select=[&](auto& into,const auto& from){
        if(auto it=from.find(name);it!=from.end()){
         require_access(it->second,*m,imp.where);
         value_alias(selected,alias,imp.where);insert(into,alias,it->second,imp.where);found=true;
        }
       };
       select(selected.values,visible.values);select(selected.syntax,visible.syntax);
       select(selected.types,visible.types);select(selected.cliches,visible.cliches);
       if(auto it=visible.modules.find(name);it!=visible.modules.end()){
        bind_module(selected,alias,it->second,imp.where);found=true;
       }
     }
     if(found)continue;
     if(named_module(*target)||(!directory(target->path)&&target->path.filename()!="__init__.on1x"))
      throw Error(imp.where,"module '"+imp.module+"' has no export '"+name+"'");
     auto child=candidate(package(*target)/name,imp.where);
      for(const auto& root:standard_roots)
       if(directory(root)&&directory(target->path)&&target->path==canonical(root,imp.where)){child=standard(name,imp.where);break;}
    if(child.empty())throw Error(imp.where,"module '"+imp.module+"' has no export or submodule '"+name+"'");
     auto* imported=metadata(child,imp.where);
     if(indexed_programs.contains(imported->path))
      throw Error(imp.where,"an indexed program requires an explicit program import: "+name,"ON1X-PROGRAM-IMPORT");
     if(named_module(*imported))throw Error(imp.where,"submodule requires a canonical named import","ON1X-MODULE-IDENTITY");
     visit(imported,imp.where,isolated);depend(*m,*imported);
     bind_module(selected,alias,imported,imp.where);
     }
     merge(m->names,selected,imp.where);
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
   if(auto it=symbols.find(text);it!=symbols.end()){
    if(current_module)require_access(it->second,*current_module,at);
    text=name(it->second);return;
   }
  if(dot!=std::string::npos){
   // Longest bound prefix, followed by explicitly imported/re-exported modules.
   for(auto cut=text.rfind('.');cut!=std::string::npos;cut=cut?text.rfind('.',cut-1):std::string::npos){
    auto it=names.modules.find(text.substr(0,cut));if(it==names.modules.end())continue;
    std::string member=text.substr(cut+1);auto* module=it->second;
    while(true){
      const auto& visible=exported(*module);
      const auto& exports=type?visible.types:syntax?visible.syntax:visible.values;
      if(auto symbol=exports.find(member);symbol!=exports.end()){
       if(current_module)require_access(symbol->second,*current_module,at);
       text=name(symbol->second);return;
      }
     bool descended=false;
     for(auto split=member.rfind('.');split!=std::string::npos;split=split?member.rfind('.',split-1):std::string::npos){
       auto child=visible.modules.find(member.substr(0,split));if(child==visible.modules.end())continue;
      module=child->second;member=member.substr(split+1);descended=true;break;
     }
     if(!descended)break;
     }
     throw Error(at,"module has no export: "+text);
    }
    throw Error(at,"unknown module qualifier: "+text);
  }
   if(names.modules.contains(text))throw Error(at,"module is not a first-class value: "+text);
    if(current_module&&current_module->program&&!type&&!syntax&&text=="main")
     throw Error(at,"imported program entry point is not available","ON1X-PROGRAM-IMPORT");
   if(!type&&globals.contains(text)&&!is_intrinsic_name(text))throw Error(at,"name is not imported in this module: "+text);
   if(type&&type_globals.contains(text))throw Error(at,"type is not imported in this module: "+text);
   // Unknown unqualified names remain for the ordinary checker, or a PPX which
   // generates globals (derive/test). The loader never infers types or effects.
   }
    void cliche_reference(std::string& text,const Location& at,const Names& names){
     if(auto found=names.cliches.find(text);found!=names.cliches.end()){
      if(current_module)require_access(found->second,*current_module,at);
     text=name(found->second);return;
    }
    if(auto dot=text.find('.');dot!=std::string::npos){
     for(auto cut=text.rfind('.');cut!=std::string::npos;
         cut=cut?text.rfind('.',cut-1):std::string::npos){
      auto module=names.modules.find(text.substr(0,cut));if(module==names.modules.end())continue;
      std::string member=text.substr(cut+1);auto* owner=module->second;
      while(true){
        const auto& visible=exported(*owner);
        if(auto found=visible.cliches.find(member);found!=visible.cliches.end()){
         if(current_module)require_access(found->second,*current_module,at);
        text=name(found->second);return;
       }
       bool descended=false;
       for(auto split=member.rfind('.');split!=std::string::npos;
           split=split?member.rfind('.',split-1):std::string::npos){
         auto child=visible.modules.find(member.substr(0,split));
         if(child==visible.modules.end())continue;
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
                                                bool cliche,const Location& at)const{
     const auto direct=[&](const Module* module,const std::string& key)
       ->std::optional<std::string>{
       const auto& visible=exported(*module);
       const auto& symbols=cliche?visible.cliches:visible.types;
       if(auto found=symbols.find(key);found!=symbols.end()){
        if(current_module)require_access(found->second,*current_module,at);
        return name(found->second);
       }
      return {};
     };
     const auto& local=cliche?names.cliches:names.types;
      if(auto found=local.find(text);found!=local.end()){
       if(current_module)require_access(found->second,*current_module,at);
       return name(found->second);
      }
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
         const auto& children=exported(*owner).modules;
         auto child=children.find(member.substr(0,split));
         if(child==children.end())continue;
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
       auto cliche=named_reference(e.text,names,true,e.where);
       auto nominal=named_reference(e.text,names,false,e.where);
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
        if(auto nominal=named_reference(spelling,names,false,e.where)){
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
     if(verified_signatures.contains(origin))continue;
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
   void index_membership(){
    const Location at{"<project-module-index>"};
    const auto valid_root=[&](const fs::path& root){
     if(!root.is_absolute()||canonical(root,at)!=root||!directory(root))
      throw Error(at,"expected a canonical project package directory: "+root.string(),"ON1X-MODULE-MEMBERSHIP");
     known_packages.insert(root);
    };
    const auto valid_file=[&](const fs::path& file,const fs::path& root,std::string_view suffix){
     if(!contained(root,file)||file.extension()!=suffix||canonical(file,at)!=file||!regular(file))
      throw Error(at,"invalid project source membership: "+file.string(),"ON1X-MODULE-MEMBERSHIP");
    };
    if(!options.project_package.empty())valid_root(options.project_package);
    else if(!options.module_members.empty()||!options.scoped_module_members.empty()||!options.program_members.empty())
     throw Error(at,"module membership requires a project package","ON1X-MODULE-MEMBERSHIP");
    std::map<std::pair<fs::path,std::string>,const Member*> identities;
    const auto read=[&](const auto& any_scope){
     for(const auto& [import,member]:any_scope){
      if(!valid_module_name(import)||!valid_module_name(member.declared_name)||
         member.fragments.empty()||member.fragments.size()>options.max_modules)
       throw Error(at,"invalid project module membership: "+import,"ON1X-MODULE-MEMBERSHIP");
      auto [original,added]=identities.emplace(std::make_pair(member.package,member.declared_name),&member);
      if(!added){
       if(original->second->fragments!=member.fragments||original->second->interfaces!=member.interfaces)
        throw Error(at,"conflicting membership records for "+member.declared_name,"ON1X-MODULE-MEMBERSHIP");
       continue;
      }
      valid_root(member.package);
      std::set<fs::path> seen;
      for(const auto& fragment:member.fragments){
       valid_file(fragment,member.package,".on1xm");
       if(!seen.insert(fragment).second||!enrolled.emplace(fragment,&member).second)
        throw Error(at,"a source belongs to more than one fragment membership: "+fragment.string(),"ON1X-MODULE-MEMBERSHIP");
      }
      for(const auto& interface:member.interfaces){
       valid_file(interface,member.package,".on1xi");
       if(!seen.insert(interface).second)
        throw Error(at,"duplicate interface membership: "+interface.string(),"ON1X-MODULE-MEMBERSHIP");
      }
     }
    };
    read(options.module_members);
    for(const auto& [scope,membership]:options.scoped_module_members){valid_root(scope);read(membership);}
    for(const auto& [name,path]:options.program_members){
     if(!valid_module_name(name))throw Error(at,"invalid project program name: "+name,"ON1X-MODULE-MEMBERSHIP");
     valid_file(path,options.project_package,".on1x");
     indexed_programs.insert(path);
    }
    for(const auto& path:options.interfaces){
     const Location where{path.string()};
     if(path.extension()!=".on1xi"||!regular(path))
      throw Error(where,"expected a regular .on1xi file","ON1X-INTERFACE-FILE");
     auto origin=canonical(path,where);
     if(origin.extension()!=".on1xi")
      throw Error(where,"interface must resolve to a .on1xi file","ON1X-INTERFACE-FILE");
     if(!explicit_interfaces.emplace(origin,origin).second)
      throw Error(where,"duplicate interface file","ON1X-INTERFACE-DUPLICATE");
    }
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
     index_membership();
    (void)discover_preludes(input);
    const Location at{input.string()};
    auto* entry=metadata(input,at,false);
     auto result=parse_with_operators(entry->scanned,
            {options.extensions.enabled.begin(),options.extensions.enabled.end()},
             std::move(entry->docs),visible_operators(*entry,at));
     check_asdl_module(result);
     return result;
   }
   LoadedUnit run(const fs::path& input){
    if(options.kernel_load_time&&
       (inspection||options.standalone_guest||options.kernel_c11||options.kernel_stage||
        !options.extensions.enabled.empty()))
     throw Error({input.string()},"load-time stdffi cannot be combined with KernelBind, guest or declaration extension profiles",
                 "ON1X-KERNEL-PROFILE");
    if(options.interfaces.size()>max_interface_files)
    throw Error({input.string()},"interface file count limit exceeded","ON1X-INTERFACE-LIMIT");
    if((options.declaration_interfaces||!options.interfaces.empty())&&
       (inspection||options.kernel_c11||!options.extensions.enabled.empty()))
    throw Error({input.string()},"interface verification cannot run with KernelBind or declaration extensions",
                 "ON1X-INTERFACE-UNSUPPORTED");
     index_membership();
    const Location prelude_at{"<prelude>"};
    const auto discovered=discover_preludes(input);
    implicit=true;
    if(discovered.pervasive)preludes.push_back(visit(discovered.pervasive,prelude_at));
    if(discovered.operators)visit(discovered.operators,prelude_at); // Open only its operator exports.
    implicit=false;
    for(auto* module:discovered.configured)preludes.push_back(visit(module,prelude_at));
    const Location entry_at{input.string()};
    auto* entry=metadata(input,entry_at,false);
    if(!options.project_package.empty()&&named_module(*entry)&&!entry->member)
     throw Error(entry_at,"declared module entry is not in project membership","ON1X-MODULE-MEMBERSHIP");
    if(entry->member)visit(group_metadata(*entry->member,entry_at),entry_at);
    else visit(entry,entry_at);
   if(inspection)return {};
    if(!named_module(*entry))legacy(entry);
  // Pick an identifier prefix absent from every token, including local binders.
  while(std::any_of(tokens.begin(),tokens.end(),[&](const auto& t){return t.starts_with(prefix);}))prefix+="m_";
  Names flat;for(auto* m:order)if(m->legacy)merge(flat,m->names,{m->path.string()});
    for(auto* m:order)for(const auto& f:m->unit.functions)globals.insert(name({m,f.name,f.external}));
    for(auto* m:order)for(const auto& d:m->unit.nominals)type_globals.insert(name({m,d.name}));
    for(auto* m:order)for(const auto& c:m->unit.cliches)cliche_globals.insert(name({m,c.name}));
    LoadedUnit result;std::map<std::string,const Function*> externs;std::set<std::string> lazy;
    result.unit.module=entry->unit.module;
    if(entry->unit.module)result.unit.module_imports=entry->unit.module_imports;
    result.declaration_interface_members=declaration_interface_members;
    std::set<std::pair<fs::path,fs::path>> emitted_edges;
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
      result.unit.requires_module_boundary|=named_module(*m)||m->program;
     const auto scope_key=m->path.string();
      record_operator_scope(result.unit,m->operator_visibility,scope_key);
    for(auto& doc:m->unit.docstrings)result.unit.docstrings.push_back(std::move(doc));
     auto names=m->legacy?flat:m->names;
     if(m->group)merge(names,m->group->declarations,{m->path.string()},m);
     if(m!=entry||!entry->unit.module)
      for(const auto& imp:m->unit.module_imports)
       if(program_import(imp))result.unit.module_imports.push_back(imp);
     const auto add_edge=[&](const Module* target){
      if(!regular(m->path)||!regular(target->path))return;
      if(emitted_edges.emplace(m->path,target->path).second){
       if(emitted_edges.size()>16384)throw Error({m->path.string()},"physical dependency edge limit exceeded");
       result.source_edges.emplace_back(m->path,target->path);
      }
     };
     for(auto* dep:m->dependencies){
      if(dep->fragments.empty())add_edge(dep);
      else for(auto* fragment:dep->fragments)add_edge(fragment);
     }
      for(const auto& [alias,request]:m->kernels)if(import_alias_conflicts(names,alias))
       throw Error(request.where,"conflicting kernel import alias: "+alias,"ON1X-KERNEL-ALIAS");
      for(const auto& [alias,request]:m->foreigns)if(import_alias_conflicts(names,alias))
       throw Error(request.where,"conflicting stdffi import alias: "+alias,"ON1X-STDFFI-ALIAS");
    for(auto it=preludes.rbegin();it!=preludes.rend();++it){
     auto* prelude=*it;
     const auto bound=[&](const std::string& key){return module_bound(names,key)||m->foreigns.contains(key);};
      const auto& allowed=exported(*prelude);
      const auto inject=[&](auto& into,const auto& from){
       for(const auto& [key,symbol]:from)
        if(!into.contains(key)&&!bound(key)&&accessible(*symbol.owner,*m,symbol.visibility))
         insert(into,key,symbol,{m->path.string()});
      };
      inject(names.values,allowed.values);inject(names.syntax,allowed.syntax);
      inject(names.types,allowed.types);inject(names.cliches,allowed.cliches);
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

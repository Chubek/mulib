#include <on1x/loader.hpp>
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
 std::map<std::string,Symbol> values,syntax;
 std::map<std::string,Module*> modules;
};
struct Module {
 fs::path path;
 Unit unit;
 Names names;
 std::vector<Module*> quoted;
 std::vector<Module*> dependencies;
 std::size_t id=0;
 bool active=true,legacy=false,implicit=false;
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

class Loader {
 const LoaderOptions& options;
 fs::path entry_directory;
 std::map<fs::path,std::unique_ptr<Module>> modules;
 std::vector<Module*> order,stack,preludes;
 std::set<std::string> tokens,globals;
 std::string prefix="on1x_module_";
 std::size_t bytes=0,bindings=0,edges=0;
 bool implicit=false;
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
   auto first=candidate(options.stdlib/module_path(module),at);
   auto second=candidate(options.stdlib.parent_path()/"std"/module_path(module),at);
   if(!first.empty()&&!second.empty())throw Error(at,"ambiguous standard module: "+module);
   return first.empty()?second:first;
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
   if(options.stdlib.empty())throw Error(imp.where,"standard library root is not configured");
    auto found=imp.module=="std"?options.stdlib:standard(imp.module.substr(4),imp.where);
   if(!found.empty()&&fs::exists(found))return found;
   throw Error(imp.where,"standard module not found: "+imp.module);
  }
  std::vector<fs::path> roots{entry_directory};roots.insert(roots.end(),options.paths.begin(),options.paths.end());
  if(!options.stdlib.empty())roots.push_back(options.stdlib);
  std::string searched;
  for(const auto& root:roots){
   auto found=candidate(root/relative,imp.where);if(!found.empty())return found;
   if(!searched.empty())searched+=", ";
   searched+=root.string();
  }
  throw Error(imp.where,"module not found: "+imp.module+" (searched "+searched+")");
 }
 void bind_module(Names& names,const std::string& alias,Module* target,const Location& at){
  auto first=alias.substr(0,alias.find('.'));
  if(names.values.contains(first)||names.syntax.contains(first))throw Error(at,"module alias conflicts with declaration: "+first);
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
    auto source=replacement==options.source_overrides.end()?read_source_file(path.string()):replacement->second;
   if(source.size()>options.max_bytes-bytes)throw Error(at,"total imported source byte limit exceeded");
   bytes+=source.size();auto scanned=scan(source,path.string());
   for(const auto& t:scanned)tokens.insert(t.text);
   m->unit=parse(scanned);
  }
  for(const auto& f:m->unit.functions){
   if(is_intrinsic_name(f.name))throw Error(f.where,"intrinsic name cannot be redefined: "+f.name);
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
     if(auto it=target->names.modules.find(name);it!=target->names.modules.end()){bind_module(m->names,alias,it->second,imp.where);found=true;}
    }
    if(found)continue;
    if(!directory(target->path)&&target->path.filename()!="__init__.on1x")throw Error(imp.where,"module '"+imp.module+"' has no export '"+name+"'");
     auto child=candidate(package(*target)/name,imp.where);
     if(directory(options.stdlib)&&directory(target->path)&&target->path==canonical(options.stdlib,imp.where))child=standard(name,imp.where);
    if(child.empty())throw Error(imp.where,"module '"+imp.module+"' has no export or submodule '"+name+"'");
    auto* imported=visit(child,imp.where);m->dependencies.push_back(imported);
    bind_module(m->names,alias,imported,imp.where);
   }
  }
  stack.pop_back();m->active=false;order.push_back(m);return m;
 }
 void legacy(Module* m){if(m->legacy)return;m->legacy=true;for(auto* child:m->quoted)legacy(child);}
 std::string name(const Symbol& s)const{return s.external||s.owner->legacy?s.name:prefix+std::to_string(s.owner->id)+"_"+s.name;}
 using Scope=std::set<std::string>;
 void bind(Scope& scope,const Pattern& p){if(p.is_tuple())for(const auto& child:p.elements)bind(scope,child);else scope.insert(p.name);}
 void parameters(Scope& scope,const std::vector<Param>& ps){for(const auto& p:ps){if(p.pattern)bind(scope,*p.pattern);else scope.insert(p.name);}}
 void reference(std::string& text,const Location& at,const Names& names,const Scope& locals,bool syntax){
  auto dot=text.find('.');auto first=text.substr(0,dot);
  if(locals.contains(first)){
   if(dot!=std::string::npos)throw Error(at,"qualified reference is shadowed by local binding: "+first);
   return;
  }
  const auto& symbols=syntax?names.syntax:names.values;
  if(auto it=symbols.find(text);it!=symbols.end()){text=name(it->second);return;}
  if(dot!=std::string::npos){
   // Longest bound prefix, followed by explicitly imported/re-exported modules.
   for(auto cut=text.rfind('.');cut!=std::string::npos;cut=cut?text.rfind('.',cut-1):std::string::npos){
    auto it=names.modules.find(text.substr(0,cut));if(it==names.modules.end())continue;
    std::string member=text.substr(cut+1);auto* module=it->second;
    while(true){
     const auto& exports=syntax?module->names.syntax:module->names.values;
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
  if(globals.contains(text)&&!is_intrinsic_name(text))throw Error(at,"name is not imported in this module: "+text);
  // Unknown unqualified names remain for the ordinary checker, or a PPX which
  // generates globals (derive/test). The loader never infers types or effects.
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
  if(e.kind==Expr::Kind::Variable)reference(e.text,e.where,names,values,false);
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
  if(e.kind==Expr::Kind::Lambda){auto inner=values;parameters(inner,e.parameters);body(e.body,names,inner,syntax);return;}
  if(e.callee)expression(*e.callee,names,values,syntax);
  if(e.syntax_tail)expression(*e.syntax_tail,names,values,syntax);
  for(auto& child:e.children)expression(*child,names,values,syntax);
  body(e.body,names,values,syntax);
 }
 void body(std::vector<Stmt>& statements,const Names& names,const Scope& outer,const Scope& syntax){
  if(statements.empty())return;
  auto values=outer;
  for(auto& s:statements){
   if(s.value)expression(*s.value,names,values,syntax);
   if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(values,*s.pattern);else values.insert(s.name);}
   if(s.kind==Stmt::Kind::Assign)reference(s.name,s.where,names,values,false);
   body(s.body,names,values,syntax);body(s.otherwise,names,values,syntax);
  }
 }
public:
 explicit Loader(const LoaderOptions& opts):options(opts){}
 LoadedUnit run(const fs::path& input){
  entry_directory=canonical(input,{input.string()}).parent_path();
  if(options.prelude){
   if(options.stdlib.empty())throw Error({input.string()},"automatic prelude requires --stdlib or --no-prelude");
   implicit=true;preludes.push_back(visit(options.stdlib/"pervasive.on1x",{"<prelude>"},false));implicit=false;
  }
  for(const auto& path:options.preludes)preludes.push_back(visit(path,{"<prelude>"},false));
  auto* entry=visit(input,{input.string()},false);legacy(entry);
  // Pick an identifier prefix absent from every token, including local binders.
  while(std::any_of(tokens.begin(),tokens.end(),[&](const auto& t){return t.starts_with(prefix);}))prefix+="m_";
  Names flat;for(auto* m:order)if(m->legacy)merge(flat,m->names,{m->path.string()});
  for(auto* m:order)for(const auto& f:m->unit.functions)globals.insert(name({m,f.name,f.external}));
  LoadedUnit result;std::map<std::string,const Function*> externs;std::set<std::string> lazy;
  // Reserve storage so pointers used for identical raw-extern admission stay valid.
  std::size_t count=0;for(auto* m:order)count+=m->unit.functions.size();
  if(count>16384)throw Error({input.string()},"loaded function count limit exceeded");
  result.unit.functions.reserve(count);
  for(auto* m:order){
   auto names=m->legacy?flat:m->names;
   for(auto it=preludes.rbegin();it!=preludes.rend();++it){
    auto* prelude=*it;
    for(const auto& [key,symbol]:prelude->names.values)if(!names.values.contains(key)&&!module_bound(names,key))insert(names.values,key,symbol,{m->path.string()});
    for(const auto& [key,symbol]:prelude->names.syntax)if(!names.syntax.contains(key)&&!module_bound(names,key))insert(names.syntax,key,symbol,{m->path.string()});
   }
   for(auto& f:m->unit.functions){
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
   if(regular(m->path))result.dependencies.push_back(m->path);
  }
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
  for(const auto& f:result.unit.functions){
   auto& refs=calls[f.name];statements(statements,f.body,refs);for(const auto& a:f.attributes)collect(collect,*a,refs);
   if(!lazy.contains(f.name))needed.insert(f.name);
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
 if(const char* root=std::getenv("ON1X_STDLIB");root&&*root)options.stdlib=root;
 else{
  fs::path binary=executable;
  if(!binary.has_parent_path())if(const char* path=std::getenv("PATH")){
   std::istringstream search(path);std::string dir;
   while(std::getline(search,dir,':'))if(!dir.empty()&&regular(fs::path(dir)/binary)){binary=fs::path(dir)/binary;break;}
  }
  std::error_code ec;auto resolved=fs::canonical(binary,ec);
  if(!ec&&directory(resolved.parent_path()/"../share/on1x/stdlib"))options.stdlib=resolved.parent_path()/"../share/on1x/stdlib";
#ifdef ON1X_STDLIB_ROOT
  else options.stdlib=ON1X_STDLIB_ROOT;
#endif
 }
 if(const char* path=std::getenv("ON1X_PATH")){
  std::istringstream search(path);std::string dir;
#ifdef _WIN32
  constexpr char separator=';';
#else
  constexpr char separator=':';
#endif
  while(std::getline(search,dir,separator))if(!dir.empty())options.paths.emplace_back(dir);
 }
 return options;
}
LoadedUnit load_unit(const fs::path& input,const LoaderOptions& options){return Loader(options).run(input);}
}

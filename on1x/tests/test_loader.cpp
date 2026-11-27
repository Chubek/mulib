#include <on1x/loader.hpp>
#include <on1x/clone.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace {
void standard_module_imports(const std::filesystem::path& root){
 namespace fs=std::filesystem;
 using namespace on1x;
 const auto stdlib=root/"stdlib",project=root/"project",untrusted=root/"untrusted",alternate=root/"alternate";
 const auto entry=project/"entry.on1x";
 const auto primary=stdlib/"Tools.on1xm",side=stdlib/"Tools.fragments"/"second.on1xm";
 const auto signature=stdlib/"Tools.on1xi",side_signature=stdlib/"Tools.fragments"/"second.on1xi";
 const auto wrapper=stdlib/"Compat"/"__init__.on1x";
 const auto write=[](const fs::path& path,const std::string& source){
  fs::create_directories(path.parent_path());
  std::ofstream out(path);out<<source;
  if(!out)throw std::runtime_error("cannot create standard module fixture");
 };
 const auto require=[](bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);};
 LoaderOptions options;
 options.prelude=false;
 options.stdlib=stdlib;
 write(primary,"module std.Tools; public fn answer()->i64{return impl();}"
               "public fn omitted()->i64{return -1;}"
               "public operator(lexeme: \"%%\", arity: \"binary\", formals: x:i64,y:i64)"
               "->i64{return x+y;}");
 write(side,"module std.Tools; shared fn impl()->i64{return 42;}"
            "public fn extra()->i64{return 3;}");
 write(signature,"abstract module std.Tools; fn answer()->i64;");
 write(side_signature,"abstract module std.Tools; fn extra()->i64;");
 write(wrapper,"import \"../Tools.on1x\"; from ..Tools import answer as relative_answer;"
               "fn via_relative()->i64{return relative_answer();}");
 write(untrusted/"On1xfile","(On1xfile 1 (kind \"stdlib\") (name \"std\") (root \".\"))");
 write(untrusted/"Tools.on1x","fn answer()->i64{return -13;}");
 write(project/"std"/"Tools.on1xm","module std.Tools; fn answer()->i64{return -17;}");
 write(entry,"import std.Tools as A; from std import Tools as B; import std.Compat as C;import Tools as D;"
             "import \"../stdlib/Tools.on1x\"; "
             "fn main(){assert(A.answer()==B.answer());assert(B.answer()==C.via_relative());"
             "assert(C.answer()==answer());assert(D.answer()==42);assert(A.extra()==3);}");
 auto loaded=load_unit(entry,options);
 require(loaded.unit.requires_module_boundary,"legacy alias erased standard module boundary");
 require(loaded.dependencies==std::vector<fs::path>{primary,side,wrapper,entry},
         "standard aliases did not share one group");
 require(loaded.signature_dependencies==std::vector<fs::path>{side_signature,signature},
         "trusted interfaces were not reported");
 require(std::count_if(loaded.unit.functions.begin(),loaded.unit.functions.end(),
                       [](const auto& f){return f.name.ends_with("_answer");})==1,
         "standard body duplicated or dropped its canonical name");
 check(loaded.unit);
 (void)emit_c(loaded.unit,"standard-client.on1x");
 options.paths={untrusted};
 write(entry,"import std.Tools as A;from std import Tools as B;fn main(){A.extra();B.extra();}");
 auto trusted=load_unit(entry,options);
 check(trusted.unit);
 const auto reject=[&](const std::string& source,const std::string& message){
  write(entry,source);
  try{(void)load_unit(entry,options);}
  catch(const Error& error){
   if(std::string(error.what()).find(message)!=std::string::npos||error.diagnostic_id.starts_with(message))return;
   throw;
  }
  throw std::runtime_error("unexpectedly accepted: "+source);
 };
 reject("import std.Compat as C;fn main(){C.omitted();}","no export");
 reject("import std.Tools as C;fn main(){C.impl();}","no export");
 reject("from std.Tools import (operator(\"%%\"));fn main(){}","no operator export");
 reject("import \"std/Tools.on1xm\";fn main(){}","reserved standard identity");
 reject("import \"../stdlib/Tools.on1xm\";fn main(){}","identity");
 write(alternate/"On1xfile","(On1xfile 1 (kind \"stdlib\") (name \"std\") (root \".\"))");
 write(alternate/"Tools.on1xm","module std.Tools; fn answer()->i64{return -2;}");
 write(alternate/"Tools.on1xi","abstract module std.Tools; fn answer()->i64;");
 options.stdlib_explicit=true;
 options.stdlib_paths={alternate};
 reject("import \"../alternate/Tools.on1x\";import std.Tools;fn main(){}","nonselected root");
 options.stdlib_explicit=false;
 options.stdlib_paths.clear();
 write(side,"module std.Tools; fn extra()->i64{return +;}");
 const auto parsed=parse_source_with_operator_imports(entry,
   "import \"../stdlib/Tools.on1x\";fn main(){answer();}",options);
 require(parsed.functions.size()==1,"formatting parsed a dependency body");
 write(side,"module std.Tools; shared fn impl()->i64{return 42;}"
            "public fn extra()->i64{return 3;}");
 options.project_package=project;
 write(entry,"import std.Tools as A;fn main(){assert(A.answer()==42);}");
 auto project_client=load_unit(entry,options);
 check(project_client.unit);
 options.module_members.emplace("std.Tools",LoaderOptions::ModuleMember{
                                  project,"std.Tools",{project/"std"/"Tools.on1xm"},{}});
 reject("import std.Tools;fn main(){}","invalid project module membership");
 options.module_members.clear();
 write(side_signature,"abstract module std.Tools; fn extra()->bool;");
 reject("import std.Tools;fn main(){}","ON1X-INTERFACE");
 write(side_signature,"abstract module std.Tools; fn extra()->i64;");
 fs::remove(signature);
 reject("import std.Tools;fn main(){}","expected a local interface");
 write(signature,"abstract module std.Tools; fn answer()->i64;");
}
void module_imports(const std::filesystem::path& root){
 namespace fs=std::filesystem;
 using namespace on1x;
 fs::create_directories(root/"nested");
 fs::create_directories(root/"project");
 const auto entry=root/"entry.on1x";
 const auto modern=root/"nested"/"Choice.on1xm";
 const auto program=root/"program.on1x";
 const auto helper=root/"helper.on1x";
 const auto write=[](const fs::path& path,const std::string& source){
  std::ofstream out(path);out<<source;
  if(!out)throw std::runtime_error("cannot create module fixture");
 };
 const auto require=[](bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);};
 LoaderOptions options;options.prelude=false;
 auto reject=[&](const std::string& source,const std::string& message){
  write(entry,source);
  try{(void)load_unit(entry,options);}
  catch(const Error& error){
   if(std::string(error.what()).find(message)!=std::string::npos||error.diagnostic_id.starts_with(message))return;
   throw;
  }
  throw std::runtime_error("unexpectedly accepted: "+source);
 };

 write(modern,"module nested.Choice; private fn secret()->i64{return 40;}"
              "public fn answer()->i64{return secret()+2;}"
              "private operator(lexeme: \"%%\", arity: \"binary\", formals: x:i64,y:i64)"
              "->i64{return x+y;}");
 write(entry,"import nested.Choice as c;import nested.Choice : answer as get;"
             "fn main(){assert(c.answer()==get());}");
 auto modules=load_unit(entry,options);
 require(modules.unit.requires_module_boundary,"declared module did not retain its boundary");
 require(modules.dependencies==std::vector<fs::path>{modern,entry},"wrong canonical module order");
 require(modules.source_edges==std::vector<std::pair<fs::path,fs::path>>{{entry,modern}},
         "canonical import diamond was not deduplicated");
 check(modules.unit);
 reject("import nested.Choice as c;fn main(){c.secret();}","not visible");
 reject("from nested.Choice import answer,secret;fn main(){}","not visible");
 reject("from nested.Choice import (operator(\"%%\"));fn main(){}","no operator export");
 reject("import nested.Choice : answer as get,answer as get;fn main(){}","duplicate selector alias");
 reject("import \"nested/Choice.on1xm\";fn main(){}","canonical module identity");

 write(helper,"fn number()->i64{return 42;}");
 write(program,"import helper;fn main(){println(999031);}"
               "fn public_result()->i64{return helper.number();}");
 write(entry,"import program \"program.on1x\" as p;"
             "fn main(){assert(p.public_result()==42);}");
 auto imported=load_unit(entry,options);
 require(imported.unit.requires_module_boundary&&imported.unit.module_imports.size()==1,
         "program import did not retain its boundary");
 require(imported.dependencies==std::vector<fs::path>{helper,program,entry},
         "program dependencies are not ordered correctly");
 require(imported.unit.functions.size()==3,"imported main reached the executable");
 check(imported.unit);
 require(emit_c(imported.unit,"entry.on1x").find("999031")==std::string::npos,
         "imported main body reached the emitter");
 reject("import program \"program.on1x\" as p;fn main(){p.main();}","no export");
 reject("import program \"program.on1x\" as p;fn main(){p.helper.number();}","no export");
 write(entry,"import program \"program.on1x\" : public_result as get;fn main(){assert(get()==42);}");
 auto selected=load_unit(entry,options);
 check(selected.unit);
 reject("import program \"program.on1x\" : public_result as get,main;fn main(){}","no export");
 reject("import program \"program.on1x\" : public_result as get;fn main(){program.public_result();}",
        "unknown module qualifier");
 reject("import program \"program.on1x\" : public_result as get,public_result as get;fn main(){}",
        "duplicate selector alias");
 write(helper,"extern fn number()->i64;");
 reject("import program \"program.on1x\" as p;fn main(){}","ON1X-PROGRAM-CAPABILITY");
 write(helper,"@ppx identity \"must-not-run.py\";fn number()->i64{return 42;}");
 reject("import program \"program.on1x\" as p;fn main(){}","ON1X-PROGRAM-CAPABILITY");
 write(helper,"fn main(){}fn number()->i64{return 42;}");
 reject("import program \"program.on1x\" as p;fn main(){}","needs an explicit program import");

 const auto a=root/"project"/"a.on1xm",b=root/"project"/"b.on1xm";
 const auto sig=root/"project"/"signature.on1xi";
 write(a,"module Work.Joined;public fn first()->i64{return second();}");
 write(b,"module Work.Joined;shared fn second()->i64{return 42;}");
 write(sig,"abstract module Work.Joined;fn first()->i64;");
 options.project_package=root;
 options.module_members.emplace("Work.Joined",LoaderOptions::ModuleMember{root,"Work.Joined",{a,b},{sig}});
 write(entry,"import Work.Joined as w;fn main(){assert(w.first()==42);}");
 auto grouped=load_unit(entry,options);
 require(grouped.dependencies==std::vector<fs::path>{a,b,entry},"fragment dependency order changed");
 require(grouped.source_edges==std::vector<std::pair<fs::path,fs::path>>{{entry,a},{entry,b}},
         "module graph did not report both fragment edges");
 check(grouped.unit);
 reject("import Work.Joined as w;fn main(){w.second();}","no export");
 write(sig,"abstract module Work.Joined;fn first()->bool;");
 reject("import Work.Joined as w;fn main(){}","ON1X-INTERFACE");
 write(helper,"fn number()->i64{return 42;}");
 options.program_members.emplace("Tool",program);
 write(entry,"import program Tool as p;fn main(){assert(p.public_result()==42);}");
 auto indexed=load_unit(entry,options);
 check(indexed.unit);
 write(entry,"import program \"program.on1x\" : public_result;fn main(){assert(public_result()==42);}");
 indexed=load_unit(entry,options);
 check(indexed.unit);
 reject("import program;fn main(){}","requires an explicit program import");
 reject("import program \"helper.on1x\" as h;fn main(){}","program path is not in the project index");
}
}

int main(int argc,char** argv){
 assert(argc==2);namespace fs=std::filesystem;
 fs::path root=argv[1];fs::create_directories(root);
 module_imports(fs::canonical(root)/"module-imports");
 standard_module_imports(fs::canonical(root)/"standard-module-imports");
 std::ofstream(root/"a.on1x")<<"import b;fn main(){}";
 std::ofstream(root/"b.on1x")<<"fn value()->i64{return 42;}";
 on1x::LoaderOptions options;options.prelude=false;
 auto reject=[&](const char* expected){
  bool rejected=false;
  try{on1x::load_unit(root/"a.on1x",options);}
  catch(const on1x::Error& error){rejected=std::string(error.what()).find(expected)!=std::string::npos;}
  assert(rejected);
 };
 options.max_modules=1;reject("module count");options.max_modules=1024;
 options.max_depth=1;reject("recursion limit");options.max_depth=64;
 options.max_bytes=30;reject("source byte limit");options.max_bytes=1024;
 auto first=on1x::load_unit(root/"a.on1x",options);
 auto second=on1x::load_unit(root/"a.on1x",options);
 assert(first.dependencies==second.dependencies&&first.unit.functions.size()==2);
 std::ofstream(root/"b.on1x")<<"fn changed()->i64{return 43;}";
 auto changed=on1x::load_unit(root/"a.on1x",options);
  assert(changed.unit.functions.front().name!=first.unit.functions.front().name);
  std::ofstream(root/"a.on1x")<<"import kernel mulib.clamp as C;fn main(){println(C.add_i64(40,2));}";
  options.kernel_c11=true;
  reject("ON1X-KERNEL-NOT-ADMITTED");
  options.kernel_stage=true;
  auto scalar=on1x::load_unit(root/"a.on1x",options);
  on1x::check(scalar.unit);
  auto code=on1x::emit_c(scalar.unit,"a.on1x");
  assert(code.find("extern int64_t on1x_kernel_scalar_v1_mulib_clamp_add_i64")!=std::string::npos);
  assert(code.find("#include <mulib-clamp.h>")==std::string::npos);
  auto& foreign=scalar.unit.functions.back();
  assert(foreign.kernel_link_only&&foreign.kernel_symbol=="mulib_clamp_add_i64");
  std::size_t budget=1000;
  auto cloned=on1x::clone_unit(scalar.unit,budget);
  const auto& copied=cloned.functions.back();
  assert(copied.kernel_link_only&&copied.kernel_symbol==foreign.kernel_symbol);
  foreign.result=on1x::Type::String;
  bool rejected=false;
  try{on1x::emit_c(scalar.unit,"a.on1x");}
  catch(const on1x::Error& error){rejected=error.diagnostic_id=="ON1X-KERNEL-ABI";}
  assert(rejected);
  options.kernel_stage=false;
  std::ofstream(root/"a.on1x")<<"unsafe import kernel mulib.clamp as C;"
    "fn main(){let n=unsafe { let x=40; C.add_i64(x,2) };println(n);}";
  auto unsafe=on1x::load_unit(root/"a.on1x",options);
  on1x::check(unsafe.unit);
  auto& region=*unsafe.unit.functions.front().body.front().value;
  assert(region.kind==on1x::Expr::Kind::Region&&region.unsafe_region);
  assert(region.children.at(0)->callee->reference==on1x::Expr::Reference::UnsafeForeign);
  assert(on1x::emit_c(unsafe.unit,"a.on1x").find("unsafe_foreign:")!=std::string::npos);
  region.unsafe_region=false; // A transform cannot silently detach the call from its authority.
  rejected=false;
  try{on1x::emit_c(unsafe.unit,"a.on1x");}
   catch(const on1x::Error& error){rejected=error.diagnostic_id=="ON1X-VALUES-VERIFY";}
   assert(rejected);
   if(!on1x::stdffi::has_kernel("clamp"))return 0;

   options.kernel_c11=false;
   options.kernel_load_time=true;
   options.foreign_default_paths={root/"absent-recipes"};
   std::ofstream(root/"b.on1x")<<"import foreign stdffi.clamp as Local;"
    "fn via_helper()->i64{let add=Local.add_i64;return add(40,2);}";
   std::ofstream(root/"a.on1x")<<"import b;import foreign stdffi.clamp as C;"
    "extern fn on1x_environ_argc()->i64;"
    "fn main(){println(b.via_helper());println(C.add_i64(1,2));"
    "println(C.i64(27,0,10));println(on1x_environ_argc());}";
   auto late=on1x::load_unit(root/"a.on1x",options);
   on1x::check(late.unit);
   code=on1x::emit_c(late.unit,"a.on1x");
   const std::string add_record=".export_name = \"add_i64\"";
   const auto add_slot=code.find(add_record);
   assert(add_slot!=std::string::npos&&code.find(add_record,add_slot+1)==std::string::npos);
   const auto interval_slot=code.find(".export_name = \"i64\"");
   assert(interval_slot!=std::string::npos&&add_slot<interval_slot);
   assert(code.find(".export_name = \"sub_i64\"")==std::string::npos);
   const auto init=code.find("if (on1x_kernel_import_init(on1x_kernel_required_imports, 2) != 0) return 1;");
   const auto environ_init=code.find("on1x_environ_init(argc, argv);");
   assert(environ_init!=std::string::npos&&init<environ_init);
   assert(code.find("on1x_kernel_call2(0, a, b)")!=std::string::npos);
   assert(code.find("on1x_kernel_call3(1, a, b, c)")!=std::string::npos);
   assert(code.find("on1x_adapter_kernel_load_time_add_i64")!=std::string::npos);
   assert(code.find("mulib_clamp")==std::string::npos&&code.find("stdffi_clamp")==std::string::npos);

   std::ofstream(root/"a.on1x")<<"import foreign stdffi.clamp as C;fn main(){}";
   auto unused=on1x::load_unit(root/"a.on1x",options);
   on1x::check(unused.unit);
   assert(on1x::emit_c(unused.unit,"a.on1x").find("on1x_kernel_import")==std::string::npos);

   std::ofstream(root/"a.on1x")<<"import foreign stdffi.clamp as C;fn main(){println(C.unknown(1,2));}";
   reject("no load-time stdffi export");
   std::ofstream(root/"a.on1x")<<"import foreign stdffi.clamp as C;fn main(){println(C.i64(1,2));}";
   auto wrong_arity=on1x::load_unit(root/"a.on1x",options);
   rejected=false;
   try{on1x::check(wrong_arity.unit);}
   catch(const on1x::Error& error){rejected=std::string(error.what()).find("argument")!=std::string::npos;}
   assert(rejected);
 }

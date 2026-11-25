#include <on1x/loader.hpp>
#include <on1x/clone.hpp>
#include <on1x/stdffi.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
int main(int argc,char** argv){
 assert(argc==2);namespace fs=std::filesystem;
 fs::path root=argv[1];fs::create_directories(root);
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

#include <on1x/loader.hpp>
#include <on1x/clone.hpp>
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
}

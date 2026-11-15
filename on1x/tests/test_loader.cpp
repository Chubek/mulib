#include <on1x/loader.hpp>
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
}

#include "wire.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>

namespace {
void usage(){std::cerr<<"usage: on1x-vm [--verify] [--steps N] [--depth N] FILE.o1vm\n";}
// Never echo terminal controls supplied by an artifact.
std::string display(std::string_view s){
 std::string out;for(unsigned char c:s){if(out.size()>=512){out+="...";break;}
  out+=c<32||c>=127?'?':static_cast<char>(c);}return out;
}
}
int main(int argc,char** argv){
 bool only_verify=false;std::string path;std::size_t steps=1000000,depth=256;
 for(int i=1;i<argc;++i){std::string_view arg=argv[i];
  if(arg=="--verify"){only_verify=true;continue;}
  if(arg=="--steps"||arg=="--depth"){
   if(++i==argc){usage();return 2;}std::string_view number=argv[i];
   auto [p,ec]=std::from_chars(number.data(),number.data()+number.size(),arg=="--depth"?depth:steps);
   if(ec!=std::errc{}||p!=number.data()+number.size()){usage();return 2;}continue;
  }
  if(arg=="--help"){usage();return 0;}
  if(arg.starts_with('-')||!path.empty()){usage();return 2;}path=arg;
 }
 if(path.empty()){usage();return 2;}
 try{
  std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("cannot open VM artifact");
  std::array<char,8192> buf;std::string bytes;
  while(file){file.read(buf.data(),buf.size());auto n=static_cast<std::size_t>(file.gcount());
   if(n>on1x::vm::max_artifact_bytes-bytes.size())throw std::runtime_error("VM artifact exceeds byte limit");
   bytes.append(buf.data(),n);}
  if(!file.eof())throw std::runtime_error("error reading VM artifact");
  if(only_verify){on1x::vm::verify(bytes);return 0;}
  auto r=on1x::vm::run_main(bytes,steps,depth);
  std::cout<<r.output;
  if(r.status==on1x::mir::Status::Returned)return static_cast<unsigned char>(r.value);
  std::cerr<<display(r.where.file)<<':'<<r.where.line<<':'<<r.where.column<<": ON1X-VM: "<<display(r.diagnostic)<<'\n';
  return r.status==on1x::mir::Status::Trapped?70:75;
 }catch(const on1x::Error& e){
  std::cerr<<display(e.where.file)<<':'<<e.where.line<<':'<<e.where.column<<": "<<display(e.what())<<'\n';
 }catch(const std::exception& e){std::cerr<<display(e.what())<<'\n';}
 return 1;
}

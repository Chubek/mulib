/* On1x binding bootstrap from ExoML's versioned CDecl and Contract IRs.
 * The output is a RAW/UNSAFE On1x import plus C11 scalar adapters. It does
 * not assert ownership or effect safety. No pointer or aggregate conversion.
 */
#include <exoml/CDecl.hpp>
#include <exoml/Contract.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using moonstone::exoml::cdecl::TranslationUnit;
using moonstone::exoml::cdecl::Type;
using moonstone::exoml::cdecl::TypeKind;
using moonstone::exoml::cdecl::Declaration;
using moonstone::exoml::contract::FunctionContract;

std::string read(const char* path){std::ifstream stream(path,std::ios::binary);
 if(!stream)throw std::runtime_error(std::string("cannot open ")+path);
 std::ostringstream os;os<<stream.rdbuf();return os.str();}
void write(const std::string& path,const std::string& value){std::ofstream stream(path,std::ios::binary);
 if(!stream||!(stream<<value))throw std::runtime_error("cannot write "+path);}
const Type* lookup(const TranslationUnit& u,const std::string& key){
 auto it=u.types.find(key);return it==u.types.end()?nullptr:&it->second;
}
const Type* primitive(const TranslationUnit& u,const std::string& key){
 const Type *t=lookup(u,key);
 for(int i=0;t&&i<32;++i){
  if(t->kind!=TypeKind::Typedef && t->kind!=TypeKind::Elaborated)return t;
  if(t->element.empty())return nullptr;
  t=lookup(u,t->element);
 }return nullptr;
}
struct Scalar {std::string spelling;unsigned bits=0;bool is_void=false;};
std::optional<Scalar> scalar(const TranslationUnit&u,const std::string& id){
 const Type *native=lookup(u,id),*base=primitive(u,id);
 if(!native||!base)return std::nullopt;
 const auto spelling=native->spelling;
 if(!std::regex_match(spelling,std::regex(R"([A-Za-z_][A-Za-z_0-9]*( [A-Za-z_][A-Za-z_0-9]*)*)")))return std::nullopt;
 if(base->kind==TypeKind::Void)return Scalar{spelling,0,true};
 if(base->kind!=TypeKind::Integer||!base->is_signed||!*base->is_signed||!base->size)return std::nullopt;
 unsigned bits=static_cast<unsigned>(*base->size*8);
 if(bits!=8&&bits!=16&&bits!=32&&bits!=64)return std::nullopt;
 return Scalar{spelling,bits,false};
}
struct Emission {std::string language;std::string native;unsigned generated=0,skipped=0;};
Emission emit(const TranslationUnit&u,const moonstone::exoml::contract::ContractBundle& bundle){
 const std::string header=std::filesystem::path(u.header).filename().string();
 if(!std::regex_match(header,std::regex(R"([A-Za-z_][A-Za-z_0-9.\-]*)")))
  throw std::runtime_error("invalid C header basename");
 std::map<std::string,const Declaration*> declarations;
 for(auto&d:u.declarations)if(d.kind==moonstone::exoml::cdecl::DeclKind::Function)declarations.emplace(d.name,&d);
 Emission e;
 e.language="# Auto-generated RAW On1x C ABI import. No safe ownership/effect facade.\n";
 e.native="/* Auto-generated RAW C11 signed-integer adapters, no ownership guarantees. */\n"
          "#include <stdint.h>\n#include <limits.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <"+header+">\n"
          "_Static_assert(CHAR_BIT == 8, \"On1x bridge requires 8-bit C bytes\");\n"
          "static inline void on1x_bind_range_error(void){fputs(\"On1x foreign integer out of range\\n\",stderr);exit(72);}\n";
 std::set<std::string> seen;
 for(const FunctionContract&fn:bundle.functions){
  if(!seen.insert(fn.c_name).second)throw std::runtime_error("duplicate function in contract");
  const auto hit=declarations.find(fn.c_name);
  if(hit==declarations.end())throw std::runtime_error("contract function missing");
  const Declaration&decl=*hit->second;
  const Type *ft=lookup(u,decl.type);
  if(!ft||ft->kind!=TypeKind::Function||ft->variadic||!ft->prototype||ft->arguments.size()!=decl.parameters.size()||decl.parameters.size()>12){++e.skipped;continue;}
  if(fn.parameters.size()!=decl.parameters.size()||fn.effects.unknown||!fn.transfers.empty()||!fn.callbacks.empty()||!fn.destructors.empty()||!fn.invalidations.empty()){
   ++e.skipped;continue;
  }
  if(fn.return_contract.kind!=moonstone::exoml::contract::ReturnKind::Value||
     fn.error_contract.kind!=moonstone::exoml::contract::ErrorKind::None){++e.skipped;continue;}
  auto res=scalar(u,ft->result);if(!res){++e.skipped;continue;}
  std::vector<Scalar> args;bool valid=true;
  for(std::size_t i=0;i<decl.parameters.size();++i){
   auto p=scalar(u,decl.parameters[i].type);
   if(!p||p->is_void||fn.parameters[i].ownership!=moonstone::exoml::contract::OwnershipKind::Scalar){valid=false;break;}
   args.push_back(*p);
  }
  if(!valid||!std::regex_match(decl.name,std::regex(R"([A-Za-z_][A-Za-z_0-9]*)"))){++e.skipped;continue;}
  const auto exported="on1x_ffi_"+decl.name;
  const std::string result=res->is_void?"void":"i64";
  const std::string native_ret=res->is_void?"void":"int64_t";
  e.language+="extern fn "+exported+"(";
  e.native+=native_ret+" "+exported+"(";
  for(std::size_t i=0;i<args.size();++i){
   if(i){e.language+=", ";e.native+=", ";}
   e.language+="a"+std::to_string(i)+": i64";
   e.native+="int64_t a"+std::to_string(i);
  }
  if(args.empty())e.native+="void";
  e.language+=") -> "+result+";\n";
  e.native+=") {\n";
  // The type selected at C compilation must be compatible with the type
  // discovered and reviewed by ExoML; a cast alone would hide ABI drift.
  e.native+="  _Static_assert(_Generic(&"+decl.name+", "+res->spelling+" (*) (";
  for(std::size_t i=0;i<args.size();++i){
   if(i)e.native+=", ";
   e.native+=args[i].spelling;
  }
  if(args.empty())e.native+="void";
  e.native+="): 1, default: 0), \"native signature drift\");\n";
  // Check sizes on the actual compilation target; target drift is not silent.
  if(!res->is_void)e.native+="  _Static_assert(sizeof("+res->spelling+") == "+std::to_string(res->bits/8)+", \"result ABI drift\");\n";
  for(std::size_t i=0;i<args.size();++i){
   const auto& a=args[i];const auto name="a"+std::to_string(i);
   e.native+="  _Static_assert(sizeof("+a.spelling+") == "+std::to_string(a.bits/8)+", \"parameter ABI drift\");\n";
   if(a.bits<64)e.native+="  if("+name+" < INT"+std::to_string(a.bits)+"_MIN || "+name+" > INT"+std::to_string(a.bits)+"_MAX) on1x_bind_range_error();\n";
  }
  e.native+="  ";if(!res->is_void)e.native+="return (int64_t)";
  e.native+=decl.name+"(";
  for(std::size_t i=0;i<args.size();++i){if(i)e.native+=", ";
   e.native+="("+args[i].spelling+")a"+std::to_string(i);
  }
  e.native+=");\n}\n";
  ++e.generated;
 }
 return e;
}
}
int main(int argc,char**argv){
 if(argc!=4 && argc!=5){
  std::cerr<<"usage: on1x-bind verify DECL.json CONTRACT.json\n"
              "       on1x-bind emit DECL.json CONTRACT.json OUT_PREFIX\n";return 2;}
 const std::string mode=argv[1];if((mode=="verify"&&argc!=4)||(mode=="emit"&&argc!=5))return 2;
 if(mode!="verify"&&mode!="emit")return 2;
 try{
  using namespace moonstone::exoml;
  const auto unit=cdecl::parse_translation_unit_json(read(argv[2]));
  const auto bundle=contract::parse_contract_sidecar(read(argv[3]));
  if(!unit.complete){std::cerr<<"On1x: incomplete C discovery cannot be imported\n";return 3;}
  if(unit.fingerprint.empty()) {std::cerr<<"On1x: missing compiler fingerprint\n";return 3;}
  if(!bundle.bundle_provenance.revision.empty() && bundle.bundle_provenance.revision!=unit.revision){
   std::cerr<<"On1x: bundle revision does not match discovered revision\n";return 3;}
  if(!bundle.bundle_provenance.toolchain_fingerprint.empty() &&
     bundle.bundle_provenance.toolchain_fingerprint!=unit.fingerprint){
   std::cerr<<"On1x: toolchain fingerprint differs\n";return 3;}
  for(const auto& function:bundle.functions){
   if(!function.provenance.revision.empty() && function.provenance.revision!=unit.revision){
    std::cerr<<"On1x: function contract revision drift: "<<function.c_name<<"\n";return 3;
   }
   if(!function.provenance.toolchain_fingerprint.empty() &&
      function.provenance.toolchain_fingerprint!=unit.fingerprint){
    std::cerr<<"On1x: function contract compiler fingerprint drift: "<<function.c_name<<"\n";return 3;
   }
  }
  auto diagnostics=contract::validate_contract(bundle,unit);
  for(const auto& d:diagnostics)std::cerr<<moonstone::format(d)<<"\n";
  if(!diagnostics.empty())return 3;
  if(!contract::is_safe_for_export(bundle)){
   std::cerr<<"On1x: Gate B passed but review gate is not satisfied\n";return 3;
  }
  if(mode=="verify"){
   std::cout<<"ExoML declarations/contract gate passed for "<<bundle.functions.size()<<" functions.\n"
            <<"This is not a proof that C implementation semantics are safe.\n";return 0;
  }
  auto out=emit(unit,bundle);
  if(!out.generated){std::cerr<<"On1x: no supported signed-integer scalar ABI functions ("<<out.skipped<<" skipped)\n";return 3;}
  const std::string prefix=argv[4];
  write(prefix+".on1x",out.language);
  write(prefix+".c",out.native);
  std::cout<<"Emitted "<<out.generated<<" raw scalar adapter(s); skipped "<<out.skipped
           <<" unsupported signatures. Generated code still requires C11 compile/ABI checks.\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"on1x-bind: "<<e.what()<<"\n";return 2;}
}

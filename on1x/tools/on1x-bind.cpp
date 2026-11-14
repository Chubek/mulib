/* Build-time CDecl/semantic-contract tooling. Public ExoML IRs remain canonical.
 * Gate B and review metadata are NOT foreign semantic safety proofs. */
#include "contract_cdecl.hpp"
#ifdef ON1X_EXOML_EXTRACT
#include <exoml/Extract.hpp>
#endif
#include <fstream>
#include <iostream>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
namespace {
namespace cx=on1x::contracts;
namespace json=cx::json;
namespace cdecl=cx::cdecl;
namespace contract=cx::contract;
using cdecl::TranslationUnit;using cdecl::Type;using cdecl::TypeKind;
using cdecl::Declaration;using contract::FunctionContract;
std::string read(const std::string& path,std::size_t limit){
 cx::require(std::filesystem::is_regular_file(path),"input is not a regular file: "+path);
 std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("cannot open "+path);
 std::string text;char buffer[8192];
 while(stream){stream.read(buffer,sizeof buffer);auto n=static_cast<std::size_t>(stream.gcount());cx::require(n<=limit-text.size(),"input byte limit exceeded: "+path);text.append(buffer,n);}
 if(!stream.eof())throw std::runtime_error("cannot read "+path);
 return text;
}
void write(const std::string& path,const std::string& value){
 // Exclusive creation protects existing files, hard links and symlinks, even
 // if a competing build creates one after the caller's inventory check.
 int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
 cx::require(fd>=0,"cannot create new output (existing files are never replaced): "+path);
 bool okay=true;
 for(std::size_t offset=0;offset<value.size();){auto n=::write(fd,value.data()+offset,value.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0){okay=false;break;}offset+=static_cast<std::size_t>(n);}
 if(::close(fd)!=0)okay=false;
 if(!okay){std::filesystem::remove(path);throw std::runtime_error("cannot write "+path);}
}
const Type* lookup(const TranslationUnit& unit,const std::string& id){auto found=unit.types.find(id);return found==unit.types.end()?nullptr:&found->second;}
const Type* primitive(const TranslationUnit& unit,const std::string& id){
 const Type* type=lookup(unit,id);std::set<std::string> visited;
 while(type&&(type->kind==TypeKind::Typedef||type->kind==TypeKind::Elaborated)){
  if(visited.size()>=64||!visited.insert(type->id).second)return nullptr;
  type=lookup(unit,type->element);
 }return type;
}
bool letter(unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';}
bool digit(unsigned char c){return c>='0'&&c<='9';}
bool identifier(std::string_view value){
 if(value.empty()||value.size()>256||!letter(static_cast<unsigned char>(value[0])))return false;
 for(unsigned char c:value)if(!letter(c)&&!digit(c))return false;
 return true;
}
bool spelling(std::string_view value){
 if(value.empty()||value.size()>256)return false;
 for(std::size_t begin=0;begin<value.size();){auto end=value.find(' ',begin);if(end==std::string_view::npos)end=value.size();if(!identifier(value.substr(begin,end-begin)))return false;begin=end+1;if(begin==value.size())return false;}
 return true;
}
struct Scalar {std::string spelling;unsigned bits=0;bool is_void=false;};
std::optional<Scalar> scalar(const TranslationUnit& unit,const std::string& id){
 const auto* named=lookup(unit,id);const auto* base=primitive(unit,id);
 if(!named||!base||!named->unsupported_reason.empty()||!base->unsupported_reason.empty()||!named->attributes.empty()||!base->attributes.empty()||!spelling(named->spelling))return {};
 if(base->kind==TypeKind::Void)return Scalar{named->spelling,0,true};
 if(base->kind!=TypeKind::Integer||!base->is_signed||!*base->is_signed||!base->size||(*base->size!=1&&*base->size!=2&&*base->size!=4&&*base->size!=8))return {};
 return Scalar{named->spelling,static_cast<unsigned>(*base->size*8),false};
}
std::string unsupported(const TranslationUnit& unit,const Declaration& decl,const FunctionContract& fn){
 const auto* type=lookup(unit,decl.type);
 if(!type||type->kind!=TypeKind::Function||!type->prototype||type->variadic||type->arguments.size()!=decl.parameters.size())return "requires a complete non-variadic C prototype";
 if(type->calling_convention>1||!type->attributes.empty()||!type->unsupported_reason.empty()||!decl.unsupported_reason.empty()||decl.parameters.size()>12)return "unsupported calling convention/attributes/arity";
 if(fn.effects.unknown)return "unknown effects (including grammar callbacks)";
 if(!fn.transfers.empty()||!fn.callbacks.empty()||!fn.destructors.empty()||!fn.invalidations.empty())return "ownership/callback/destructor/invalidation obligations need a reviewed adapter";
 if(fn.return_contract.kind!=contract::ReturnKind::Value||fn.return_contract.nullable||fn.return_contract.lifetime||!fn.return_contract.destroy_with.empty()||!fn.return_contract.alias_of.empty()||!fn.return_contract.allocation_source.empty()||!fn.return_contract.deallocator_family.empty())return "nontrivial return ownership/lifetime";
 if(fn.error_contract.kind!=contract::ErrorKind::None||fn.error_contract.cleanup_required_on_failure||!fn.error_contract.sentinel_value.empty()||!fn.error_contract.error_parameter.empty())return "error classification/cleanup is not lowered";
 if(!scalar(unit,type->result))return "unsupported return ABI (only signed integers/void)";
 for(std::size_t i=0;i<decl.parameters.size();++i){
  auto arg=scalar(unit,decl.parameters[i].type);if(!arg||arg->is_void)return "unsupported parameter ABI (pointers/aggregates/callbacks are not scalars)";
  auto p=std::find_if(fn.parameters.begin(),fn.parameters.end(),[&](const auto& p){return p.index==i;});
  if(p==fn.parameters.end()||p->ownership!=contract::OwnershipKind::Scalar||p->nullable||p->lifetime||p->length_parameter||p->count_parameter||p->alignment||p->transfer_branch||!p->transfer_predicate.empty()||!p->encoding.empty())return "nontrivial parameter contract";
 }
 if(!identifier(decl.name))return "invalid/oversized C function name";
 return {};
}
std::string report(const TranslationUnit& unit,const contract::ContractBundle& bundle){
 auto declarations=cx::declarations(unit);std::string out="{\"schema\":\"on1x.contract-check/v1\",\"gate_b\":true,\"review_metadata\":";
 out+=contract::is_safe_for_export(bundle)?"true":"false";
 out+=",\"safe_export\":false,\"missing_safe_gates\":[\"independent semantic review\",\"ownership/effect lowering\",\"Chisel/Wetstone usage checks\",\"target adapter/runtime acceptance\"],\"library\":"+json::quote(bundle.library)+",\"fingerprint\":"+json::quote(unit.fingerprint)+",\"functions\":[";
 bool first=true;for(const auto& fn:bundle.functions){if(!first)out+=',';first=false;auto why=unsupported(unit,*declarations.at(fn.c_name),fn);out+="{\"name\":"+json::quote(fn.c_name)+",\"raw_scalar_abi_supported\":"+(why.empty()?"true":"false")+",\"reason\":"+json::quote(why)+"}";}
 return out+"]}\n";
}
std::string identity(const TranslationUnit& unit){
 std::string out="{\"header\":"+json::quote(unit.header)+",\"revision\":"+json::quote(unit.revision)+",\"fingerprint\":"+json::quote(unit.fingerprint)+",\"target\":{\"compiler\":"+json::quote(unit.target.compiler)+",\"triple\":"+json::quote(unit.target.triple)+",\"pointer_bits\":"+std::to_string(unit.target.pointer_bits)+"},\"arguments\":[";
 for(std::size_t i=0;i<unit.arguments.size();++i){if(i)out+=',';out+=json::quote(unit.arguments[i]);}out+="],\"inputs\":[";
 for(std::size_t i=0;i<unit.inputs.size();++i){if(i)out+=',';out+="{\"path\":"+json::quote(unit.inputs[i].path)+",\"sha256\":"+json::quote(unit.inputs[i].sha256)+"}";}
 return out+"]}";
}
struct Emission {std::string language,native;unsigned generated=0,skipped=0;};
Emission emit(const TranslationUnit& unit,const contract::ContractBundle& bundle){
 auto header=std::filesystem::path(unit.header).filename().string();bool valid=!header.empty()&&header.size()<=256&&letter(static_cast<unsigned char>(header[0]));
 for(unsigned char c:header)valid=valid&&(letter(c)||digit(c)||c=='.'||c=='-');
 cx::require(valid,"invalid/oversized C header basename");
 auto declarations=cx::declarations(unit);Emission out;
 out.language="# Auto-generated RAW/UNSAFE On1x scalar import. Not a safe facade.\n";
 out.native="/* RAW/UNSAFE scalar ABI adapters. Rebuild with the recorded inputs. */\n#include <stdint.h>\n#include <limits.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <"+header+">\n_Static_assert(CHAR_BIT == 8, \"On1x requires 8-bit bytes\");\nstatic inline void on1x_bind_range_error(void){fputs(\"On1x foreign integer out of range\\n\",stderr);exit(72);}\n";
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);if(!unsupported(unit,decl,fn).empty()){++out.skipped;continue;}
  const auto& type=*lookup(unit,decl.type);auto result=*scalar(unit,type.result);std::vector<Scalar> args;for(const auto& p:decl.parameters)args.push_back(*scalar(unit,p.type));
  auto name="on1x_ffi_"+decl.name;out.language+="extern fn "+name+"(";out.native+=(result.is_void?"void ":"int64_t ")+name+"(";
  for(std::size_t i=0;i<args.size();++i){if(i){out.language+=", ";out.native+=", ";}out.language+="a"+std::to_string(i)+": i64";out.native+="int64_t a"+std::to_string(i);}
  if(args.empty())out.native+="void";
  out.language+=") -> "+std::string(result.is_void?"void":"i64")+";\n";out.native+=") {\n  _Static_assert(_Generic(&"+decl.name+", "+result.spelling+" (*)(";
  for(std::size_t i=0;i<args.size();++i){if(i)out.native+=", ";out.native+=args[i].spelling;}if(args.empty())out.native+="void";
  out.native+="): 1, default: 0), \"native signature drift\");\n";
  auto abi=[&](const Scalar& scalar){auto name="INT"+std::to_string(scalar.bits);return "  _Static_assert(sizeof("+scalar.spelling+") == "+std::to_string(scalar.bits/8)+", \"integer size drift\");\n  _Static_assert(("+scalar.spelling+")-1 < ("+scalar.spelling+")0, \"integer signedness drift\");\n  _Static_assert(("+scalar.spelling+")"+name+"_MIN == "+name+"_MIN && ("+scalar.spelling+")"+name+"_MAX == "+name+"_MAX, \"integer range drift\");\n";};
  if(!result.is_void)out.native+=abi(result);
  for(std::size_t i=0;i<args.size();++i){out.native+=abi(args[i]);if(args[i].bits<64){auto arg="a"+std::to_string(i);out.native+="  if("+arg+" < INT"+std::to_string(args[i].bits)+"_MIN || "+arg+" > INT"+std::to_string(args[i].bits)+"_MAX) on1x_bind_range_error();\n";}}
  out.native+="  ";if(!result.is_void)out.native+="return (int64_t)";out.native+=decl.name+"(";
  for(std::size_t i=0;i<args.size();++i){if(i)out.native+=", ";out.native+="("+args[i].spelling+")a"+std::to_string(i);}out.native+=");\n}\n";++out.generated;
 }return out;
}
void usage(){std::cout<<"on1x-bind check|explain|pin|verify DECL.json CONTRACT.json\n"
 "on1x-bind normalize-decl DECL.json\n"
 "on1x-bind emit-raw|emit DECL.json CONTRACT.json OUT_PREFIX\n"
 "on1x-bind emit-safe DECL.json CONTRACT.json (fails until all safe gates exist)\n"
 "on1x-bind extract HEADER --revision REV [--include DIR] [--define NAME=VALUE]\n"
 "  [--undefine NAME] [--target TRIPLE] [--resource-dir DIR] [--provenance FILE]\n"
 "  [--compile-commands FILE --compile-file FILE] [--clang-arg ARG] [-o FILE]\n";}
int extract(int argc,char** argv){
#ifdef ON1X_EXOML_EXTRACT
 cx::require(argc>=3,"extract requires a header");cdecl::ExtractOptions options;options.header=argv[2];std::string output;
 for(int i=3;i<argc;++i){std::string flag=argv[i];cx::require(i+1<argc,"missing extract option value: "+flag);std::string value=argv[++i];cx::require(value.size()<=4096,"extract option exceeds 4096 bytes");
  if(flag=="--include")options.include_directories.push_back(value);else if(flag=="--define")options.defines.push_back(value);else if(flag=="--undefine")options.undefines.push_back(value);
  else if(flag=="--target")options.target=value;else if(flag=="--revision")options.revision=value;else if(flag=="--resource-dir")options.resource_directory=value;else if(flag=="--provenance")options.provenance_files.push_back(value);
  else if(flag=="--compile-commands")options.compile_commands=value;else if(flag=="--compile-file")options.compile_file=value;else if(flag=="--clang-arg")options.compiler_arguments.push_back(value);else if(flag=="-o")output=value;else throw cx::Rejected("unknown extract option: "+flag);
 }
 cx::require(!options.revision.empty(),"extract requires an explicit source revision");
 auto unit=cdecl::extract_c11(options);for(const auto& d:unit.diagnostics)std::cerr<<moonstone::format(d)<<"\n";cx::evidence(unit);
 auto data=cdecl::describe_translation_unit(unit);if(output.empty())std::cout<<data;else write(output,data);return 0;
#else
 (void)argc;(void)argv;throw cx::Rejected("extract unavailable: configure ON1X_WITH_EXOML_EXTRACT=ON (libclang developer dependency only)");
#endif
}
}
int main(int argc,char** argv){
 try{
  if(argc==2&&std::string(argv[1])=="--help"){usage();return 0;}
  if(argc<2||argc>4096){usage();return 2;}
  for(int i=1;i<argc;++i)cx::require(std::char_traits<char>::length(argv[i])<=4096,"argument exceeds 4096 bytes");
  std::string mode=argv[1];if(mode=="extract")return extract(argc,argv);
  if(mode=="normalize-decl"){cx::require(argc==3,"normalize-decl requires one CDecl artifact");auto unit=cx::parse_cdecl(read(argv[2],64*1024*1024));cx::evidence(unit);std::cout<<cdecl::describe_translation_unit(unit);return 0;}
  bool emitting=mode=="emit"||mode=="emit-raw";
  if((emitting&&argc!=5)||(!emitting&&argc!=4)){usage();return 2;}
  if(mode!="check"&&mode!="explain"&&mode!="pin"&&mode!="verify"&&mode!="emit-safe"&&!emitting){usage();return 2;}
  auto declaration=read(argv[2],64*1024*1024);auto unit=cx::parse_cdecl(declaration);
  auto annotation=read(argv[3],2*1024*1024);cx::validate_shape(json::parse(annotation));auto bundle=contract::parse_contract_sidecar(annotation);
  if(mode=="pin"){std::cout<<contract::serialize_contract(cx::pin(unit,std::move(bundle)));return 0;}
  cx::validate(unit,bundle,true);
  if(mode=="emit-safe")throw cx::Rejected("safe export unavailable: independent semantic review, ownership/effect lowering, Chisel/Wetstone usage checks and target runtime acceptance are required");
  if(mode=="check"||mode=="explain"){std::cout<<report(unit,bundle);return 0;}
  cx::require(contract::is_safe_for_export(bundle),"Gate B passed, but review metadata gate is not satisfied; no safe-export claim follows from either gate");
  if(mode=="verify"){std::cout<<"ExoML Gate B and review metadata passed for "<<bundle.functions.size()<<" functions; not a C semantic safety proof.\n";return 0;}
  auto output=emit(unit,bundle);cx::require(output.generated>0,"no supported signed-integer scalar ABI functions ("+std::to_string(output.skipped)+" skipped)");
  std::string prefix=argv[4];std::vector<std::pair<std::string,std::string>> files={{prefix+".on1x",output.language},{prefix+".c",output.native},{prefix+".manifest.json","{\"schema\":\"on1x.raw-ffi/v1\",\"safe_export\":false,\"identity\":"+identity(unit)+",\"contracts\":"+contract::serialize_contract(bundle)+",\"eligibility\":"+report(unit,bundle)+"}\n"}};
  for(const auto& [path,contents]:files){(void)contents;cx::require(!std::filesystem::exists(path)&&!std::filesystem::is_symlink(path),"output already exists: "+path);}
  std::vector<std::string> written;
  try{for(const auto& [path,contents]:files){write(path,contents);written.push_back(path);}}
  catch(...){for(const auto& path:written){std::error_code ignored;std::filesystem::remove(path,ignored);}throw;}
  std::cout<<"Emitted "<<output.generated<<" RAW/UNSAFE scalar adapters; "<<output.skipped<<" unsupported. C11 ABI checks and all recorded inputs are still required.\n";return 0;
 }catch(const cx::Rejected& e){std::cerr<<"on1x-bind: "<<e.what()<<"\n";return 3;}
 catch(const std::exception& e){std::cerr<<"on1x-bind: "<<e.what()<<"\n";return 2;}
}

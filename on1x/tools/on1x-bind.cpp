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
#include <set>
#include <sys/wait.h>
#include <sys/resource.h>
#include <csignal>
#include <ctime>
#include <chrono>
#include <cstdint>
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
struct Scalar {
 enum Kind { SignedInteger, UnsignedInteger, Boolean, Binary32, Binary64, Void, CString } kind=Void;
 std::string spelling;unsigned bits=0;std::uint64_t alignment=0;
 std::string on1x()const{return kind==CString?"string":kind==SignedInteger?"i64":kind==UnsignedInteger?"u"+std::to_string(bits):kind==Boolean?"bool":kind==Binary32?"f32":kind==Binary64?"f64":"void";}
 std::string adapter()const{return kind==CString?"const char *":kind==SignedInteger?"int64_t":kind==UnsignedInteger?"uint"+std::to_string(bits)+"_t":kind==Boolean?"bool":kind==Binary32?"float":kind==Binary64?"double":"void";}
 std::string admission(bool result)const{return kind==CString?(result?"static_string":"borrowed_string"):on1x();}
 bool floating()const{return kind==Binary32||kind==Binary64;}
};
std::optional<Scalar> scalar(const TranslationUnit& unit,const std::string& id){
 const auto* named=lookup(unit,id);const auto* base=primitive(unit,id);
 if(!named||!base||!named->unsupported_reason.empty()||!base->unsupported_reason.empty()||!named->attributes.empty()||!base->attributes.empty()||!spelling(named->spelling))return {};
 if(base->kind==TypeKind::Void)return Scalar{Scalar::Void,named->spelling};
 if(!base->size||!named->alignment||!*named->alignment)return {};
 auto alignment=*named->alignment;
 if(base->kind==TypeKind::Integer&&(base->spelling=="_Bool"||base->canonical_spelling=="_Bool"))
  return *base->size==1?std::optional<Scalar>{{Scalar::Boolean,named->spelling,8,alignment}}:std::nullopt;
 if(base->kind==TypeKind::Floating){
  if(*base->size==4&&(base->spelling=="float"||base->canonical_spelling=="float"))return Scalar{Scalar::Binary32,named->spelling,32,alignment};
  if(*base->size==8&&(base->spelling=="double"||base->canonical_spelling=="double"))return Scalar{Scalar::Binary64,named->spelling,64,alignment};
  return {};
 }
 if(base->kind!=TypeKind::Integer||!base->is_signed||
    (*base->size!=1&&*base->size!=2&&*base->size!=4&&*base->size!=8))return {};
 return Scalar{*base->is_signed?Scalar::SignedInteger:Scalar::UnsignedInteger,named->spelling,static_cast<unsigned>(*base->size*8),alignment};
}
// The string profile deliberately admits only an extracted direct pointer to
// const plain char. No void*, signed/unsigned char*, wide text or alias guessing.
std::optional<Scalar> foreign_type(const TranslationUnit& unit,const std::string& id,bool strings=true){
 if(auto s=scalar(unit,id))return s;
 if(!strings)return {};
 const auto* pointer=lookup(unit,id);
 if(!pointer||pointer->kind!=TypeKind::Pointer||!pointer->unsupported_reason.empty()||!pointer->attributes.empty()||
    pointer->qualifiers.is_volatile||pointer->qualifiers.is_restrict||!pointer->size||!pointer->alignment||!*pointer->alignment||
    unit.target.pointer_bits%8||*pointer->size!=unit.target.pointer_bits/8)return {};
 const auto* element=lookup(unit,pointer->element);
 if(!element||element->kind!=TypeKind::Integer||!element->qualifiers.is_const||element->qualifiers.is_volatile||
    element->qualifiers.is_restrict||!element->attributes.empty()||!element->unsupported_reason.empty()||
    element->spelling!="const char"||element->size!=1)return {};
 return Scalar{Scalar::CString,"const char *",unit.target.pointer_bits,*pointer->alignment};
}
bool lifetime(const std::optional<contract::LifetimeSpec>& value,contract::LifetimeOrigin origin,const std::string& region={}){
 return value&&value->origin==origin&&value->region_name==region&&value->parameter_index==0;
}
std::string unsupported(const TranslationUnit& unit,const Declaration& decl,const FunctionContract& fn,bool strings=false){
 const auto* type=lookup(unit,decl.type);
 if(!type||type->kind!=TypeKind::Function||!type->prototype||type->variadic||type->arguments.size()!=decl.parameters.size())return "requires a complete non-variadic C prototype";
 if(type->calling_convention>1||!type->attributes.empty()||!type->unsupported_reason.empty()||!decl.unsupported_reason.empty()||decl.parameters.size()>12)return "unsupported calling convention/attributes/arity";
 if(fn.effects.unknown)return "unknown effects (including grammar callbacks)";
 if(!fn.transfers.empty()||!fn.callbacks.empty()||!fn.destructors.empty()||!fn.invalidations.empty())return "ownership/callback/destructor/invalidation obligations need a reviewed adapter";
 auto result=foreign_type(unit,type->result,strings);
 const auto& ret=fn.return_contract;
 bool static_text=result&&result->kind==Scalar::CString;
 if(ret.nullable||ret.nullable_means_success||!ret.destroy_with.empty()||!ret.alias_of.empty()||!ret.allocation_source.empty()||!ret.deallocator_family.empty()||
    (static_text?(ret.kind!=contract::ReturnKind::Borrow||!lifetime(ret.lifetime,contract::LifetimeOrigin::GlobalStatic)):
                 (ret.kind!=contract::ReturnKind::Value||ret.lifetime)))return "nontrivial return ownership/lifetime (string results require non-null global_static borrow)";
 if(fn.error_contract.kind!=contract::ErrorKind::None||fn.error_contract.cleanup_required_on_failure||!fn.error_contract.sentinel_value.empty()||!fn.error_contract.error_parameter.empty())return "error classification/cleanup is not lowered";
 if(!result)return "unsupported return ABI (only exact scalars or reviewed immutable C strings)";
 bool text=static_text;
 for(std::size_t i=0;i<decl.parameters.size();++i){
  auto arg=foreign_type(unit,decl.parameters[i].type,strings);if(!arg||arg->kind==Scalar::Void)return "unsupported parameter ABI (pointers/aggregates/callbacks are not scalars)";
  auto p=std::find_if(fn.parameters.begin(),fn.parameters.end(),[&](const auto& p){return p.index==i;});
  if(p==fn.parameters.end()||p->nullable||p->length_parameter||p->count_parameter||p->alignment||p->transfer_branch||!p->transfer_predicate.empty())return "nontrivial parameter contract";
  if(arg->kind==Scalar::CString){
   text=true;
   if(p->ownership!=contract::OwnershipKind::BorrowedImmutable||p->encoding!="nul_terminated_bytes"||
      !lifetime(p->lifetime,contract::LifetimeOrigin::ExplicitRegion,"call"))return "string parameter requires immutable call-scoped nul_terminated_bytes borrow";
  }else if(p->ownership!=contract::OwnershipKind::Scalar||p->lifetime||!p->encoding.empty())return "nontrivial parameter contract";
 }
 if(text&&(fn.concurrency.thread_safety!=contract::ThreadSafety::SingleThread||fn.concurrency.suspension_safe))return "immutable string profile requires single_thread, non-suspending calls";
 if(!identifier(decl.name))return "invalid/oversized C function name";
 return {};
}
std::string safe_unsupported(const TranslationUnit&,const Declaration&,const FunctionContract&);
unsigned profile_version(const TranslationUnit& unit,const contract::ContractBundle& bundle){
 auto declarations=cx::declarations(unit);
 unsigned version=1;
 auto typed=[&](const std::string& id){auto s=foreign_type(unit,id);if(!s)return;
  if(s->kind==Scalar::CString)version=4;
  else if(s->kind==Scalar::Binary32||s->kind==Scalar::UnsignedInteger)version=std::max(version,3u);
  else if(s->kind==Scalar::Boolean||s->kind==Scalar::Binary64)version=std::max(version,2u);
 };
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);const auto* type=lookup(unit,decl.type);
  if(type)typed(type->result);
  for(const auto& p:decl.parameters)typed(p.type);
 }
 return version;
}
std::string profile(const TranslationUnit& unit,const contract::ContractBundle& bundle){
 auto version=profile_version(unit,bundle);
 return version==4?"reviewed-immutable-string/v4":"reviewed-scalar/v"+std::to_string(version);
}
std::string report(const TranslationUnit& unit,const contract::ContractBundle& bundle){
 auto declarations=cx::declarations(unit);std::string out="{\"schema\":\"on1x.contract-check/v1\",\"gate_b\":true,\"review_metadata\":";
 auto bundle_reviewed=bundle.bundle_provenance.review_status==contract::ReviewStatus::Reviewed||
                      bundle.bundle_provenance.review_status==contract::ReviewStatus::Verified;
 out+=bundle_reviewed&&contract::is_safe_for_export(bundle)?"true":"false";
 out+=",\"safe_export\":false,\"safe_profile\":"+json::quote(profile(unit,bundle))+",\"safe_export_reason\":\"check/explain do not execute Chisel or target runtime gates\",\"missing_safe_gates\":[\"complete reviewed profile eligibility\",\"Chisel usage check\",\"target adapter/runtime acceptance\"],\"library\":"+json::quote(bundle.library)+",\"fingerprint\":"+json::quote(unit.fingerprint)+",\"functions\":[";
 bool first=true;for(const auto& fn:bundle.functions){
  if(!first)out+=',';
  first=false;const auto& decl=*declarations.at(fn.c_name);
  auto raw_why=unsupported(unit,decl,fn),safe_why=safe_unsupported(unit,decl,fn);
  out+="{\"name\":"+json::quote(fn.c_name)+",\"raw_scalar_abi_supported\":"+(raw_why.empty()?"true":"false")+
       ",\"reason\":"+json::quote(raw_why)+",\"raw_reason\":"+json::quote(raw_why)+
       ",\"safe_scalar_profile_supported\":"+(safe_why.empty()&&raw_why.empty()?"true":"false")+
       ",\"safe_profile_supported\":"+(safe_why.empty()?"true":"false")+
       ",\"safe_reason\":"+json::quote(safe_why)+"}";
 }
 return out+"]}\n";
}
std::string identity(const TranslationUnit& unit){
 std::string out="{\"header\":"+json::quote(unit.header)+",\"revision\":"+json::quote(unit.revision)+",\"fingerprint\":"+json::quote(unit.fingerprint)+",\"target\":{\"compiler\":"+json::quote(unit.target.compiler)+",\"triple\":"+json::quote(unit.target.triple)+",\"pointer_bits\":"+std::to_string(unit.target.pointer_bits)+"},\"arguments\":[";
 for(std::size_t i=0;i<unit.arguments.size();++i){if(i)out+=',';out+=json::quote(unit.arguments[i]);}out+="],\"inputs\":[";
 for(std::size_t i=0;i<unit.inputs.size();++i){if(i)out+=',';out+="{\"path\":"+json::quote(unit.inputs[i].path)+",\"sha256\":"+json::quote(unit.inputs[i].sha256)+"}";}
 return out+"]}";
}
struct Emission {std::string language,native;unsigned generated=0,skipped=0;};
std::string adapter_name(const TranslationUnit& unit,const std::string& c_name,bool identity_scoped){
 return "on1x_ffi_"+(identity_scoped?unit.fingerprint+"_":"")+c_name;
}
Emission emit(const TranslationUnit& unit,const contract::ContractBundle& bundle,
              const std::string& language_banner,const std::string& native_banner,bool wrappers){
 auto header=std::filesystem::path(unit.header).filename().string();bool valid=!header.empty()&&header.size()<=256&&letter(static_cast<unsigned char>(header[0]));
 for(unsigned char c:header)valid=valid&&(letter(c)||digit(c)||c=='.'||c=='-');
 cx::require(valid,"invalid/oversized C header basename");
 auto declarations=cx::declarations(unit);Emission out;
 out.language=language_banner;
 out.native=native_banner+"#include <stdint.h>\n#include <stdbool.h>\n#include <float.h>\n#include <limits.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <"+header+">\n_Static_assert(CHAR_BIT == 8, \"On1x requires 8-bit bytes\");\n";
 bool range_support=false,text_support=false;
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);if(!unsupported(unit,decl,fn,wrappers).empty()){++out.skipped;continue;}
  const auto& type=*lookup(unit,decl.type);auto result=*foreign_type(unit,type.result,wrappers);std::vector<Scalar> args;for(const auto& p:decl.parameters)args.push_back(*foreign_type(unit,p.type,wrappers));
  if(!text_support&&(result.kind==Scalar::CString||std::any_of(args.begin(),args.end(),[](const auto& s){return s.kind==Scalar::CString;}))){
   out.native+="static void on1x_bind_check_text(const char *text, size_t limit){\n  if(text){for(size_t i=0;i<=limit;++i)if(text[i]=='\\0')return;}\n  fputs(\"On1x foreign string null or byte limit exceeded\\n\",stderr);exit(72);\n}\n";
   text_support=true;
  }
  if(!range_support&&std::any_of(args.begin(),args.end(),[](const auto& s){return s.kind==Scalar::SignedInteger&&s.bits<64;})){
   out.native+="static void on1x_bind_range_error(void){fputs(\"On1x foreign integer out of range\\n\",stderr);exit(72);}\n";
   range_support=true;
  }
  if(result.floating()||std::any_of(args.begin(),args.end(),[](const auto& s){return s.floating();})){
   // Match the runtime's initial floating ABI without adding a runtime dependency.
   out.native+="#if !defined(__linux__) || !defined(__x86_64__) || !defined(__SSE2__) || (!defined(__GNUC__) && !defined(__clang__)) || FLT_EVAL_METHOD != 0\n#error \"unsupported On1x floating target\"\n#endif\n";
   out.native+="#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n#error \"On1x binary64 adapters forbid relaxed IEEE math\"\n#endif\n";
  }
  auto name=adapter_name(unit,decl.name,wrappers);out.language+="extern fn "+name+"(";out.native+=result.adapter()+" "+name+"(";
  for(std::size_t i=0;i<args.size();++i){if(i){out.language+=", ";out.native+=", ";}out.language+="a"+std::to_string(i)+": "+args[i].on1x();out.native+=args[i].adapter()+" a"+std::to_string(i);}
  if(args.empty())out.native+="void";
  out.language+=") -> "+result.on1x()+";\n";out.native+=") {\n  _Static_assert(_Generic(&"+decl.name+", "+result.spelling+" (*)(";
  if(wrappers){
   auto safe="on1x_safe_"+decl.name;out.language+="fn "+safe+"(";
   for(std::size_t i=0;i<args.size();++i){if(i)out.language+=", ";out.language+="a"+std::to_string(i)+": "+args[i].on1x();}
   out.language+=") -> "+result.on1x()+" { ";
   if(result.kind==Scalar::Void)out.language+=name+"(";else out.language+="return "+name+"(";
   for(std::size_t i=0;i<args.size();++i){if(i)out.language+=", ";out.language+="a"+std::to_string(i);}
   out.language+="); }\n";
  }
  for(std::size_t i=0;i<args.size();++i){if(i)out.native+=", ";out.native+=args[i].spelling;}if(args.empty())out.native+="void";
  out.native+="): 1, default: 0), \"native signature drift\");\n";
  auto abi=[&](const Scalar& scalar){
   auto common="  _Static_assert(sizeof("+scalar.spelling+") == "+std::to_string(scalar.bits/8)+", \"scalar size drift\");\n  _Static_assert(_Alignof("+scalar.spelling+") == "+std::to_string(scalar.alignment)+", \"scalar alignment drift\");\n";
   if(scalar.kind==Scalar::CString)return common+"  _Static_assert(_Generic(("+scalar.spelling+")0, const char *: 1, default: 0), \"string pointer type drift\");\n";
   if(scalar.kind==Scalar::Boolean)return common+"  _Static_assert(_Generic(("+scalar.spelling+")0, _Bool: 1, default: 0), \"boolean type drift\");\n";
   if(scalar.kind==Scalar::Binary32)return common+"  _Static_assert(_Generic(("+scalar.spelling+")0, float: 1, default: 0), \"binary32 type drift\");\n  _Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && FLT_MIN_EXP == -125, \"binary32 format drift\");\n";
   if(scalar.kind==Scalar::Binary64)return common+"  _Static_assert(_Generic(("+scalar.spelling+")0, double: 1, default: 0), \"binary64 type drift\");\n  _Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021, \"binary64 format drift\");\n";
   if(scalar.kind==Scalar::UnsignedInteger){auto name="UINT"+std::to_string(scalar.bits);return common+
    "  _Static_assert(("+scalar.spelling+")-1 > ("+scalar.spelling+")0 && ("+scalar.spelling+")-1 == "+name+"_MAX, \"unsigned range/signedness drift\");\n";}
   auto name="INT"+std::to_string(scalar.bits);return common+"  _Static_assert(("+scalar.spelling+")-1 < ("+scalar.spelling+")0, \"integer signedness drift\");\n  _Static_assert(("+scalar.spelling+")"+name+"_MIN == "+name+"_MIN && ("+scalar.spelling+")"+name+"_MAX == "+name+"_MAX, \"integer range drift\");\n";
  };
  if(result.kind!=Scalar::Void)out.native+=abi(result);
  for(std::size_t i=0;i<args.size();++i){out.native+=abi(args[i]);if(args[i].kind==Scalar::SignedInteger&&args[i].bits<64){auto arg="a"+std::to_string(i);out.native+="  if("+arg+" < INT"+std::to_string(args[i].bits)+"_MIN || "+arg+" > INT"+std::to_string(args[i].bits)+"_MAX) on1x_bind_range_error();\n";}
   if(args[i].kind==Scalar::CString)out.native+="  on1x_bind_check_text(a"+std::to_string(i)+", 2097152u);\n";
  }
  out.native+="  ";if(result.kind==Scalar::CString)out.native+="const char *result = ";else if(result.kind!=Scalar::Void)out.native+="return ("+result.adapter()+")";out.native+=decl.name+"(";
  for(std::size_t i=0;i<args.size();++i){if(i)out.native+=", ";out.native+="("+args[i].spelling+")a"+std::to_string(i);}out.native+=");\n";
  if(result.kind==Scalar::CString)out.native+="  on1x_bind_check_text(result, 4194304u);\n  return result;\n";
  out.native+="}\n";++out.generated;
 }return out;
}
// ---- reviewed scalar/immutable-string admission (on1x.safe-ffi/v1) --------
// Gate R: reviewed contract provenance (claims, recorded as evidence).
// Gate E: every function admits under the scalar/immutable-string ABI and declares only
//         effects whose ownership/concurrency obligations need no lowering.
// Gate C: the real on1xc Chisel checker admits the emitted module with the
//         versioned reviewed-extern evidence (on1x.admitted-extern/v1/v2/v3/v4).
// Gate T: the adapter plus a boundary harness compile under strict C11 flags
//         and execute on the target; results are recorded, not interpreted.
std::string effect_name(contract::EffectKind kind){
 using K=contract::EffectKind;switch(kind){
  case K::Read:return "read";case K::Write:return "write";case K::Allocate:return "allocate";
  case K::Free:return "free";case K::Escape:return "escape";case K::IO:return "io";
  case K::Thread:return "thread";case K::Suspend:return "suspend";case K::ForeignCall:return "foreign_call";
 }return "unknown";
}
std::string safe_unsupported(const TranslationUnit& unit,const Declaration& decl,const FunctionContract& fn){
 auto raw=unsupported(unit,decl,fn,true);if(!raw.empty())return raw;
 if(adapter_name(unit,decl.name,true).size()>256)return "identity-scoped adapter name exceeds 256 bytes";
 static const std::set<contract::EffectKind> allowed{
  contract::EffectKind::Read,contract::EffectKind::Write,contract::EffectKind::IO,contract::EffectKind::ForeignCall};
 for(auto effect:fn.effects.effects)
  if(!allowed.contains(effect))
   return "declared effect '"+effect_name(effect)+"' needs ownership/concurrency lowering outside the reviewed scalar/immutable-string profiles";
 return {};
}
std::string excerpt(const std::string& text){
 auto line=text;auto end=line.find('\n');if(end!=std::string::npos)line=line.substr(0,end);
 if(line.size()>240)line=line.substr(0,240);
 return line;
}
std::string read_capped(const std::filesystem::path& path,std::size_t limit){
 std::ifstream stream(path,std::ios::binary);cx::require(static_cast<bool>(stream),"cannot read child output: "+path.string());
 std::string text;char buffer[8192];
 while(stream){stream.read(buffer,sizeof buffer);auto n=static_cast<std::size_t>(stream.gcount());
  cx::require(n<=limit-text.size(),"child output limit exceeded: "+path.string());text.append(buffer,n);}
 cx::require(stream.eof(),"cannot read child output: "+path.string());
 return text;
}
struct RunOutcome {int exit_status=-1;bool signaled=false,timed_out=false;std::string out,error;};
RunOutcome run_process(const std::string& program,const std::vector<std::string>& arguments,
                       const std::filesystem::path& scratch,std::size_t limit,unsigned seconds){
 static unsigned slot=0;
 RunOutcome result;auto tag=std::to_string(++slot);
 auto out_path=scratch/("gate"+tag+".out"),err_path=scratch/("gate"+tag+".err");
 int out_fd=::open(out_path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
 cx::require(out_fd>=0,"cannot capture child output in "+scratch.string());
 int err_fd=::open(err_path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
 if(err_fd<0)::close(out_fd);
 cx::require(err_fd>=0,"cannot capture child diagnostics in "+scratch.string());
 auto pid=::fork();
 if(pid==0){
  ::setpgid(0,0);
  rlimit file_limit{static_cast<rlim_t>(limit),static_cast<rlim_t>(limit)};
  if(::setrlimit(RLIMIT_FSIZE,&file_limit)!=0)::_exit(126);
  ::dup2(out_fd,STDOUT_FILENO);::dup2(err_fd,STDERR_FILENO);
  std::vector<char*> argv;argv.push_back(const_cast<char*>(program.c_str()));
  for(const auto& argument:arguments)argv.push_back(const_cast<char*>(argument.c_str()));
  argv.push_back(nullptr);
  execvp(program.c_str(),argv.data());::_exit(127);
 }
 ::close(out_fd);::close(err_fd);
 cx::require(pid>0,"cannot fork gate process");
 (void)::setpgid(pid,pid);
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
 int status=0;
 for(;;){
  auto waited=::waitpid(pid,&status,WNOHANG);
  if(waited<0&&errno==EINTR)continue;
  if(waited==pid)break;
  cx::require(waited>=0,"cannot wait for gate process");
  if(waited==0&&std::chrono::steady_clock::now()>=deadline){
   ::kill(-pid,SIGKILL);::kill(pid,SIGKILL);
   while(::waitpid(pid,&status,0)<0&&errno==EINTR){}
   result.timed_out=true;break;
  }
  timespec pause{0,10*1000*1000};::nanosleep(&pause,nullptr);
 }
 if(!result.timed_out){
  if(WIFEXITED(status))result.exit_status=WEXITSTATUS(status);
  else if(WIFSIGNALED(status))result.signaled=true;
  else cx::require(false,"gate process stopped instead of exiting");
 }
 result.out=read_capped(out_path,limit);result.error=read_capped(err_path,limit);
 std::error_code ignored;std::filesystem::remove(out_path,ignored);std::filesystem::remove(err_path,ignored);
 return result;
}
void emit_safe(const TranslationUnit& unit,const contract::ContractBundle& bundle,const std::string& prefix,int argc,char** argv,int first){
 std::string on1xc,cc,workdir;std::vector<std::string> cflags,impls;
 for(int i=first;i<argc;++i){
  std::string flag=argv[i];
  auto value=[&](){cx::require(i+1<argc,"missing emit-safe option value: "+flag);return std::string(argv[++i]);};
  if(flag=="--on1xc")on1xc=value();
  else if(flag=="--cc")cc=value();
  else if(flag=="--workdir")workdir=value();
  else if(flag=="--cflag")cflags.push_back(value());
  else if(flag=="--impl")impls.push_back(value());
  else throw cx::Rejected("unknown emit-safe option: "+flag);
 }
 cx::require(!on1xc.empty()&&!cc.empty()&&!workdir.empty(),
  "safe export unavailable without gate execution: pass --on1xc, --cc and --workdir to run the review/eligibility/Chisel/runtime gates; metadata-only admission does not exist");
 // Gate R: reviewed provenance across the bundle (necessity, not proof).
 auto bundle_reviewed=bundle.bundle_provenance.review_status==contract::ReviewStatus::Reviewed||
                      bundle.bundle_provenance.review_status==contract::ReviewStatus::Verified;
 cx::require(bundle_reviewed&&contract::is_safe_for_export(bundle),
  "safe export refused: review metadata gate failed; every function and the bundle must record reviewed/verified provenance");
 // Gate E: full-bundle profile eligibility; no partial safe claims.
 auto declarations=cx::declarations(unit);std::string reasons;
 for(const auto& fn:bundle.functions){
  auto why=safe_unsupported(unit,*declarations.at(fn.c_name),fn);
  if(!why.empty())reasons+="\n  "+fn.c_name+": "+why;
 }
 auto selected_profile=profile(unit,bundle);
 cx::require(reasons.empty(),"safe export refused: functions outside the reviewed "+std::string(profile_version(unit,bundle)==4?"immutable-string":"scalar-only")+" profile:"+reasons);
 auto output=emit(unit,bundle,
   "# Auto-generated SAFE-ADMITTED On1x import ("+selected_profile+"). Admission evidence accompanies this module.\n",
   "/* SAFE-ADMITTED ABI adapters ("+selected_profile+"). Rebuild with the recorded inputs. */\n",true);
 cx::require(output.generated==bundle.functions.size()&&output.skipped==0,"safe export refused: unsupported functions reached emission");
 for(const auto& suffix:{".on1x",".c",".manifest.json",".admitted-extern.txt"}){
  auto path=prefix+suffix;
  cx::require(!std::filesystem::exists(path)&&!std::filesystem::is_symlink(path),"output already exists: "+path);
 }
 // Fresh workdir keeps every gate artifact as auditable evidence.
 cx::require(std::filesystem::create_directories(workdir),
  "cannot create gate workdir (it must not already exist): "+workdir);
 auto work=std::filesystem::absolute(workdir);
 auto module_path=(work/"module.on1x").string(),adapter_path=(work/"adapter.c").string(),
      harness_path=(work/"boundary_harness.c").string(),executable_path=(work/"boundary").string(),
      fixture_path=(work/"chisel_fixture.on1x").string(),admission_path=(work/"admitted-extern.txt").string();
 std::string admission="on1x.admitted-extern/v"+std::to_string(profile_version(unit,bundle))+"\nidentity "+unit.fingerprint+"\n";
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);const auto& type=*lookup(unit,decl.type);
  admission+="extern "+adapter_name(unit,fn.c_name,true)+" (";
  for(std::size_t i=0;i<decl.parameters.size();++i){if(i)admission+=",";admission+=foreign_type(unit,decl.parameters[i].type)->admission(false);}
  admission+=") -> "+foreign_type(unit,type.result)->admission(true)+"\n";
 }
 std::string fixture="# Chisel usage check for the reviewed import.\nimport \""+module_path+"\";\nfn main(){\n";
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);const auto& type=*lookup(unit,decl.type);
  auto result=*foreign_type(unit,type.result);auto safe="on1x_safe_"+fn.c_name;fixture+="  ";
  if(result.kind==Scalar::Void)fixture+=safe+"(";else fixture+="println("+safe+"(";
  for(std::size_t i=0;i<decl.parameters.size();++i){if(i)fixture+=", ";auto arg=*foreign_type(unit,decl.parameters[i].type);
   fixture+=arg.kind==Scalar::CString?"\"\"":arg.kind==Scalar::Boolean?"true":arg.kind==Scalar::Binary64?"1.0":arg.kind==Scalar::SignedInteger?"1":arg.on1x()+"_from_i64(1)";}
  fixture+=result.kind==Scalar::Void?");\n":"));\n";
 }
 fixture+="}\n";
 std::string harness="#include <stdio.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n";
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);const auto& type=*lookup(unit,decl.type);
  auto result=*foreign_type(unit,type.result);auto name=adapter_name(unit,fn.c_name,true);
  harness+=result.adapter()+" "+name+"(";
  if(decl.parameters.empty())harness+="void";
  else for(std::size_t i=0;i<decl.parameters.size();++i){if(i)harness+=", ";harness+=foreign_type(unit,decl.parameters[i].type)->adapter();}
  harness+=");\n";
 }
 harness+="int main(void){\n";
 for(const auto& fn:bundle.functions){
  const auto& decl=*declarations.at(fn.c_name);const auto& type=*lookup(unit,decl.type);
  auto result=*foreign_type(unit,type.result);auto name=adapter_name(unit,fn.c_name,true);
  auto arguments=[&](){std::string value;for(std::size_t i=0;i<decl.parameters.size();++i){if(i)value+=", ";auto arg=*foreign_type(unit,decl.parameters[i].type);value+=arg.kind==Scalar::CString?"\"\"":arg.kind==Scalar::Boolean?"true":arg.kind==Scalar::Binary64?"0x1p+0":"1";}return value;};
  if(result.kind==Scalar::Void){
   harness+="  "+name+"("+arguments();
   harness+=");\n  printf(\""+fn.c_name+"=void\\n\");\n";
  }else if(result.kind==Scalar::CString){
   harness+="  printf(\""+fn.c_name+"=%zu\\n\",strlen("+name+"("+arguments()+")));\n";
  }else if(result.kind==Scalar::SignedInteger){
   harness+="  printf(\""+fn.c_name+"=%lld\\n\",(long long)"+name+"(";
   harness+=arguments()+"));\n";
  }else if(result.kind==Scalar::Boolean)
   harness+="  printf(\""+fn.c_name+"=%s\\n\","+name+"("+arguments()+") ? \"true\" : \"false\");\n";
  else if(result.kind==Scalar::UnsignedInteger)
   harness+="  printf(\""+fn.c_name+"=%llu\\n\",(unsigned long long)"+name+"("+arguments()+"));\n";
  else harness+="  printf(\""+fn.c_name+"=%a\\n\",(double)"+name+"("+arguments()+"));\n";
 }
 harness+="  return 0;\n}\n";
 write(admission_path,admission);write(module_path,output.language);write(adapter_path,output.native);
 write(harness_path,harness);write(fixture_path,fixture);
 // Gate C: the real compiler Chisel checker over the emitted module.
 auto chisel=run_process(on1xc,{"--check-memory","--admitted-extern",admission_path,fixture_path},work,8*1024*1024,120);
 cx::require(!chisel.timed_out&&!chisel.signaled&&chisel.exit_status==0,
  "Chisel usage gate failed (on1xc --check-memory): "+excerpt(chisel.error.empty()?chisel.out:chisel.error));
 // Gate T: strict target compile, then a bounded boundary run.
 auto include_directory=std::filesystem::path(unit.header).parent_path();
 if(include_directory.empty())include_directory=".";
 std::vector<std::string> compile={cc,"-std=c11","-Wall","-Wextra","-Wpedantic","-Werror","-I",include_directory.string(),adapter_path,harness_path};
 for(const auto& implementation:impls)compile.push_back(implementation);
 for(const auto& flag:cflags)compile.push_back(flag);
 compile.push_back("-o");compile.push_back(executable_path);
 auto built=run_process(compile.front(),std::vector<std::string>(compile.begin()+1,compile.end()),work,8*1024*1024,120);
 cx::require(!built.timed_out&&!built.signaled&&built.exit_status==0,
  "target compile gate failed: "+excerpt(built.error.empty()?built.out:built.error));
 auto boundary=run_process(executable_path,{},work,8*1024*1024,30);
 cx::require(!boundary.timed_out&&!boundary.signaled&&boundary.exit_status==0,
  "target runtime gate failed (boundary exit "+std::to_string(boundary.exit_status)+"): "+excerpt(boundary.error));
 std::vector<std::string> lines;std::size_t begin=0;
 while(begin<boundary.out.size()){
  auto end=boundary.out.find('\n',begin);cx::require(end!=std::string::npos,"boundary protocol missing newline");
  lines.push_back(boundary.out.substr(begin,end-begin));begin=end+1;
 }
 cx::require(lines.size()==bundle.functions.size(),
  "boundary protocol drift: expected "+std::to_string(bundle.functions.size())+" observations, got "+std::to_string(lines.size()));
 std::string observed;
 for(std::size_t i=0;i<lines.size();++i){
  const auto& fn=bundle.functions[i];
  cx::require(lines[i]==fn.c_name+"=void"||lines[i].starts_with(fn.c_name+"="),"boundary observation drift: "+excerpt(lines[i]));
  if(i)observed+=',';
  observed+="{\"name\":"+json::quote(fn.c_name)+",\"value\":"+json::quote(lines[i].substr(fn.c_name.size()+1))+"}";
 }
 std::string flags;
 for(auto flag:{"-std=c11","-Wall","-Wextra","-Wpedantic","-Werror"}){if(!flags.empty())flags+=',';flags+=json::quote(flag);}
 flags+=','+json::quote("-I")+','+json::quote(include_directory.string());
 for(const auto& flag:cflags)flags+=','+json::quote(flag);
 std::string implementations;
 for(const auto& implementation:impls){if(!implementations.empty())implementations+=',';implementations+=json::quote(implementation);}
 std::string manifest="{\"schema\":\"on1x.safe-ffi/v1\",\"safe_export\":true,\"profile\":"+json::quote(selected_profile)+",\"identity\":"+identity(unit);
 manifest+=",\"contracts\":"+contract::serialize_contract(bundle);
 manifest+=",\"eligibility\":{\"allowed_effects\":[\"read\",\"write\",\"io\",\"foreign_call\"],\"functions\":[";
 bool first_entry=true;
 for(const auto& fn:bundle.functions){
  if(!first_entry)manifest+=',';
  first_entry=false;
  manifest+="{\"name\":"+json::quote(fn.c_name)+",\"effects\":[";
  bool first_effect=true;for(auto effect:fn.effects.effects){if(!first_effect)manifest+=',';first_effect=false;manifest+=json::quote(effect_name(effect));}
  manifest+="],\"safe_scalar_abi_supported\":"+std::string(unsupported(unit,*declarations.at(fn.c_name),fn).empty()?"true":"false")+",\"safe_profile_abi_supported\":true}";
 }
 manifest+="]},\"gates\":{";
 manifest+="\"review_metadata\":{\"result\":\"pass\",\"note\":\"review flags are provenance claims recorded in contracts, not an independent semantic proof\"},";
 manifest+="\"profile_eligibility\":{\"result\":\"pass\"},";
 manifest+="\"chisel_usage_check\":{\"on1xc\":"+json::quote(on1xc)+",\"fixture\":"+json::quote(fixture_path)+",\"exit\":0,\"report\":"+json::quote(chisel.out)+"},";
 manifest+="\"runtime_acceptance\":{\"cc\":"+json::quote(cc)+",\"flags\":["+flags+"],\"implementations\":["+implementations+"],\"exit\":0,\"observed\":["+observed+"]}";
 manifest+="},\"admitted_extern\":{\"path\":"+json::quote(prefix+".admitted-extern.txt")+",\"content\":"+json::quote(admission);
 manifest+="},\"limitations\":[";
 manifest+="\"Boundary observations use fixed representative arguments (1 for scalars, empty immutable strings) and are recorded, never interpreted as semantic proofs.\",";
 manifest+="\"Foreign effects, ownership and error behavior are admitted as declared by the reviewed contract; the C implementation itself is not verified.\",";
 manifest+="\"Only v4 admits pointers: exact const char * with immutable call-scoped inputs and non-null immutable global_static results; other pointers, aggregates, callbacks, transfers and error contracts remain unsupported.\",";
 if(profile_version(unit,bundle)==4)manifest+="\"String input/output byte limits are 2097152/4194304, excluding NUL; null/limit failure terminates with exit 72. Review must establish valid NUL termination, no retained inputs and immutable result storage until process exit. These checks do not validate arbitrary pointer provenance or UTF-8.\",";
 manifest+="\"Gate artifacts live in the recorded workdir; the published module, adapter and admission file are byte-identical copies.\"";
 manifest+="]}\n";
 std::vector<std::pair<std::string,std::string>> files={
  {prefix+".on1x",output.language},{prefix+".c",output.native},
  {prefix+".manifest.json",manifest},{prefix+".admitted-extern.txt",admission}};
 for(const auto& [path,contents]:files){(void)contents;cx::require(!std::filesystem::exists(path)&&!std::filesystem::is_symlink(path),"output already exists: "+path);}
 std::vector<std::string> written;
 try{for(const auto& [path,contents]:files){write(path,contents);written.push_back(path);}}
 catch(...){for(const auto& path:written){std::error_code ignored;std::filesystem::remove(path,ignored);}throw;}
 std::cout<<"Emitted "<<output.generated<<" SAFE-ADMITTED imports ("<<selected_profile<<"); all four gates passed (review metadata, profile eligibility, Chisel usage check, target runtime acceptance). Recorded claims are not proofs of foreign implementation behavior.\n";
}
void usage(){std::cout<<"on1x-bind check|explain|pin|verify DECL.json CONTRACT.json\n"
 "on1x-bind normalize-decl DECL.json\n"
 "on1x-bind emit-raw|emit DECL.json CONTRACT.json OUT_PREFIX\n"
 "on1x-bind emit-safe DECL.json CONTRACT.json OUT_PREFIX --on1xc PATH --cc PATH --workdir DIR\n"
 "  [--cflag ARG] [--impl FILE] (reviewed scalar/immutable-string admission; runs all four gates)\n"
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
  bool arity_ok=mode=="emit-safe"?(argc==4||argc>=5):(emitting?argc==5:argc==4);
  if(!arity_ok){usage();return 2;}
  if(mode!="check"&&mode!="explain"&&mode!="pin"&&mode!="verify"&&mode!="emit-safe"&&!emitting){usage();return 2;}
  auto declaration=read(argv[2],64*1024*1024);auto unit=cx::parse_cdecl(declaration);
  auto annotation=read(argv[3],2*1024*1024);cx::validate_shape(json::parse(annotation));auto bundle=contract::parse_contract_sidecar(annotation);
  if(mode=="pin"){std::cout<<contract::serialize_contract(cx::pin(unit,std::move(bundle)));return 0;}
  cx::validate(unit,bundle,true);
  if(mode=="emit-safe"){
   if(argc==4)throw cx::Rejected("safe export unavailable without gate execution: emit-safe requires OUT_PREFIX plus --on1xc, --cc and --workdir; metadata-only admission does not exist");
   emit_safe(unit,bundle,argv[4],argc,argv,5);return 0;
  }
  if(mode=="check"||mode=="explain"){std::cout<<report(unit,bundle);return 0;}
  cx::require(contract::is_safe_for_export(bundle),"Gate B passed, but review metadata gate is not satisfied; no safe-export claim follows from either gate");
  if(mode=="verify"){std::cout<<"ExoML Gate B and review metadata passed for "<<bundle.functions.size()<<" functions; not a C semantic safety proof.\n";return 0;}
  auto output=emit(unit,bundle,"# Auto-generated RAW/UNSAFE On1x scalar import. Not a safe facade.\n",
                    "/* RAW/UNSAFE scalar ABI adapters. Rebuild with the recorded inputs. */\n",false);cx::require(output.generated>0,"no supported scalar ABI functions ("+std::to_string(output.skipped)+" skipped)");
  std::string prefix=argv[4];std::vector<std::pair<std::string,std::string>> files={{prefix+".on1x",output.language},{prefix+".c",output.native},{prefix+".manifest.json","{\"schema\":\"on1x.raw-ffi/v1\",\"safe_export\":false,\"identity\":"+identity(unit)+",\"contracts\":"+contract::serialize_contract(bundle)+",\"eligibility\":"+report(unit,bundle)+"}\n"}};
  for(const auto& [path,contents]:files){(void)contents;cx::require(!std::filesystem::exists(path)&&!std::filesystem::is_symlink(path),"output already exists: "+path);}
  std::vector<std::string> written;
  try{for(const auto& [path,contents]:files){write(path,contents);written.push_back(path);}}
  catch(...){for(const auto& path:written){std::error_code ignored;std::filesystem::remove(path,ignored);}throw;}
  std::cout<<"Emitted "<<output.generated<<" RAW/UNSAFE scalar adapters; "<<output.skipped<<" unsupported. C11 ABI checks and all recorded inputs are still required.\n";return 0;
 }catch(const cx::Rejected& e){std::cerr<<"on1x-bind: "<<e.what()<<"\n";return 3;}
 catch(const std::exception& e){std::cerr<<"on1x-bind: "<<e.what()<<"\n";return 2;}
}

#include <on1x/ast.hpp>
#include <on1x/analysis.hpp>
#include <on1x/sema.hpp>
#include <on1x/meta.hpp>
#include <on1x/loader.hpp>
#include <on1x/discovery.hpp>
#include <on1x/formatter.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/backend/vm.hpp>
#include <on1x/backend/passes.hpp>
#include "../hir/lower_mir.hpp"
#include "optimization_policy.hpp"
#ifdef ON1X_WITH_CEPHYR
#include "backend/cephyr.hpp"
#include "backend/cephyr_sarif.hpp"
#endif
#ifdef ON1X_WITH_EXTENSION_SDK
#include "Builtin.hpp"
#include "Driver.hpp"
#endif
#ifdef ON1X_WITH_SYREN
#include <on1x/diagnostics.hpp>
#endif
#ifdef ON1X_WITH_NEROTYPICAL
#include <on1x/type_safety.hpp>
#endif
#ifdef ON1X_WITH_CHISEL
#include <on1x/safety.hpp>
#endif
#ifdef ON1X_WITH_NOVOSTONE
#include <on1x/backend/relational.hpp>
#endif
#include <charconv>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
namespace {
 std::string terminal_text(std::string_view text){return on1x::diagnostic::terminal_text(text);}
 int project_command(int argc,char** argv){
#if defined(__unix__) || defined(__APPLE__)
  namespace fs=std::filesystem;
  std::error_code ec;
  fs::path compiler;
#if defined(__linux__)
  compiler=fs::canonical("/proc/self/exe",ec);
#elif defined(__APPLE__)
  uint32_t size=0;
  (void)::_NSGetExecutablePath(nullptr,&size);
  if(size){
   std::string name(size,'\0');
   if(::_NSGetExecutablePath(name.data(),&size)==0)compiler=fs::canonical(name.c_str(),ec);
  }
#endif
  if(compiler.empty()){
   fs::path candidate=argv[0];
   if(!candidate.has_parent_path()){
    std::vector<fs::path> paths;
    on1x::discovery::environment(paths,"PATH");
    candidate.clear();
    for(const auto& dir:paths)
     if(auto path=dir/argv[0];fs::is_regular_file(path)&&::access(path.c_str(),X_OK)==0){candidate=path;break;}
   }
   compiler=fs::canonical(candidate,ec);
   if(ec)throw std::runtime_error("cannot locate on1xc for the project driver");
  }
  auto installed=compiler.parent_path()/"../share/on1x";
  auto helper=installed/"tools/on1x-build.py";
  if(!fs::is_directory(installed))helper=fs::path(ON1X_DRIVER_SOURCE_TOOLS)/"on1x-build.py";
  auto script=helper.string();
  auto binary=compiler.string();
  if(::setenv("ON1X_PROJECT_COMPILER",binary.c_str(),1)!=0)
   throw std::runtime_error("cannot set compiler path for the project driver");
  std::vector<char*> arguments{script.data()};
  arguments.insert(arguments.end(),argv+1,argv+argc);
  arguments.push_back(nullptr);
  ::execv(script.c_str(),arguments.data());
  auto error=errno;
  throw std::runtime_error("cannot start project driver "+script+": "+std::strerror(error));
#else
  (void)argc;(void)argv;
  throw std::runtime_error("on1xc project commands require a POSIX host; try tools/on1x-build.py");
#endif
 }
#ifdef ON1X_WITH_EXTENSION_SDK
 int sdk_command(int argc,char** argv){
#if defined(__unix__) || defined(__APPLE__)
  namespace fs=std::filesystem;
  std::error_code ec;
  fs::path compiler;
#if defined(__linux__)
  compiler=fs::canonical("/proc/self/exe",ec);
#elif defined(__APPLE__)
  uint32_t size=0;
  (void)::_NSGetExecutablePath(nullptr,&size);
  if(size){
   std::string name(size,'\0');
   if(::_NSGetExecutablePath(name.data(),&size)==0)compiler=fs::canonical(name.c_str(),ec);
  }
#endif
  if(compiler.empty()){
   fs::path candidate=argv[0];
   if(!candidate.has_parent_path()){
    std::vector<fs::path> paths;
    on1x::discovery::environment(paths,"PATH");
    candidate.clear();
    for(const auto& dir:paths)
     if(auto path=dir/argv[0];fs::is_regular_file(path)&&::access(path.c_str(),X_OK)==0){candidate=path;break;}
   }
   compiler=fs::canonical(candidate,ec);
   if(ec)throw std::runtime_error("cannot locate on1xc for the SDK tool");
  }
  auto helper=compiler.parent_path()/"on1x-sdk";
  if(!fs::is_regular_file(helper)||::access(helper.c_str(),X_OK)!=0)
   helper=fs::path(ON1X_DRIVER_SOURCE_SDK_TOOL);
  auto script=helper.string();
  std::vector<char*> arguments{script.data()};
  arguments.insert(arguments.end(),argv+2,argv+argc);
  arguments.push_back(nullptr);
  ::execv(script.c_str(),arguments.data());
  auto error=errno;
  throw std::runtime_error("cannot start SDK tool "+script+": "+std::strerror(error));
#else
  (void)argc;(void)argv;
  throw std::runtime_error("on1xc SDK data commands require a POSIX host; run on1x-sdk directly");
#endif
 }
#endif
 void publish_regular_file(const std::string& destination,const std::string& text,
                           const std::string& action,const std::string& temporary_name){
#if defined(__unix__) || defined(__APPLE__)
  namespace fs=std::filesystem;
  const fs::path path=destination;
  const auto parent=path.has_parent_path()?path.parent_path():fs::path(".");
   auto temporary=(parent/temporary_name).string();
  struct stat prior{};
  const bool exists=::lstat(destination.c_str(),&prior)==0;
  if((exists&&!S_ISREG(prior.st_mode))||(!exists&&errno!=ENOENT))
    throw std::runtime_error(action+" output must be a regular file: "+destination);
  int fd=::mkstemp(temporary.data());
   if(fd<0)throw std::runtime_error("cannot stage "+action+" output: "+destination);
  bool ok=true;
  for(std::size_t written=0;written<text.size();){
   auto count=::write(fd,text.data()+written,text.size()-written);
   if(count<0&&errno==EINTR)continue;
   if(count<=0){ok=false;break;}
   written+=static_cast<std::size_t>(count);
  }
  if(ok&&exists&&::fchmod(fd,prior.st_mode&0777)<0)ok=false;
  if(::close(fd)<0)ok=false;
  if(ok&&std::rename(temporary.c_str(),destination.c_str())==0)return;
  ::unlink(temporary.c_str());
   throw std::runtime_error("cannot publish "+action+" output: "+destination);
#else
   throw std::runtime_error("atomic "+action+" file output requires a POSIX host; use stdout");
#endif
 }
 int format_command(int argc,char** argv,std::string_view command){
  namespace fmt=on1x::format;
  fmt::Options options;
  if(command=="--emit-latex")options.target=fmt::Target::latex;
  if(command=="--emit-troff")options.target=fmt::Target::troff;
  bool check=false, inplace=false, options_done=false;
  std::string source,out;
   auto usage_error=[](const std::string& message){
    std::cerr<<"on1xc: "<<terminal_text(message)<<'\n';
   return 2;
  };
  for(int i=2;i<argc;++i){
   std::string_view arg=argv[i];
   if(!options_done&&arg=="--"){options_done=true;continue;}
   if(!options_done&&(arg=="--help"||arg=="-h")){
    std::cout<<"usage: on1xc fmt INPUT [--target plain|latex|troff] [--check | --inplace | -i | -o OUTPUT]\n"
     "  -i, --inplace  Replace INPUT atomically with plain formatted source.\n";
    return 0;
   }
   if(!options_done&&arg=="--target"){
     if(++i==argc)return usage_error("--target requires plain, latex or troff");
    std::string_view target=argv[i];
    if(target=="plain")options.target=fmt::Target::plain;
    else if(target=="latex")options.target=fmt::Target::latex;
    else if(target=="troff")options.target=fmt::Target::troff;
     else return usage_error("unknown formatter target: "+std::string(target));
   }else if(!options_done&&arg=="--check")check=true;
   else if(!options_done&&(arg=="--inplace"||arg=="-i"))inplace=true;
   else if(!options_done&&arg=="--enable"){
    if(++i==argc)return usage_error("--enable requires registered feature names");
    std::set<std::string> selected;
    on1x::extensions::select(selected,argv[i],{"<command-line>"});
    options.enabled_extensions.insert(options.enabled_extensions.end(),selected.begin(),selected.end());
   }else if(!options_done&&arg=="-o"){
    if(++i==argc||!argv[i][0]||!out.empty())return usage_error("-o requires exactly one nonempty output path");
    out=argv[i];
   }else if(!options_done&&arg.starts_with('-'))return usage_error("unknown formatter option: "+std::string(arg));
   else if(source.empty()&&!arg.empty())source=arg;
   else return usage_error("formatter requires exactly one input file");
  }
  if(source.empty())return usage_error("formatter requires an input file");
  if(inplace){
   if(check||!out.empty())
    return usage_error("formatter --inplace cannot be combined with --check or -o");
   if(options.target&&*options.target!=fmt::Target::plain)
    return usage_error("formatter --inplace requires plain output");
   options.target=fmt::Target::plain;
  }else if(check){
   if(!out.empty()||(options.target&&*options.target!=fmt::Target::plain))
    return usage_error("formatter --check requires plain output and cannot be combined with -o");
   options.target=fmt::Target::plain;
  }
  auto result=fmt::file(source,options);
  if(check)return result==on1x::read_source_file(source)?0:1;
   if(inplace||(!out.empty()&&out!="-"))
    publish_regular_file(inplace?source:out,result,"formatter",".on1xfmt-XXXXXX");
  else if(!(std::cout<<result).flush())throw std::runtime_error("cannot write formatted output");
  return 0;
 }
 bool diagnostic_color(const std::string& mode,const std::string& format){
  if(format!="text"||mode=="never")return false;
  if(mode=="always")return true;
  if(std::getenv("NO_COLOR"))return false;
#if defined(__unix__) || defined(__APPLE__)
  return ::isatty(STDERR_FILENO);
#else
  return false;
#endif
 }
#ifdef ON1X_WITH_SYREN
  std::optional<std::string> diagnostic_source(const on1x::Location& where,
   const std::map<std::string,std::string>& snapshots,bool fallback=true){
   if(auto found=snapshots.find(where.file);found!=snapshots.end())return found->second;
   if(!fallback)return {};
   try{
   auto source=on1x::read_source_file(where.file);
   if(source.size()<=on1x::max_source_bytes)return source;
  }catch(const std::exception&){}
  return {};
 }
  void emit_diagnostics(const on1x::diagnostics::Bundle& bundle,const std::string& format,bool color){
  std::cerr<<(format=="json"?on1x::diagnostics::json(bundle):
   format=="sarif"?on1x::diagnostics::sarif(bundle):on1x::diagnostics::text(bundle,color));
 }
  on1x::diagnostics::Bundle diagnostic_bundle(const on1x::diagnostic::Registry& registry,
   const on1x::diagnostic::Catalog& catalog,const std::map<std::string,std::string>& snapshots){
   return on1x::diagnostics::describe(registry,catalog,[&](const on1x::Location& where){
    // Registry locations include PPX-controlled paths. Never grant them a
    // filesystem read; only immutable snapshots from the loaded graph qualify.
    return diagnostic_source(where,snapshots,false);
   });
 }
 void report_registry(const on1x::diagnostic::Registry& registry,
  const on1x::diagnostic::Catalog& catalog,const std::map<std::string,std::string>& snapshots,
  const std::string& format,bool color){
  if(registry.empty()&&!registry.dropped())return;
  emit_diagnostics(diagnostic_bundle(registry,catalog,snapshots),format,color);
 }
  void report_error(const on1x::Error& error,const on1x::diagnostic::Registry& registry,
   const on1x::diagnostic::Catalog& catalog,const std::map<std::string,std::string>& snapshots,
   const std::string& format,bool color,bool include_registry,bool source_fallback){
  auto bundle=include_registry?diagnostic_bundle(registry,catalog,snapshots):on1x::diagnostics::Bundle{};
  bundle.presentation=catalog.presentation();
  auto described=on1x::diagnostics::describe(error,std::nullopt,catalog);
  const bool duplicate=std::any_of(bundle.diagnostics.begin(),bundle.diagnostics.end(),[&](const auto& item){
   return item.diagnostic.id==described.diagnostic.id&&item.diagnostic.message==described.diagnostic.message&&
    item.diagnostic.primary.file==error.where.file&&item.offset==error.where.offset;
  });
   if(!duplicate){
     auto source=diagnostic_source(error.where,snapshots,source_fallback);
    std::size_t retained=0;
    for(const auto& item:bundle.diagnostics)if(item.source)retained+=item.source->size();
    if(source&&(retained>=on1x::diagnostic::max_source_snapshot_bytes||
       source->size()>on1x::diagnostic::max_source_snapshot_bytes-retained))source.reset();
    bundle.diagnostics.push_back(on1x::diagnostics::describe(error,std::move(source),catalog));
   }
  emit_diagnostics(bundle,format,color);
 }
  int repair_source(const std::string& source,const std::string& out,const on1x::LoaderOptions& options,
   const on1x::processor::RuleSelection& rules,const on1x::diagnostic::Catalog& catalog,const std::string& format,bool color){
 namespace fs=std::filesystem;
 using Failure=on1x::diagnostic::CommandFailure;
 if(out.empty()||out=="-")throw std::runtime_error(catalog.command_failure(Failure::RepairOutput));
 // Refuse all existing destinations, including symlinks and imported sources.
 if(fs::symlink_status(out).type()!=fs::file_type::not_found)
  throw std::runtime_error(catalog.command_failure(Failure::RepairExists));
 auto path=fs::canonical(source);
 auto original=on1x::read_source_file(path.string());
 auto compile=[&](std::string_view text){
  auto overlay=options;overlay.source_overrides[path]=std::string(text);
  auto loaded=on1x::load_unit(path,overlay);
  if(!loaded.unit.ppx.empty())throw std::runtime_error(catalog.presentation().source_ppx_unavailable);
  on1x::diagnostic::Registry validation_records;
  on1x::analysis::Context validation_context{catalog,validation_records,{},{}};
   on1x::check(loaded.unit,validation_context,rules);
  if(validation_records.has_errors()){
   const auto& finding=*std::find_if(validation_records.records().begin(),validation_records.records().end(),
    [](const auto& item){return item.level.failure;});
   throw on1x::Error(finding.where,finding.message,finding.id);
  }
  (void)on1x::emit_c(loaded.unit,path.string());
 };
  try{compile(original);std::cerr<<terminal_text(catalog.presentation().already_valid)<<"\n";return 0;}
 catch(const on1x::Error& error){
  if(error.where.file!=path.string())throw; // Do not misapply imported-file offsets.
   auto report=on1x::diagnostics::describe(error,original,catalog);
  on1x::diagnostics::validate(report,[&](std::string_view changed){
   using namespace moonstone;
    try{compile(changed);return ProofResult{Status::Proven,catalog.presentation().compiler_validated,{},1};}
    catch(const on1x::Error& e){
     auto detail=e.where.file+":"+std::to_string(e.where.line)+": "+e.what();
     return ProofResult{Status::Unknown,on1x::diagnostic::expand(catalog.presentation().validator_failure,{.message=detail}),{},1};
    }catch(const std::exception& e){
     return ProofResult{Status::Unknown,on1x::diagnostic::expand(catalog.presentation().validator_failure,{.message=e.what()}),{},1};
    }
  });
   on1x::diagnostics::Bundle bundle;bundle.diagnostics.push_back(report);
   emit_diagnostics(bundle,format,color);
  std::optional<std::string> proposal;
  for(const auto& candidate:report.repairs)if(candidate.status==moonstone::syren::RepairStatus::Validated){
   auto changed=moonstone::apply_edits(original,candidate.edits);
    if(proposal&&proposal!=changed){std::cerr<<terminal_text(catalog.presentation().ambiguous_repairs)<<"\n";return 1;}
   proposal=std::move(changed);
  }
  if(!proposal)return 1;
  // Recheck before publication; compilation and validation never write sources.
   if(fs::symlink_status(out).type()!=fs::file_type::not_found)
    throw std::runtime_error(catalog.command_failure(Failure::RepairExists));
  auto* output=std::fopen(out.c_str(),"wbx"); // C11 exclusive creation, including against symlink races.
   if(!output)throw std::runtime_error(catalog.command_failure(Failure::RepairCreate,{.message=out}));
  bool written=std::fwrite(proposal->data(),1,proposal->size(),output)==proposal->size();
  if(std::fclose(output)!=0)written=false;
   if(!written)throw std::runtime_error(catalog.command_failure(Failure::RepairWrite,{.message=out}));
  return 0;
 }
 }
 #endif
#ifndef ON1X_WITH_SYREN
 void report_registry(const on1x::diagnostic::Registry& registry,
  const on1x::diagnostic::Catalog& catalog,const std::map<std::string,std::string>&,
  const std::string&,bool color){
  std::cerr<<on1x::diagnostic::text(registry,catalog,color);
 }
#endif
void write_all(const std::string& name,const std::string& text){
 if(name.empty()||name=="-"){std::cout<<text;return;}
 std::ofstream out(name,std::ios::binary);if(!out||(out<<text).fail())throw std::runtime_error("cannot write "+name);
}
std::string read_ir(const std::string& name){
 std::ifstream in(name,std::ios::binary);if(!in)throw std::runtime_error("cannot read "+name);
 std::string text;char chunk[8192];
 while(in){in.read(chunk,sizeof chunk);auto n=static_cast<std::size_t>(in.gcount());
  if(n>on1x::meta::max_wire_bytes-text.size())throw std::runtime_error("MIR input byte limit exceeded");
  text.append(chunk,n);
 }
 if(!in.eof())throw std::runtime_error("cannot read "+name);
 return text;
}
using VmSourcePrefixes=std::map<std::string,std::string>;
void vm_source_paths(on1x::mir::Module& module,const VmSourcePrefixes& prefixes){
 auto map=[&](on1x::Location& at){
  const std::string* replacement=nullptr;std::size_t length=0;
  for(const auto& [from,to]:prefixes){
   if(from.size()<=length||!at.file.starts_with(from))continue;
   if(from!="/"&&at.file.size()!=from.size()&&at.file[from.size()]!='/')continue;
   length=from.size();replacement=&to;
  }
  if(replacement){
   auto suffix=at.file.substr(length);
   if(!suffix.empty()&&suffix.front()=='/')suffix.erase(0,1);
   at.file=suffix.empty()?*replacement:(std::filesystem::path(*replacement)/suffix).lexically_normal().generic_string();
  }
 };
 for(auto& f:module.functions){
  map(f.where);for(auto& s:f.slots)map(s.where);
  for(auto& b:f.blocks){
   map(b.where);for(auto& p:b.parameters)map(p.where);
   for(auto& i:b.instructions)map(i.where);
   map(b.exit.where);
  }
 }
}
int mir_output(const on1x::mir::Module& input,const std::string& mode,const std::string& out,std::size_t analysis_work,const on1x::mir::passes::Options* optimization,const VmSourcePrefixes& prefixes){
 std::optional<on1x::mir::passes::Result> optimized;
 if(optimization){
  optimized=on1x::mir::passes::run(input,*optimization);
  if(optimized->status()==on1x::mir::passes::Status::BudgetExhausted)
    std::cerr<<"on1xc: ON1X-MIR-PASS: "<<terminal_text(optimized->diagnostic())<<"; using last verified MIR prefix\n";
 }
 if(mode=="--dump-passes"){write_all(out,on1x::mir::passes::report(*optimized));return 0;}
  const auto& module=optimized?optimized->module():input;
  if(mode=="--emit-vm"||mode=="--run-vm"){
   auto image=[&]{
    if(prefixes.empty())return on1x::vm::compile(module);
    auto portable=module;vm_source_paths(portable,prefixes);return on1x::vm::compile(portable);
   }();
   if(mode=="--emit-vm"){
    if(out.empty()||out=="-")write_all(out,image);
    else publish_regular_file(out,image,"mir-vm",".on1x-vm-XXXXXX");
    return 0;
   }
   auto r=on1x::vm::run_main(image);
   write_all(out,r.output);
   if(r.status==on1x::mir::Status::Returned)return static_cast<int>(r.value);
   std::cerr<<terminal_text(r.where.file)<<":"<<r.where.line<<":"<<r.where.column
            <<": ON1X-VM: "<<terminal_text(r.diagnostic)<<"\n";
   return r.status==on1x::mir::Status::Trapped?70:75;
  }
#ifdef ON1X_WITH_NOVOSTONE
 if(mode=="--emit-ir-facts"||mode=="--analyze-ir"){
  namespace r=on1x::mir::relational;r::Store store(module);
  novostone::datalog::Limits limits;limits.work=analysis_work;
  write_all(out,mode=="--emit-ir-facts"?r::dump_facts(store.snapshot()):r::report(store.snapshot(),limits));return 0;
 }
#else
 (void)analysis_work;
#endif
#ifdef ON1X_WITH_EXTENSION_SDK
  const char *backend=mode=="--run-ir"?"mir-eval":mode=="--emit-ir"?"mir-text":
                      mode=="--ir-cfg"?"mir-cfg":"mir-c11";
  auto result=on1x::backends::run_mir(module,backend);
  write_all(out,result.bytes);
  if(!result.diagnostic.empty())std::cerr<<result.diagnostic;
  return result.exit_code;
#else
   if(mode=="--run-ir"){
   for(const auto& f:module.functions)if(f.name=="main"&&(!f.blocks.front().parameters.empty()||(f.result!=on1x::Type::I64&&f.result!=on1x::Type::Void)))
    throw on1x::Error(f.where,"ON1X-MIR-EVAL: main must have no parameters and return i64 or void");
  auto result=on1x::mir::evaluate(module);write_all(out,result.output);
  if(result.status!=on1x::mir::Status::Returned){
    std::cerr<<terminal_text(result.where.file)<<":"<<result.where.line<<":"<<result.where.column<<": ON1X-MIR-EVAL: "<<terminal_text(result.diagnostic)<<"\n";
   return result.status==on1x::mir::Status::Trapped?70:75;
  }
  return static_cast<int>(result.value);
 }
  write_all(out,mode=="--emit-ir"?on1x::mir::serialize(module):mode=="--ir-cfg"?on1x::mir::cfg_dot(module):on1x::mir::emit_c(module));return 0;
#endif
}
std::string kernel_requests(const on1x::KernelImportRequests& inspected){
 // Filesystem paths need not be UTF-8. Retain exact bytes without emitting
 // malformed JSON; module and alias spellings are scanner identifiers.
 auto path_hex=[](const std::string& file){
  constexpr char hex[]="0123456789abcdef";
  std::string out;out.reserve(file.size()*2);
  for(unsigned char c:file){out+=hex[c>>4];out+=hex[c&15];}return out;
 };
 std::ostringstream out;
 out<<"{\"schema\":\"on1x.kernel-requests/v1\",\"sources\":[";
 bool first=true;
 for(const auto& [file,digest]:inspected.source_digests){
  if(!first)out<<',';
  first=false;out<<"{\"file_hex\":\""<<path_hex(file)<<"\",\"sha256\":\""<<digest<<"\"}";
 }
 out<<"],\"requests\":[";first=true;
 for(const auto& item:inspected.requests){
  if(!first)out<<',';
  first=false;out<<"{\"file_hex\":\""<<path_hex(item.where.file)<<"\",\"line\":"<<item.where.line
    <<",\"column\":"<<item.where.column<<",\"offset\":"<<item.where.offset
    <<",\"module\":\""<<item.module<<"\",\"alias\":\""<<item.alias<<"\",\"unsafe\":"
    <<(item.is_unsafe?"true":"false")<<(item.dynlib?",\"linkage\":\"dynlib\"":"")<<"}";
 }
 out<<"]}\n";
 return out.str();
}
void usage(){std::cout<<"on1xc - a small strict ML-family frontend targeting ISO C11\n"
 "usage: on1xc [--check | --emit-c | --tokens | --types | --emit-ast] INPUT.on1x [-o OUTPUT]\n"
#ifdef ON1X_WITH_CEPHYR
 "       on1xc --emit-cephyr-object INPUT.on1x -o OUTPUT.o [--cephyr-target x86_64-linux-gnu]\n"
 "       [--cephyr-factir on|off] [--cephyr-blackstone on|off] [--cephyr-sandslab on|off]\n"
 "       [--cephyr-profile scalar] [--cephyr-qml-pass NAME]\n"
 "       [--cephyr-trusted-qml-file FILE] [--cephyr-qml-fuel N] [--cephyr-qml-param PHASE.fuel=N]\n"
 "       [--cephyr-sarif FILE] (Cephyr diagnostics; generated C locations)\n"
 "       [--cephyr-factir-facts NEWFILE] [--cephyr-factir-smtlib NEWFILE]\n"
#endif
  "       on1xc --check-interfaces INPUT.on1x --interface FILE.on1xi [--interface FILE.on1xi ...]\n"
 "       [--ppx LABEL PROGRAM] [--ppx-schema LABEL FILE] [--allow-source-ppx]\n"
 "       [--ppx-timeout-ms MILLISECONDS]\n"
  "       [--enable NAME[,NAME]] [--disable NAME] (bundled native extensions only)\n"
  "       on1xc --list-extensions | --explain-extension NAME\n"
  "       on1xc --emit-category-plan INPUT [-o OUTPUT] (checked concrete evidence)\n"
   "       on1xc --kernel-imports INPUT [-o OUTPUT] (source requests only; never executes PPX)\n"
    "       on1xc --emit-c --kernel-c11 INPUT (unsafe KernelBind C generation; link with on1x-build.py)\n"
    "       on1xc --emit-c --kernel-c11 --kernel-stage INPUT (unresolved kernel callers; NOT admission)\n"
   "       [-I DIRECTORY | --module-path DIRECTORY] [--stdlib DIRECTORY]\n"
   "       [--lib-path DIR:DIR] [--stdlib-path DIR:DIR]\n"
   "       [--ppx-path DIR:DIR] [--stdppx-path DIR:DIR] [--use-ppx NAME]\n"
   "       on1xc --print-stdlib (resolved first standard-library root)\n"
   "       on1xc --stdlib-resource RELATIVE_PATH (find a standard-library resource)\n"
  "       [--no-prelude] [--prelude FILE] [--stdppx PROCESSOR]\n"
  "       on1xc --deps INPUT.on1x [-o OUTPUT] (dependency paths, no PPX execution)\n"
   "       on1xc [--emit-ir | --emit-ir-c | --ir-cfg | --run-ir] INPUT [-o OUTPUT] [--ir-input]\n"
   "       on1xc [--emit-vm | --run-vm] INPUT [-o OUTPUT] [--ir-input]\n"
   "         --vm-source-prefix FROM TO  remap absolute diagnostic paths in emitted bytecode\n"
#ifdef ON1X_WITH_EXTENSION_SDK
   "       on1xc --sdk-analyze NAME|linked-v2:HOOK INPUT [--ir-input]\n"
    "       on1xc --sdk-emit NAME INPUT [-o OUTPUT] [--ir-input]\n"
    "       on1xc --sdk-emit mir-archon --sdk-target TRIPLE INPUT [-o ASSEMBLY] [--ir-input] [--optimize-ir]\n"
    "         native options: --native-pic --native-no-entry\n"
   "       [--plugin +NAME_OR_PATH] (repeatable in SDK hook modes)\n"
   "       on1xc sdk COMMAND [ARGS ...] (bounded SDK data tool)\n"
#endif
  "       on1xc [--emit-ir-facts | --analyze-ir] INPUT [-o OUTPUT] [--ir-input]\n"
  "       [--analysis-work UNITS] (Novostone opt-in build; --analyze-ir only)\n"
   "       [--optimize-ir] [--opt-work UNITS] [--opt-rewrites COUNT] (MIR modes only)\n"
   "       [--opt-profile default|debug|release] [--enable-opt NAME] [--disable-opt NAME]\n"
   "       on1xc --list-optimizations (JSON; accepts profile and selection flags)\n"
   "         --profile aliases --opt-profile; --all-optimizations / --no-optimizations select all\n"
  "       on1xc --dump-passes INPUT [-o OUTPUT] [--ir-input]\n"
  "       on1xc --dump-processor INPUT [-o OUTPUT]\n"
  "       on1xc --dump-memgraph INPUT [-o OUTPUT] (inspect C11 bounds decisions)\n"
  "       on1xc --check-memory INPUT [-o OUTPUT] (Chisel opt-in build)\n"
  "       [--admitted-extern FILE] (reviewed scalar-only extern evidence; memory modes only)\n"
  "       [--memory-check] (check resource usage before source compilation)\n"
  "       on1xc --check-types INPUT [-o OUTPUT] (Nerotypical opt-in build)\n"
   "       [--verify-types] (require independent structural HIR type verification)\n"
    "       [--diagnostics text|json|sarif] [--color auto|always|never] (diagnostics use stderr)\n"
    "       [--diagnostic-catalog FILE] [--scrutiny LEVEL] [--project-scrutiny LEVEL]\n"
    "       [--scrutiny-work UNITS] [--no-analysis-repairs] (bounded Wetstone analysis)\n"
   "       on1xc --repair INPUT.on1x -o NEW_FILE (Syren; reviewable proposal only)\n"
  "       on1xc --help\n"
   "       on1xc init (--bin | --proj | --lib) [DIRECTORY]\n"
   "       on1xc exec COMMAND [ARGS ...] (explicit On1xfile command execution)\n"
    "       on1xc fmt INPUT [--target plain|latex|troff] [--check | --inplace | -i | -o OUTPUT]\n"
   "       on1xc (--format | --emit-latex | --emit-troff) INPUT [-o OUTPUT]\n"
  "--emit-ast expands macros and exports pre-PPX AST; it never executes programs.\n"
  "MIR modes support i64/bool/void, direct On1x calls, control flow and scalar print/assert.\n"
  "--run-ir is a bounded reference evaluator, not a native or VM backend.\n"
  "--ir-input reads verified serialized MIR instead of On1x source (MIR modes only).\n"
 "Output defaults to stdout. Use tools/on1x-build.py to link a native executable.\n";}
 }
 int main(int argc,char**argv){
  std::string diagnostic_format="text",color_mode="auto";
  on1x::diagnostic::Registry diagnostic_registry;
  const on1x::diagnostic::Catalog* diagnostic_catalog=nullptr;
  std::optional<on1x::diagnostic::Catalog> external_catalog;
   std::map<std::string,std::string> diagnostic_snapshots;
   std::size_t diagnostic_snapshot_bytes=0;
  bool color=false,diagnostics_published=false;
  [[maybe_unused]] bool diagnostic_sources_sealed=false;
  try{
    diagnostic_catalog=&on1x::diagnostic::Catalog::embedded();
    using CommandFailure=on1x::diagnostic::CommandFailure;
    auto command_failure=[&](CommandFailure kind,std::string_view detail={}){
     return std::runtime_error(diagnostic_catalog->command_failure(kind,{.message=detail}));
    };
   if(argc>1&&(std::string_view(argv[1])=="init"||std::string_view(argv[1])=="exec"))
    return project_command(argc,argv);
#ifdef ON1X_WITH_EXTENSION_SDK
   if(argc>1&&std::string_view(argv[1])=="sdk")
    return sdk_command(argc,argv);
#endif
   if(argc>1){
    std::string_view command=argv[1];
    if(command=="fmt"||command=="--format"||command=="--emit-latex"||command=="--emit-troff")
     return format_command(argc,argv,command);
   }
         std::string mode="--emit-c",source,out,explain_extension,stdlib_resource;bool ir_input=false;
          std::string sdk_mode,sdk_name,sdk_target;
          bool native_pic=false,native_no_entry=false;
         std::vector<std::string> plugin_selectors;
         bool explicit_c=false;
         bool cephyr_object=false;
         std::string cephyr_target;
#ifdef ON1X_WITH_CEPHYR
         on1x::cephyr::Options cephyr_options;
         std::string cephyr_sarif;
         std::set<std::string> cephyr_switches;
#endif
     std::size_t analysis_work=10000000;bool analysis_limit=false;
        on1x::mir::passes::Options optimization;bool legacy_optimize=false,opt_limit=false;
         on1x::driver::OptimizationRequest requested_opts;
         VmSourcePrefixes vm_prefixes;
        bool list_opts=false;
       on1x::analysis::Options scrutiny;
       [[maybe_unused]] bool scrutiny_option=false;
       bool scrutiny_work_invalid=false;
       bool color_option=false;
       std::string diagnostic_catalog_file;
      bool memory=false;
      bool type_safety=false;
    on1x::meta::PpxOptions ppx;
    auto loader=on1x::default_loader_options(argv[0]);
     std::vector<std::filesystem::path> import_paths;
     std::vector<std::filesystem::path> stdlib_paths,ppx_paths,stdppx_paths;
    std::vector<std::string> admission_files;
    for(int i=1;i<argc;++i){std::string arg=argv[i];
         if(arg=="--help"||arg=="-h"){usage();return 0;}
         if(arg=="--vm-source-prefix"){
          if(i+2>=argc||vm_prefixes.size()>=16)
           throw std::runtime_error("--vm-source-prefix needs two absolute paths (maximum 16 maps)");
          auto path=[&]{
           std::filesystem::path p(argv[++i]);
           if(!p.is_absolute()||p.native().size()>4096||p!=p.lexically_normal())
            throw std::runtime_error("--vm-source-prefix requires normalized absolute paths");
           return p.generic_string();
          };
          auto from=path(),to=path();
          if(!vm_prefixes.emplace(from,to).second)throw std::runtime_error("duplicate VM source prefix");
          continue;
         }
        if(arg=="--opt-profile"||arg=="--profile"||arg=="--enable-opt"||arg=="--disable-opt"){
         if(++i==argc||!argv[i][0]||argv[i][0]=='-')throw std::runtime_error(arg+" requires a name");
         if(arg=="--enable-opt"||arg=="--disable-opt")requested_opts.select(argv[i],arg=="--enable-opt");
         else requested_opts.set_profile(argv[i]);
         continue;
        }
        if(arg=="--no-optimizations"||arg=="--all-optimizations"){
         requested_opts.select("all",arg=="--all-optimizations");continue;
        }
        if(arg=="--list-optimizations"){list_opts=true;continue;}
       if(arg=="--sdk-analyze"||arg=="--sdk-emit"){
         if(!sdk_mode.empty()||++i>=argc||!argv[i][0]||argv[i][0]=='-')
         throw std::runtime_error("specify one SDK mode with a nonempty hook name");
         sdk_mode=arg;sdk_name=argv[i];continue;
        }
         if(arg=="--native-pic"){native_pic=true;continue;}
         if(arg=="--native-no-entry"){native_no_entry=true;continue;}
         if(arg=="--sdk-target"){
          if(!sdk_target.empty()||++i>=argc||!argv[i][0])
           throw std::runtime_error("--sdk-target requires one nonempty target triple");
          sdk_target=argv[i];
          if(sdk_target.size()>256||sdk_target.find_first_not_of(
             "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=std::string::npos)
           throw std::runtime_error("invalid --sdk-target triple");
          continue;
         }
         if(arg=="--plugin"){
         if(++i>=argc)throw std::runtime_error("--plugin requires +NAME_OR_PATH");
         std::string_view selector=argv[i];
         if(!selector.starts_with('+')||selector.size()==1)
          throw std::runtime_error("--plugin requires +NAME_OR_PATH");
         selector.remove_prefix(1);
         if(selector.size()>4096)throw std::runtime_error("--plugin selector exceeds 4096 bytes");
         if(plugin_selectors.size()>=32)throw std::runtime_error("--plugin selector count limit exceeded (32)");
         plugin_selectors.emplace_back(selector);continue;
        }
      if(arg=="--enable"||arg=="--disable"){
       if(++i>=argc)throw std::runtime_error(arg+" requires registered feature names");
       on1x::extensions::select(arg=="--enable"?loader.extensions.enabled:loader.extensions.disabled,argv[i],{"<command-line>"});continue;
      }
       if(arg=="--list-extensions"){mode=arg;continue;}
      if(arg=="--explain-extension"){
       if(++i>=argc)throw std::runtime_error("--explain-extension requires a feature name");
        explain_extension=argv[i];mode=arg;continue;
      }
       if(arg=="--repair"||arg=="--print-stdlib"){mode=arg;continue;}
      if(arg=="--stdlib-resource"){
       if(++i>=argc||!argv[i][0])throw std::runtime_error(arg+" requires a nonempty relative path");
        stdlib_resource=argv[i];mode=arg;continue;
      }
      if(arg=="--lib-path"||arg=="--stdlib-path"||arg=="--ppx-path"||arg=="--stdppx-path"){
       if(++i>=argc||!argv[i][0])throw std::runtime_error(arg+" requires a nonempty search list");
       auto& paths=arg=="--lib-path"?import_paths:arg=="--stdlib-path"?stdlib_paths:arg=="--ppx-path"?ppx_paths:stdppx_paths;
       on1x::discovery::append_paths(paths,argv[i]);continue;
      }
      if(arg=="--diagnostics"){
      if(++i>=argc)throw command_failure(CommandFailure::FormatMissing);
      diagnostic_format=argv[i];
      if(diagnostic_format!="text"&&diagnostic_format!="json"&&diagnostic_format!="sarif")
       throw command_failure(CommandFailure::FormatUnknown,diagnostic_format);
 #ifndef ON1X_WITH_SYREN
       if(diagnostic_format!="text")
        throw command_failure(CommandFailure::StructuredUnavailable);
 #endif
       continue;
      }
      if(arg=="--color"){
       if(++i>=argc)throw command_failure(CommandFailure::ColorMissing);
       if(color_option)throw command_failure(CommandFailure::ColorDuplicate);
       color_option=true;color_mode=argv[i];
       if(color_mode!="auto"&&color_mode!="always"&&color_mode!="never")
        throw command_failure(CommandFailure::ColorUnknown,color_mode);
       continue;
      }
      if(arg=="--diagnostic-catalog"){
       if(++i>=argc||!argv[i][0]||!diagnostic_catalog_file.empty())
        throw command_failure(CommandFailure::CatalogArgument);
       diagnostic_catalog_file=argv[i];continue;
      }
      if(arg=="--scrutiny"||arg=="--project-scrutiny"){
       if(++i>=argc||!argv[i][0])throw command_failure(CommandFailure::ScrutinyArgument,arg);
       auto& selected=arg=="--scrutiny"?scrutiny.command_scrutiny:scrutiny.project_scrutiny;
       if(selected)throw command_failure(CommandFailure::OptionDuplicate,arg);
       selected=argv[i];scrutiny_option=true;continue;
      }
      if(arg=="--scrutiny-work"){
       if(++i>=argc)throw command_failure(CommandFailure::WorkMissing);
       std::string value=argv[i];auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),scrutiny.work);
       if(ec!=std::errc{}||end!=value.data()+value.size()||!scrutiny.work||scrutiny.work>10000000)
        scrutiny_work_invalid=true;
       scrutiny_option=true;continue;
      }
      if(arg=="--no-analysis-repairs"){scrutiny.repairs=false;scrutiny_option=true;continue;}
       if(arg=="--memory-check"){memory=true;continue;}
       if(arg=="--kernel-c11"){loader.kernel_c11=true;continue;}
       if(arg=="--kernel-stage"){loader.kernel_stage=true;continue;}
        if(arg=="--check-memory"||arg=="--check-interfaces"||arg=="--dump-processor"||arg=="--dump-memgraph"){mode=arg;continue;}
     if(arg=="--interface"){
      if(++i>=argc||!argv[i][0])throw std::runtime_error("--interface requires a nonempty path");
      if(loader.interfaces.size()>=on1x::max_interface_files)throw std::runtime_error("interface file count limit exceeded");
      loader.interfaces.emplace_back(argv[i]);continue;
     }
    if(arg=="--admitted-extern"){
     if(++i>=argc||!argv[i][0])throw std::runtime_error("--admitted-extern requires a nonempty path");
     admission_files.emplace_back(argv[i]);continue;
    }
    if(arg=="--verify-types"){type_safety=true;continue;}
    if(arg=="--emit-cephyr-object"){cephyr_object=true;continue;}
#ifdef ON1X_WITH_CEPHYR
    if(arg=="--cephyr-factir"||arg=="--cephyr-blackstone"||arg=="--cephyr-sandslab"||arg=="--cephyr-profile"||arg=="--cephyr-qml-pass"||
       arg=="--cephyr-trusted-qml-file"||arg=="--cephyr-qml-fuel"||
       arg=="--cephyr-qml-param"||arg=="--cephyr-sarif"||
       arg=="--cephyr-factir-facts"||arg=="--cephyr-factir-smtlib"){
     if(++i>=argc||!argv[i][0]||argv[i][0]=='-')throw std::runtime_error(arg+" requires a value");
     const std::string value=argv[i];
     if(!cephyr_switches.insert(arg).second&&arg!="--cephyr-qml-pass"&&
        arg!="--cephyr-trusted-qml-file"&&arg!="--cephyr-qml-param")
      throw std::runtime_error("duplicate "+arg);
     if(arg=="--cephyr-factir"||arg=="--cephyr-sandslab"||arg=="--cephyr-blackstone"){
      if(value!="on"&&value!="off")throw std::runtime_error(arg+" requires on or off");
      if(arg=="--cephyr-factir")cephyr_options.factir=value=="on";
      else if(arg=="--cephyr-blackstone")cephyr_options.blackstone=value=="on";
      else cephyr_options.sandslab=value=="on";
     }else if(arg=="--cephyr-profile")cephyr_options.profile=value;
     else if(arg=="--cephyr-qml-pass")cephyr_options.passes.push_back(value);
     else if(arg=="--cephyr-trusted-qml-file")cephyr_options.trusted_files.emplace_back(value);
     else if(arg=="--cephyr-qml-fuel")cephyr_options.fuel=value;
     else if(arg=="--cephyr-qml-param")cephyr_options.parameters.push_back(value);
     else if(arg=="--cephyr-factir-facts")cephyr_options.facts=value;
     else if(arg=="--cephyr-factir-smtlib")cephyr_options.smtlib=value;
     else{cephyr_options.sarif=true;cephyr_sarif=value;}
     continue;
    }
#endif
    if(arg=="--cephyr-target"){
     if(++i>=argc||!argv[i][0]||argv[i][0]=='-'||!cephyr_target.empty())
      throw std::runtime_error("--cephyr-target requires one target triple");
     cephyr_target=argv[i];continue;
    }
     if(arg=="--check-types"){mode=arg;continue;}
     if(arg=="--emit-category-plan"||arg=="--kernel-imports"){mode=arg;continue;}
       if(arg=="--check"||arg=="--emit-c"||arg=="--tokens"||arg=="--types"||arg=="--emit-ast"||arg=="--deps"||arg=="--emit-ir"||arg=="--emit-ir-c"||arg=="--ir-cfg"||arg=="--run-ir"||arg=="--emit-vm"||arg=="--run-vm"||arg=="--emit-ir-facts"||arg=="--analyze-ir"||arg=="--dump-passes"){
        mode=arg;explicit_c=explicit_c||arg=="--emit-c";
      }
      else if(arg=="--no-prelude")loader.prelude=false;
      else if(arg=="-I"||arg=="--module-path"||arg=="--stdlib"||arg=="--prelude"||arg=="--stdppx"||arg=="--use-ppx"){
       if(++i>=argc||!argv[i][0])throw std::runtime_error(arg+" requires a nonempty path or name");
       if(arg=="--stdlib"){loader.stdlib=argv[i];loader.stdlib_paths.clear();loader.stdlib_explicit=true;}
       else if(arg=="--prelude")loader.preludes.emplace_back(argv[i]);
       else if(arg=="--stdppx"||arg=="--use-ppx")ppx.programs.push_back({{"<standard-ppx>"},argv[i],argv[i]});
       else import_paths.emplace_back(argv[i]);
      }
      else if(arg.starts_with("-I")&&arg.size()>2)import_paths.emplace_back(arg.substr(2));
      else if(arg=="--ir-input")ir_input=true;
       else if(arg=="--optimize-ir")legacy_optimize=true;
      else if(arg=="--opt-work"||arg=="--opt-rewrites"){
       if(++i>=argc)throw std::runtime_error(arg+" requires a value");
       auto& limit=arg=="--opt-work"?optimization.work:optimization.rewrites;
       std::string value=argv[i];auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),limit);
       if(ec!=std::errc{}||end!=value.data()+value.size())throw std::runtime_error("invalid optimization budget");
       opt_limit=true;
      }
      else if(arg=="--analysis-work"){
       if(++i>=argc)throw std::runtime_error("--analysis-work requires a value");
       std::string value=argv[i];auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),analysis_work);
       if(ec!=std::errc{}||end!=value.data()+value.size())throw std::runtime_error("invalid analysis work budget");
       analysis_limit=true;
      }
     else if(arg=="--ppx"||arg=="--ppx-schema"){
     if(i+2>=argc)throw std::runtime_error(arg+" requires LABEL and PATH");
     std::string label=argv[++i],path=argv[++i];
     if(arg=="--ppx")ppx.programs.push_back({{"<command-line>"},std::move(label),std::move(path)});
     else if(!ppx.schemas.emplace(std::move(label),std::move(path)).second)throw std::runtime_error("duplicate --ppx-schema label");
    }else if(arg=="--allow-source-ppx")ppx.allow_source=true;
    else if(arg=="--ppx-timeout-ms"){
     if(++i>=argc)throw std::runtime_error("--ppx-timeout-ms requires a value");
     std::string value=argv[i];auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),ppx.timeout_ms);
     if(ec!=std::errc{}||end!=value.data()+value.size()||!ppx.timeout_ms||ppx.timeout_ms>60000)throw std::runtime_error("PPX timeout must be 1..60000 ms");
    }
   else if(arg=="-o"){if(++i>=argc)throw std::runtime_error("-o needs a file");out=argv[i];}
   else if(!arg.empty()&&arg[0]=='-')throw std::runtime_error("unknown option "+arg);
    else if(source.empty())source=arg;else throw std::runtime_error("too many input files");
   }
   if(!cephyr_target.empty()&&!cephyr_object)
    throw std::runtime_error("--cephyr-target requires --emit-cephyr-object");
#ifdef ON1X_WITH_CEPHYR
   if(!cephyr_switches.empty()&&!cephyr_object)
    throw std::runtime_error("Cephyr options require --emit-cephyr-object");
#endif
   if(cephyr_object){
#ifndef ON1X_WITH_CEPHYR
    throw std::runtime_error("embedded Cephyr unavailable: configure with ON1X_WITH_CEPHYR=ON");
#else
    if(mode!="--emit-c"||explicit_c||ir_input||!sdk_mode.empty()||list_opts||loader.kernel_c11||loader.kernel_stage)
     throw std::runtime_error("--emit-cephyr-object requires checked source and cannot be combined with another emission mode");
    if(out.empty()||out=="-")throw std::runtime_error("--emit-cephyr-object requires -o FILE");
    if(cephyr_target.empty())cephyr_target="x86_64-linux-gnu";
    cephyr_options.sarif=cephyr_options.sarif||diagnostic_format=="sarif";
    if(!cephyr_options.facts.empty()||!cephyr_options.smtlib.empty()||!cephyr_sarif.empty()){
     namespace fs=std::filesystem;
     auto key=[](const fs::path& p){return fs::weakly_canonical(fs::absolute(p));};
     std::set<fs::path> paths{key(out)};
     if(!source.empty())paths.insert(key(source));
     for(const auto& file:cephyr_options.trusted_files)paths.insert(key(file));
     for(const auto& path:{fs::path(cephyr_sarif),cephyr_options.facts,cephyr_options.smtlib})
      if(!path.empty()&&!paths.insert(key(path)).second)
       throw std::runtime_error("Cephyr reports require distinct files separate from inputs and object output");
     if(!cephyr_options.factir&&(!cephyr_options.facts.empty()||!cephyr_options.smtlib.empty()))
      throw std::runtime_error("FactIR evidence requires --cephyr-factir on");
    }
#endif
   }
        if(!vm_prefixes.empty()&&mode!="--emit-vm")throw std::runtime_error("--vm-source-prefix requires --emit-vm");
         const bool dump_memgraph=mode=="--dump-memgraph";
         if(dump_memgraph&&!on1x::memgraph::available())
          throw std::runtime_error("MEMGraph unavailable: configure with ON1X_WITH_MEMGRAPH=ON and LibStone LLSynth");
         const auto policy=requested_opts.resolve(legacy_optimize||mode=="--dump-passes",dump_memgraph);
       if(list_opts){
        if(mode!="--emit-c"||explicit_c||!source.empty()||!sdk_mode.empty()||ir_input||opt_limit||
           !sdk_target.empty()||native_pic||native_no_entry||!plugin_selectors.empty()||
           memory||type_safety||!loader.interfaces.empty()||analysis_limit)
         throw std::runtime_error("--list-optimizations cannot be combined with an input or compilation mode");
        write_all(out,policy.listing());return 0;
       }
        const bool analysis_mode=mode=="--emit-ir-facts"||mode=="--analyze-ir";
        const bool vm_mode=mode=="--emit-vm"||mode=="--run-vm";
        const bool mir_mode=mode=="--emit-ir"||mode=="--emit-ir-c"||mode=="--ir-cfg"||
           mode=="--run-ir"||vm_mode||mode=="--dump-passes"||analysis_mode||!sdk_mode.empty();
       const bool checked_source=!ir_input&&mode!="--tokens"&&mode!="--emit-ast"&&mode!="--deps"&&
         mode!="--kernel-imports"&&mode!="--list-extensions"&&mode!="--explain-extension"&&
         mode!="--print-stdlib"&&mode!="--stdlib-resource";
         requested_opts.check_stages(policy,checked_source,
                                     checked_source&&(vm_mode||(!mir_mode&&(mode=="--emit-c"||dump_memgraph))),
                                    mir_mode&&!ir_input,mir_mode);
       optimization.pipeline=policy.pipeline;
       const bool optimize=mir_mode&&(!optimization.pipeline.empty()||mode=="--dump-passes");
       optimization.allow_empty=mode=="--dump-passes"&&optimization.pipeline.empty();
       if(legacy_optimize&&!mir_mode)throw std::runtime_error("--optimize-ir requires a MIR mode");
       if(opt_limit&&!optimize)throw std::runtime_error("optimization budgets require enabled MIR passes or --dump-passes");
             if(!sdk_target.empty()&&(sdk_mode!="--sdk-emit"||sdk_name!="mir-archon"||!plugin_selectors.empty()))
             throw std::runtime_error("--sdk-target requires --sdk-emit mir-archon without plugins");
            if((native_pic||native_no_entry)&&sdk_target.empty())
             throw std::runtime_error("--native-pic and --native-no-entry require target-aware native emission");
             if(!sdk_mode.empty()&&(mode!="--emit-c"||explicit_c||(legacy_optimize&&sdk_target.empty())||
              (sdk_mode=="--sdk-analyze"&&!out.empty())))
             throw std::runtime_error("SDK modes cannot be combined with other modes, non-target --optimize-ir, or analyzer output files");
#ifdef ON1X_WITH_EXTENSION_SDK
           if(!plugin_selectors.empty()&&sdk_mode.empty())
            throw std::runtime_error("--plugin requires --sdk-analyze or --sdk-emit");
#else
           if(!sdk_mode.empty()||!plugin_selectors.empty())
            throw std::runtime_error("SDK hooks unavailable: configure with ON1X_BUILD_EXTENSION_SDK=ON");
#endif
         if(!diagnostic_catalog_file.empty()){
          external_catalog=on1x::diagnostic::Catalog::load(diagnostic_catalog_file);
          diagnostic_catalog=&*external_catalog;
         }
         if(scrutiny_work_invalid)
          throw diagnostic_catalog->failure(on1x::diagnostic::Role::ScrutinyFailure,{"<command-line>"});
         color=diagnostic_color(color_mode,diagnostic_format);
         auto publish_diagnostics=[&]{
          if(diagnostics_published)return;
          report_registry(diagnostic_registry,*diagnostic_catalog,diagnostic_snapshots,diagnostic_format,color);
          diagnostics_published=true;
         };
         const bool interface_mode=mode=="--check-interfaces";
        if(interface_mode==loader.interfaces.empty())
         throw std::runtime_error("--check-interfaces and at least one --interface FILE must be used together");
        if(interface_mode&&(source.empty()||!out.empty()||ir_input||memory||type_safety||optimize||opt_limit||
           analysis_limit||!ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source||
           loader.kernel_c11||loader.kernel_stage||!loader.extensions.enabled.empty()))
         throw std::runtime_error("--check-interfaces requires plain source without output, PPX, declaration extensions or other checker options");
        if(mode=="--list-extensions"){write_all(out,on1x::extensions::describe(loader.extensions));return 0;}
       if(mode=="--explain-extension"){write_all(out,on1x::extensions::describe(loader.extensions,explain_extension));return 0;}
       loader.paths.insert(loader.paths.begin(),import_paths.begin(),import_paths.end());
       loader.stdlib_paths.insert(loader.stdlib_paths.begin(),stdlib_paths.begin(),stdlib_paths.end());
       if(mode=="--print-stdlib"||mode=="--stdlib-resource"){
        auto roots=on1x::standard_library_roots(loader);
        if(roots.empty())throw std::runtime_error("standard library root is not configured");
        if(mode=="--stdlib-resource"){
         auto relative=std::filesystem::path(stdlib_resource);
         if(stdlib_resource.size()>4096||relative.is_absolute()||std::find(relative.begin(),relative.end(),std::filesystem::path(".."))!=relative.end())
          throw std::runtime_error("standard-library resource requires a local relative path");
         for(const auto& root:roots)if(std::filesystem::is_regular_file(root/relative)){
          write_all(out,std::filesystem::canonical(root/relative).string()+"\n");return 0;
         }
         throw std::runtime_error("standard-library resource not found: "+stdlib_resource);
        }
        write_all(out,roots.front().string()+"\n");return 0;
       }
       if(source.empty()){usage();return 2;}
        loader.extensions.execute=mode!="--deps"&&mode!="--emit-ast"&&mode!="--repair"&&!interface_mode;
       loader.extensions.timeout_ms=ppx.timeout_ms;
       for(const auto& name:loader.extensions.enabled)if(loader.extensions.disabled.contains(name))throw on1x::Error({"<command-line>"},"ON1X-CAT-ENABLE: enabled/disabled conflict: "+name,"ON1X-CAT-ENABLE");
       if(mode=="--repair"){
#ifdef ON1X_WITH_SYREN
         if(ir_input||memory||type_safety||optimize||opt_limit||analysis_limit||
            !ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source){
          throw command_failure(CommandFailure::RepairOptions);
         }
         if(scrutiny_option)throw command_failure(CommandFailure::RepairPolicy);
          return repair_source(source,out,loader,policy.rules,*diagnostic_catalog,diagnostic_format,color);
#else
         throw command_failure(CommandFailure::RepairUnavailable);
#endif
       }
      ppx.paths=stdppx_paths;
      on1x::discovery::environment(ppx.paths,"ON1X_STDPPX_PATH");
       ppx.paths.insert(ppx.paths.end(),ppx_paths.begin(),ppx_paths.end());
       on1x::discovery::environment(ppx.paths,"ON1X_PPX_PATH");
       ppx.diagnostics=&diagnostic_registry;
       ppx.diagnostic_catalog=diagnostic_catalog;
      for(const auto& root:on1x::standard_library_roots(loader))ppx.paths.push_back(root.parent_path()/"stdppx");
      // Executable-relative native tools are an ordinary explicitly selected PPX
      // fallback, never an override of the pinned Category extension registry.
      if(!loader.extensions.directory.empty())ppx.paths.push_back(loader.extensions.directory);
     memory=memory||mode=="--check-memory";
     type_safety=type_safety||mode=="--check-types";
#ifndef ON1X_WITH_NEROTYPICAL
     if(type_safety)throw std::runtime_error("structural type checking unavailable: configure with ON1X_WITH_NEROTYPICAL=ON");
#endif
     if(type_safety&&(ir_input||mode=="--tokens"||mode=="--emit-ast"))throw std::runtime_error("structural type checking requires elaborated source HIR");
#ifndef ON1X_WITH_CHISEL
     if(memory)throw std::runtime_error("memory checking unavailable: configure with ON1X_WITH_CHISEL=ON");
#endif
     if(memory&&(ir_input||mode=="--tokens"||mode=="--emit-ast"))throw std::runtime_error("memory checking requires checked source HIR");
     if(!admission_files.empty()&&!memory)throw std::runtime_error("--admitted-extern requires --check-memory or --memory-check");
#ifdef ON1X_WITH_CHISEL
     std::map<std::string,on1x::safety::AdmittedExtern> admitted;
     if(memory)for(const auto& file:admission_files){
      std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("cannot read admitted-extern evidence: "+file);
      std::string text;char chunk[8192];
      while(in){in.read(chunk,sizeof chunk);auto n=static_cast<std::size_t>(in.gcount());
       if(n>4*1024*1024-text.size())throw std::runtime_error("admitted-extern evidence byte limit exceeded: "+file);
       text.append(chunk,n);
      }
      if(!in.eof())throw std::runtime_error("cannot read admitted-extern evidence: "+file);
      for(auto& [name,entry]:on1x::safety::parse_admission(text))
       if(!admitted.emplace(name,std::move(entry)).second)throw std::runtime_error("duplicate admitted extern across evidence files: "+name);
     }
#endif
     if(analysis_limit&&mode!="--analyze-ir")throw std::runtime_error("--analysis-work requires --analyze-ir");
#ifndef ON1X_WITH_NOVOSTONE
     if(analysis_mode)throw std::runtime_error("MIR analysis unavailable: configure with ON1X_WITH_NOVOSTONE=ON");
#endif
#ifdef ON1X_WITH_EXTENSION_SDK
       auto sdk_output=[&](const on1x::mir::Module& input){
        const bool emit=sdk_mode=="--sdk-emit";
        std::optional<on1x::mir::passes::Result> optimized;
        if(optimize){
         optimized=on1x::mir::passes::run(input,optimization);
         if(optimized->status()==on1x::mir::passes::Status::BudgetExhausted)
          std::cerr<<"on1xc: ON1X-MIR-PASS: "<<terminal_text(optimized->diagnostic())<<"; using last verified MIR prefix\n";
        }
        const auto& module=optimized?optimized->module():input;
        auto result=sdk_target.empty()
         ?on1x::sdk_driver::run(module,source,sdk_name,emit,diagnostic_registry,plugin_selectors)
         :on1x::sdk_driver::run_native(module,source,sdk_name,sdk_target,diagnostic_registry,plugin_selectors,
            {.pic=native_pic,.emit_c_entrypoint=!native_no_entry});
       publish_diagnostics();
       if(result.exit_code!=0)return result.exit_code;
       if(emit){
        if(out.empty()||out=="-"){
         if(!(std::cout<<result.artifact))throw std::runtime_error("cannot write SDK artifact to stdout");
        }else publish_regular_file(out,result.artifact,"SDK",".on1x-sdk-XXXXXX");
       }
       return 0;
      };
#endif
       if(loader.kernel_stage&&!loader.kernel_c11)throw std::runtime_error("--kernel-stage requires --kernel-c11");
       if(loader.kernel_c11){
        if(mode!="--emit-c"||!sdk_mode.empty()||ir_input||memory||type_safety||!ppx.programs.empty()||!ppx.schemas.empty()||
          ppx.allow_source||!loader.extensions.enabled.empty())
        throw std::runtime_error("--kernel-c11 requires direct C11 emission without PPX, extensions or independent checker modes");
       loader.extensions.execute=false;
      }
      if(ir_input){
       if(!loader.extensions.enabled.empty()||!loader.extensions.disabled.empty())throw std::runtime_error("--ir-input cannot enable source extensions");
       if(!mir_mode||!ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source)throw std::runtime_error("--ir-input requires a MIR mode without PPX options");
      }
#ifdef ON1X_WITH_EXTENSION_SDK
      // Configuration only; the SDK hook runs after source lowering or MIR verification.
      if(!sdk_mode.empty())on1x::sdk_driver::check_plugin_selection(plugin_selectors,sdk_name);
#endif
      if(ir_input){
        auto module=on1x::mir::deserialize(read_ir(source),source);
#ifdef ON1X_WITH_EXTENSION_SDK
       if(!sdk_mode.empty())return sdk_output(module);
#endif
        return mir_output(module,mode,out,analysis_work,optimize?&optimization:nullptr,vm_prefixes);
    }
   if((mode=="--tokens"||mode=="--emit-ast"||mode=="--kernel-imports")&&(!ppx.programs.empty()||!ppx.schemas.empty()))throw std::runtime_error("inspection modes cannot be combined with --ppx/--ppx-schema");
   if(mode=="--kernel-imports"){
    write_all(out,kernel_requests(on1x::inspect_kernel_imports(source,loader)));return 0;
   }
   if(mode=="--tokens"){
    auto tokens=on1x::scan(on1x::read_source_file(source),source);
   std::ostringstream listing;for(auto& t:tokens)listing<<t.where.line<<":"<<t.where.column<<"  "<<t.text<<"\n";
   write_all(out,listing.str());return 0;
  }
      auto loaded=on1x::load_unit(source,loader);
#ifdef ON1X_WITH_CEPHYR
      if(!cephyr_sarif.empty()){
       const auto dest=std::filesystem::weakly_canonical(std::filesystem::absolute(cephyr_sarif));
       for(const auto& path:loaded.dependencies)
        if(dest==std::filesystem::weakly_canonical(path))
         throw std::runtime_error("--cephyr-sarif would overwrite a loaded source file");
      }
#endif
      for(const auto& path:loaded.dependencies)try{
       if(diagnostic_snapshot_bytes==on1x::diagnostic::max_source_snapshot_bytes)break;
       auto snapshot=on1x::read_source_file(path.string());
       if(snapshot.size()>on1x::max_source_bytes||
          snapshot.size()>on1x::diagnostic::max_source_snapshot_bytes-diagnostic_snapshot_bytes)break;
       auto [stored,inserted]=diagnostic_snapshots.emplace(path.string(),std::move(snapshot));
       if(inserted)diagnostic_snapshot_bytes+=stored->second.size();
      }catch(const std::exception&){break;}
      diagnostic_sources_sealed=true;
    if(mode=="--deps"){
     std::ostringstream paths;for(const auto& path:loaded.dependencies)paths<<path.string()<<"\n";
     write_all(out,paths.str());return 0;
    }
       auto& unit=loaded.unit;
       if(interface_mode){
        on1x::analysis::Context context{*diagnostic_catalog,diagnostic_registry,scrutiny,{}};
         on1x::check_interfaces(std::move(unit),loaded.interfaces,context,policy.rules);
        if(diagnostic_registry.has_errors()){publish_diagnostics();return 1;}
        publish_diagnostics();
        std::cout<<"OK: "<<loaded.interfaces.size()<<" interface members\n";return 0;
       }
     auto extension_names=loader.extensions.enabled;
     for(const auto& activation:unit.features)extension_names.insert(activation.name);
     for(const auto& registration:ppx.programs)if(extension_names.contains(registration.label))
      throw on1x::Error(registration.where,"ON1X-CAT-ENABLE: ordinary PPX label conflicts with active bundled extension: "+registration.label,"ON1X-CAT-ENABLE");
     for(const auto& registration:unit.ppx)if(extension_names.contains(registration.label))
      throw on1x::Error(registration.where,"ON1X-CAT-ENABLE: source PPX label conflicts with active bundled extension: "+registration.label,"ON1X-CAT-ENABLE");
   if(mode=="--emit-ast"){
    on1x::expand_macros(unit);auto tree=on1x::meta::encode(unit);on1x::meta::base_schema().validate(tree);
    write_all(out,on1x::meta::write_sexpr(tree));return 0;
    }
    on1x::meta::rewrite(unit,ppx);
     on1x::analysis::Context analysis_context{*diagnostic_catalog,diagnostic_registry,scrutiny,{}};
      on1x::processor::Report processor_report;
       on1x::check(unit,analysis_context,policy.rules,mode=="--dump-processor"?&processor_report:nullptr);
     if(diagnostic_registry.has_errors()){publish_diagnostics();return 1;}
     auto category_plan=on1x::extensions::evidence_plan(unit);
    std::string type_evidence;
#ifdef ON1X_WITH_NEROTYPICAL
    if(type_safety)type_evidence=on1x::type_safety::verify(unit);
#endif
#ifdef ON1X_WITH_CHISEL
     if(memory){auto evidence=on1x::safety::check(unit,{},admitted);if(mode=="--check-memory"){publish_diagnostics();write_all(out,evidence);return 0;}}
#endif
      if(mode=="--check-types"){publish_diagnostics();write_all(out,type_evidence);return 0;}
      if(mode=="--dump-processor"){
       publish_diagnostics();
       auto text=on1x::processor::report(processor_report);
       if(out.empty()||out=="-")write_all(out,text);
       else publish_regular_file(out,text,"processor",".on1x-processor-XXXXXX");
       return 0;
      }
     if(mode=="--emit-category-plan"){publish_diagnostics();write_all(out,category_plan);return 0;}
      if(mode=="--check"){publish_diagnostics();std::cout<<"OK: "<<unit.functions.size()<<" functions\n";return 0;}
       if(mir_mode){
           auto module=vm_mode
             ?on1x::hir::lower_vm(unit,{.decision_ssa=policy.decision_ssa},policy.memgraph_bounds)
            :on1x::mir::lower(unit,{.decision_ssa=policy.decision_ssa});
#ifdef ON1X_WITH_EXTENSION_SDK
        if(!sdk_mode.empty())return sdk_output(module);
#endif
         auto result=mir_output(module,mode,out,analysis_work,optimize?&optimization:nullptr,vm_prefixes);
       publish_diagnostics();return result;
      }
   if(mode=="--types"){
    std::ostringstream listing;
     for(const auto& function:unit.functions){
       if(function.name.starts_with("@")||loaded.internal_functions.contains(function.name))continue;
      std::vector<on1x::Type> parameters;for(const auto& parameter:function.params)parameters.push_back(parameter.type);
      auto signature=on1x::Type::function(std::move(parameters),function.result);
      std::map<std::size_t,std::size_t> variables;
      auto normalize=[&](auto&& self,const on1x::Type& type)->on1x::Type{
       if(type.kind()==on1x::Type::Generic){
        auto [variable,inserted]=variables.emplace(type.generic_identity(),variables.size());(void)inserted;
        return on1x::Type::bound(variable->second);
       }
       if(type.is_code())return on1x::Type::code(self(self,type.code_result()));
       if(type.is_function()){
        std::vector<on1x::Type> args;for(const auto& p:type.parameters())args.push_back(self(self,p));
        return on1x::Type::function(std::move(args),self(self,type.result()));
       }
        if(type.is_tuple()){
        std::vector<on1x::Type> elements;for(const auto& p:type.elements())elements.push_back(self(self,p));
        return on1x::Type::tuple(std::move(elements));
        }
         if(type.is_data()){
          std::vector<on1x::Type> elements;for(const auto& p:type.elements())elements.push_back(self(self,p));
          auto result=on1x::Type::data(type.kind(),std::move(elements),type.labels(),type.extent());
          if(!type.nominal().empty()){std::vector<on1x::Type> args;for(const auto& a:type.arguments())args.push_back(self(self,a));result=result.with_nominal(type.nominal(),std::move(args));}return result;
        }
       return type;
      };
      signature=normalize(normalize,signature);
      listing<<(function.external?"extern ":"")<<function.name<<" : ";
      if(!variables.empty()){
       listing<<"forall(";for(std::size_t i=0;i<variables.size();++i){if(i)listing<<", ";listing<<"a"<<i;}listing<<") ";
      }
       if(on1x::takes_borrows(function)){
        listing<<"fn(";
        for(std::size_t i=0;i<function.params.size();++i){
         if(i)listing<<", ";
         if(function.params[i].borrow==on1x::BorrowMode::Shared)listing<<"borrow ";
         else if(function.params[i].borrow==on1x::BorrowMode::Exclusive)
          listing<<"ref ";
         listing<<on1x::type_name(signature.parameters()[i]);
       }
       listing<<") -> "<<on1x::type_name(signature.result())<<"\n";
      }else listing<<on1x::type_name(signature)<<"\n";
    }
     publish_diagnostics();write_all(out,listing.str());return 0;
    }
     std::string bounds_report;
     on1x::C11Options c11_options{policy.memgraph_bounds,dump_memgraph?&bounds_report:nullptr};
     c11_options.allow_stackless_timers=!cephyr_object;
#ifdef ON1X_WITH_CEPHYR
     std::map<std::size_t,on1x::Location> c11_origins;
     if(cephyr_options.sarif)c11_options.origins=&c11_origins;
#endif
 #ifdef ON1X_WITH_EXTENSION_SDK
     auto generated=on1x::backends::run_c11(unit,source,c11_options).bytes;
 #else
     auto generated=on1x::emit_c(unit,source,c11_options);
 #endif
     if(dump_memgraph){
      publish_diagnostics();
      if(out.empty()||out=="-")write_all(out,bounds_report);
      else publish_regular_file(out,bounds_report,"MEMGraph",".on1x-memgraph-XXXXXX");
      return 0;
     }
#ifdef ON1X_WITH_CEPHYR
     if(cephyr_object){
      namespace fs=std::filesystem;
      auto root=fs::canonical("/proc/self/exe").parent_path()/"../share/on1x";
      auto config_dir=root;
      if(!fs::is_directory(root)){
       root=fs::path(ON1X_DRIVER_SOURCE_TOOLS).parent_path();
       config_dir=ON1X_CEPHYR_BUILD_CONFIG_DIR;
      }
      auto config=config_dir/(cephyr_options.blackstone?"cephyr-blackstone.toml":"cephyr-baseline.toml");
      auto result=on1x::cephyr::compile_object({generated,source+".c",cephyr_target,
       ON1X_CEPHYR_RESOURCE_ROOT,ON1X_CEPHYR_ARCHON_ROOT,config,{root/"runtime"},cephyr_options});
      using Status=on1x::cephyr::Status;
      if(cephyr_options.sarif&&(result.status==Status::ok||result.status==Status::compile_error)){
       std::string frontend;
#ifdef ON1X_WITH_SYREN
       if(diagnostic_format=="sarif")
        frontend=on1x::diagnostics::sarif(diagnostic_bundle(diagnostic_registry,*diagnostic_catalog,diagnostic_snapshots));
#endif
       auto report=on1x::cephyr::source_sarif(result.diagnostics,source+".c",c11_origins,diagnostic_snapshots,frontend);
       if(!cephyr_sarif.empty())publish_regular_file(cephyr_sarif,report,"Cephyr SARIF",".on1x-sarif-XXXXXX");
       if(!frontend.empty()){
        std::cerr<<report;diagnostics_published=true;
       }else publish_diagnostics();
       if(result.status==Status::compile_error){
        if(frontend.empty())std::cerr<<"on1xc: Cephyr compile error (see "<<terminal_text(cephyr_sarif)<<")\n";
        return 1;
       }
      }else publish_diagnostics();
      if(result.status!=Status::ok){
       const char* kind=result.status==Status::compile_error?"compile error":
        result.status==Status::invalid_argument?"invalid request":
        result.status==Status::unsupported_target?"unsupported target":"internal error";
       std::cerr<<"on1xc: Cephyr "<<kind<<": "<<terminal_text(result.diagnostics)<<'\n';
       return result.status==Status::compile_error?1:2;
      }
      if(!cephyr_options.sarif&&!result.diagnostics.empty())std::cerr<<terminal_text(result.diagnostics)<<'\n';
      publish_regular_file(out,result.object,"Cephyr",".on1x-cephyr-XXXXXX");return 0;
     }
#endif
     publish_diagnostics();write_all(out,generated);return 0;
#ifdef ON1X_WITH_NOVOSTONE
 }catch(const on1x::mir::relational::BudgetExceeded& e){
   std::cerr<<terminal_text(e.where.file)<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<terminal_text(e.what())<<"\n";return 75;
#endif
#ifdef ON1X_WITH_CHISEL
  }catch(const on1x::safety::BudgetExceeded& e){
    std::cerr<<terminal_text(e.where.file)<<":"<<e.where.line<<":"<<e.where.column<<": "<<terminal_text(e.what())<<"\n";return 75;
#endif
#ifdef ON1X_WITH_NEROTYPICAL
  }catch(const on1x::type_safety::Inconclusive& e){
    std::cerr<<terminal_text(e.where.file)<<":"<<e.where.line<<":"<<e.where.column<<": "<<terminal_text(e.what())<<"\n";return 75;
#endif
    }catch(const on1x::Error& e){
 #ifdef ON1X_WITH_SYREN
    try{
     const auto& active=diagnostic_catalog?*diagnostic_catalog:on1x::diagnostic::Catalog::embedded();
      report_error(e,diagnostic_registry,active,diagnostic_snapshots,
       diagnostic_format,color,!diagnostics_published,!diagnostic_sources_sealed);
    }catch(const std::exception&){
      const auto& active=on1x::diagnostic::Catalog::embedded();
      on1x::diagnostic::Registry fallback;(void)fallback.emit(active.record(e));
      std::cerr<<on1x::diagnostic::text(fallback,active,color);
    }
    return 1;
 #else
    if(!diagnostics_published)report_registry(diagnostic_registry,*diagnostic_catalog,diagnostic_snapshots,diagnostic_format,color);
    on1x::diagnostic::Registry fallback;
    (void)fallback.emit(diagnostic_catalog->record(e));
    std::cerr<<on1x::diagnostic::text(fallback,*diagnostic_catalog,color);return 1;
 #endif
  }catch(const std::exception&e){
   if(!diagnostics_published&&diagnostic_catalog)
    report_registry(diagnostic_registry,*diagnostic_catalog,diagnostic_snapshots,diagnostic_format,color);
   std::cerr<<"on1xc: "<<terminal_text(e.what())<<"\n";return 2;
  }
 }

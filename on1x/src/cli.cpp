#include <on1x/ast.hpp>
#include <on1x/meta.hpp>
#include <on1x/loader.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/backend/passes.hpp>
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
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
namespace {
#ifdef ON1X_WITH_SYREN
void report_error(const on1x::Error& error,const std::string& format){
 std::optional<std::string> snapshot;
 try{snapshot=on1x::read_source_file(error.where.file);}catch(const std::exception&){}
 auto report=on1x::diagnostics::describe(error,std::move(snapshot));
 std::cerr<<(format=="json"?on1x::diagnostics::json(report):format=="sarif"?on1x::diagnostics::sarif(report):on1x::diagnostics::text(report));
}
int repair_source(const std::string& source,const std::string& out,const on1x::LoaderOptions& options,const std::string& format){
 namespace fs=std::filesystem;
 if(out.empty()||out=="-")throw std::runtime_error("--repair requires -o NEW_FILE (never edits input files)");
 // Refuse all existing destinations, including symlinks and imported sources.
 if(fs::symlink_status(out).type()!=fs::file_type::not_found)throw std::runtime_error("repair output must not already exist");
 auto path=fs::canonical(source);
 auto original=on1x::read_source_file(path.string());
 auto compile=[&](std::string_view text){
  auto overlay=options;overlay.source_overrides[path]=std::string(text);
  auto loaded=on1x::load_unit(path,overlay);
  if(!loaded.unit.ppx.empty())throw std::runtime_error("repair validation never executes source PPX programs");
  on1x::check(loaded.unit);
  (void)on1x::emit_c(loaded.unit,path.string());
 };
 try{compile(original);std::cerr<<"on1xc: source is already valid; no repair output written\n";return 0;}
 catch(const on1x::Error& error){
  if(error.where.file!=path.string())throw; // Do not misapply imported-file offsets.
  auto report=on1x::diagnostics::describe(error,original);
  on1x::diagnostics::validate(report,[&](std::string_view changed){
   using namespace moonstone;
   try{compile(changed);return ProofResult{Status::Proven,"reloaded, parsed, type checked and C-emitted; intended behavior is not proven",{},1};}
   catch(const on1x::Error& e){
    // Existing frontend limits are exhaustion, not evidence against a repair.
    auto message=std::string(e.what());
    auto status=message.find("limit")!=std::string::npos||message.find("too many")!=std::string::npos?Status::BudgetExhausted:Status::Refuted;
    return ProofResult{status,e.where.file+":"+std::to_string(e.where.line)+": "+message,{},1};
   }catch(const std::exception& e){return ProofResult{Status::Unknown,e.what(),{},1};}
  });
  std::cerr<<(format=="json"?on1x::diagnostics::json(report):format=="sarif"?on1x::diagnostics::sarif(report):on1x::diagnostics::text(report));
  std::optional<std::string> proposal;
  for(const auto& candidate:report.repairs)if(candidate.status==moonstone::syren::RepairStatus::Validated){
   auto changed=moonstone::apply_edits(original,candidate.edits);
   if(proposal&&proposal!=changed){std::cerr<<"on1xc: ambiguous repairs; no output written\n";return 1;}
   proposal=std::move(changed);
  }
  if(!proposal)return 1;
  // Recheck before publication; compilation and validation never write sources.
  if(fs::symlink_status(out).type()!=fs::file_type::not_found)throw std::runtime_error("repair output must not already exist");
  auto* output=std::fopen(out.c_str(),"wbx"); // C11 exclusive creation, including against symlink races.
  if(!output)throw std::runtime_error("cannot exclusively create repair proposal "+out);
  bool written=std::fwrite(proposal->data(),1,proposal->size(),output)==proposal->size();
  if(std::fclose(output)!=0)written=false;
  if(!written)throw std::runtime_error("cannot write repair proposal "+out);
  return 0;
 }
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
int mir_output(const on1x::mir::Module& input,const std::string& mode,const std::string& out,std::size_t analysis_work,const on1x::mir::passes::Options* optimization){
 std::optional<on1x::mir::passes::Result> optimized;
 if(optimization){
  optimized=on1x::mir::passes::run(input,*optimization);
  if(optimized->status()==on1x::mir::passes::Status::BudgetExhausted)
   std::cerr<<"on1xc: ON1X-MIR-PASS: "<<optimized->diagnostic()<<"; using last verified MIR prefix\n";
 }
 if(mode=="--dump-passes"){write_all(out,on1x::mir::passes::report(*optimized));return 0;}
 const auto& module=optimized?optimized->module():input;
#ifdef ON1X_WITH_NOVOSTONE
 if(mode=="--emit-ir-facts"||mode=="--analyze-ir"){
  namespace r=on1x::mir::relational;r::Store store(module);
  novostone::datalog::Limits limits;limits.work=analysis_work;
  write_all(out,mode=="--emit-ir-facts"?r::dump_facts(store.snapshot()):r::report(store.snapshot(),limits));return 0;
 }
#else
 (void)analysis_work;
#endif
  if(mode=="--run-ir"){
   for(const auto& f:module.functions)if(f.name=="main"&&(!f.blocks.front().parameters.empty()||(f.result!=on1x::Type::I64&&f.result!=on1x::Type::Void)))
    throw on1x::Error(f.where,"ON1X-MIR-EVAL: main must have no parameters and return i64 or void");
  auto result=on1x::mir::evaluate(module);write_all(out,result.output);
  if(result.status!=on1x::mir::Status::Returned){
   std::cerr<<result.where.file<<":"<<result.where.line<<":"<<result.where.column<<": ON1X-MIR-EVAL: "<<result.diagnostic<<"\n";
   return result.status==on1x::mir::Status::Trapped?70:75;
  }
  return static_cast<int>(result.value);
 }
 write_all(out,mode=="--emit-ir"?on1x::mir::serialize(module):mode=="--ir-cfg"?on1x::mir::cfg_dot(module):on1x::mir::emit_c(module));return 0;
}
void usage(){std::cout<<"on1xc - a small strict ML-family frontend targeting ISO C11\n"
 "usage: on1xc [--check | --emit-c | --tokens | --types | --emit-ast] INPUT.on1x [-o OUTPUT]\n"
 "       [--ppx LABEL PROGRAM] [--ppx-schema LABEL FILE] [--allow-source-ppx]\n"
  "       [--ppx-timeout-ms MILLISECONDS]\n"
  "       [-I DIRECTORY | --module-path DIRECTORY] [--stdlib DIRECTORY]\n"
  "       [--no-prelude] [--prelude FILE] [--stdppx PROCESSOR]\n"
  "       on1xc --deps INPUT.on1x [-o OUTPUT] (dependency paths, no PPX execution)\n"
  "       on1xc [--emit-ir | --emit-ir-c | --ir-cfg | --run-ir] INPUT [-o OUTPUT] [--ir-input]\n"
  "       on1xc [--emit-ir-facts | --analyze-ir] INPUT [-o OUTPUT] [--ir-input]\n"
  "       [--analysis-work UNITS] (Novostone opt-in build; --analyze-ir only)\n"
  "       [--optimize-ir] [--opt-work UNITS] [--opt-rewrites COUNT] (MIR modes only)\n"
  "       on1xc --dump-passes INPUT [-o OUTPUT] [--ir-input]\n"
  "       on1xc --check-memory INPUT [-o OUTPUT] (Chisel opt-in build)\n"
  "       [--memory-check] (check resource usage before source compilation)\n"
  "       on1xc --check-types INPUT [-o OUTPUT] (Nerotypical opt-in build)\n"
   "       [--verify-types] (require independent structural HIR type verification)\n"
   "       [--diagnostics text|json|sarif] (Syren opt-in build; errors on stderr)\n"
   "       on1xc --repair INPUT.on1x -o NEW_FILE (Syren; reviewable proposal only)\n"
 "       on1xc --help\n"
  "--emit-ast expands macros and exports pre-PPX AST; it never executes programs.\n"
  "MIR modes support i64/bool/void, direct On1x calls, control flow and scalar print/assert.\n"
  "--run-ir is a bounded reference evaluator, not a native or VM backend.\n"
  "--ir-input reads verified serialized MIR instead of On1x source (MIR modes only).\n"
 "Output defaults to stdout. Use tools/on1x-build.py to link a native executable.\n";}
}
int main(int argc,char**argv){
 std::string diagnostic_format="text";
 try{
     std::string mode="--emit-c",source,out;bool ir_input=false;
     std::size_t analysis_work=10000000;bool analysis_limit=false;
      on1x::mir::passes::Options optimization;bool optimize=false,opt_limit=false;
      bool memory=false;
      bool type_safety=false;
    on1x::meta::PpxOptions ppx;
    auto loader=on1x::default_loader_options(argv[0]);
    std::vector<std::filesystem::path> import_paths;
  for(int i=1;i<argc;++i){std::string arg=argv[i];
     if(arg=="--help"||arg=="-h"){usage();return 0;}
     if(arg=="--repair"){mode=arg;continue;}
     if(arg=="--diagnostics"){
      if(++i>=argc)throw std::runtime_error("--diagnostics requires text, json or sarif");
      diagnostic_format=argv[i];
      if(diagnostic_format!="text"&&diagnostic_format!="json"&&diagnostic_format!="sarif")throw std::runtime_error("unknown diagnostic format "+diagnostic_format);
#ifndef ON1X_WITH_SYREN
      throw std::runtime_error("Syren diagnostics unavailable: configure with ON1X_WITH_SYREN=ON");
#endif
      continue;
     }
    if(arg=="--memory-check"){memory=true;continue;}
    if(arg=="--check-memory"){mode=arg;continue;}
    if(arg=="--verify-types"){type_safety=true;continue;}
    if(arg=="--check-types"){mode=arg;continue;}
     if(arg=="--check"||arg=="--emit-c"||arg=="--tokens"||arg=="--types"||arg=="--emit-ast"||arg=="--deps"||arg=="--emit-ir"||arg=="--emit-ir-c"||arg=="--ir-cfg"||arg=="--run-ir"||arg=="--emit-ir-facts"||arg=="--analyze-ir"||arg=="--dump-passes")mode=arg;
      else if(arg=="--no-prelude")loader.prelude=false;
      else if(arg=="-I"||arg=="--module-path"||arg=="--stdlib"||arg=="--prelude"||arg=="--stdppx"){
       if(++i>=argc||!argv[i][0])throw std::runtime_error(arg+" requires a nonempty path or name");
       if(arg=="--stdlib")loader.stdlib=argv[i];
       else if(arg=="--prelude")loader.preludes.emplace_back(argv[i]);
       else if(arg=="--stdppx")ppx.programs.push_back({{"<standard-ppx>"},argv[i],argv[i]});
       else import_paths.emplace_back(argv[i]);
      }
      else if(arg.starts_with("-I")&&arg.size()>2)import_paths.emplace_back(arg.substr(2));
      else if(arg=="--ir-input")ir_input=true;
      else if(arg=="--optimize-ir")optimize=true;
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
      if(source.empty()){usage();return 2;}
       loader.paths.insert(loader.paths.begin(),import_paths.begin(),import_paths.end());
       if(mode=="--repair"){
#ifdef ON1X_WITH_SYREN
        if(ir_input||memory||type_safety||optimize||opt_limit||analysis_limit||!ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source)
         throw std::runtime_error("--repair requires plain source without PPX, analysis or optimization options");
        return repair_source(source,out,loader,diagnostic_format);
#else
        throw std::runtime_error("repair unavailable: configure with ON1X_WITH_SYREN=ON");
#endif
       }
      for(auto& registration:ppx.programs){
       if(registration.where.file!="<standard-ppx>")continue;
       const auto& processor=registration.label;
       if(processor.empty()||processor.size()>128||processor.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
        throw std::runtime_error("invalid standard PPX name: "+processor);
       auto program=std::filesystem::absolute(loader.stdlib).parent_path()/"stdppx"/(processor+".py");
       if(!std::filesystem::is_regular_file(program))throw std::runtime_error("standard PPX not found: "+processor);
       registration.program=std::filesystem::absolute(program).string();
      }
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
     bool analysis_mode=mode=="--emit-ir-facts"||mode=="--analyze-ir";
     if(analysis_limit&&mode!="--analyze-ir")throw std::runtime_error("--analysis-work requires --analyze-ir");
#ifndef ON1X_WITH_NOVOSTONE
     if(analysis_mode)throw std::runtime_error("MIR analysis unavailable: configure with ON1X_WITH_NOVOSTONE=ON");
#endif
     bool mir_mode=mode=="--emit-ir"||mode=="--emit-ir-c"||mode=="--ir-cfg"||mode=="--run-ir"||mode=="--dump-passes"||analysis_mode;
     optimize=optimize||mode=="--dump-passes";
     if(optimize&&!mir_mode)throw std::runtime_error("--optimize-ir requires a MIR mode");
     if(opt_limit&&!optimize)throw std::runtime_error("optimization budgets require --optimize-ir or --dump-passes");
    if(ir_input){
     if(!mir_mode||!ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source)throw std::runtime_error("--ir-input requires a MIR mode without PPX options");
      return mir_output(on1x::mir::deserialize(read_ir(source),source),mode,out,analysis_work,optimize?&optimization:nullptr);
    }
   if((mode=="--tokens"||mode=="--emit-ast")&&(!ppx.programs.empty()||!ppx.schemas.empty()))throw std::runtime_error("inspection modes cannot be combined with --ppx/--ppx-schema");
   if(mode=="--tokens"){
    auto tokens=on1x::scan(on1x::read_source_file(source),source);
   std::ostringstream listing;for(auto& t:tokens)listing<<t.where.line<<":"<<t.where.column<<"  "<<t.text<<"\n";
   write_all(out,listing.str());return 0;
  }
    auto loaded=on1x::load_unit(source,loader);
    if(mode=="--deps"){
     std::ostringstream paths;for(const auto& path:loaded.dependencies)paths<<path.string()<<"\n";
     write_all(out,paths.str());return 0;
    }
    auto& unit=loaded.unit;
   if(mode=="--emit-ast"){
    on1x::expand_macros(unit);auto tree=on1x::meta::encode(unit);on1x::meta::base_schema().validate(tree);
    write_all(out,on1x::meta::write_sexpr(tree));return 0;
   }
   on1x::meta::rewrite(unit,ppx);
    on1x::check(unit);
    std::string type_evidence;
#ifdef ON1X_WITH_NEROTYPICAL
    if(type_safety)type_evidence=on1x::type_safety::verify(unit);
#endif
#ifdef ON1X_WITH_CHISEL
    if(memory){auto evidence=on1x::safety::check(unit);if(mode=="--check-memory"){write_all(out,evidence);return 0;}}
#endif
    if(mode=="--check-types"){write_all(out,type_evidence);return 0;}
     if(mode=="--check"){std::cout<<"OK: "<<unit.functions.size()<<" functions\n";return 0;}
     if(mir_mode)return mir_output(on1x::mir::lower(unit),mode,out,analysis_work,optimize?&optimization:nullptr);
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
       return type;
      };
      signature=normalize(normalize,signature);
      listing<<(function.external?"extern ":"")<<function.name<<" : ";
      if(!variables.empty()){
       listing<<"forall(";for(std::size_t i=0;i<variables.size();++i){if(i)listing<<", ";listing<<"a"<<i;}listing<<") ";
      }
      listing<<on1x::type_name(signature)<<"\n";
    }
    write_all(out,listing.str());return 0;
   }
  write_all(out,on1x::emit_c(unit,source));return 0;
#ifdef ON1X_WITH_NOVOSTONE
 }catch(const on1x::mir::relational::BudgetExceeded& e){
  std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<e.what()<<"\n";return 75;
#endif
#ifdef ON1X_WITH_CHISEL
  }catch(const on1x::safety::BudgetExceeded& e){
   std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": "<<e.what()<<"\n";return 75;
#endif
#ifdef ON1X_WITH_NEROTYPICAL
  }catch(const on1x::type_safety::Inconclusive& e){
   std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": "<<e.what()<<"\n";return 75;
#endif
   }catch(const on1x::Error& e){
#ifdef ON1X_WITH_SYREN
   try{report_error(e,diagnostic_format);}catch(const std::exception&){std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<e.what()<<"\n";}
   return 1;
#else
   std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<e.what()<<"\n";return 1;
#endif
 }catch(const std::exception&e){std::cerr<<"on1xc: "<<e.what()<<"\n";return 2;}
}

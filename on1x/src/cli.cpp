#include <on1x/ast.hpp>
#include <on1x/meta.hpp>
#include <on1x/backend/mir.hpp>
#ifdef ON1X_WITH_NOVOSTONE
#include <on1x/backend/relational.hpp>
#endif
#include <charconv>
#include <fstream>
#include <filesystem>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
namespace {
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
int mir_output(const on1x::mir::Module& module,const std::string& mode,const std::string& out,std::size_t analysis_work){
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
  auto result=on1x::mir::evaluate(module);write_all(out,result.output);
  if(result.status!=on1x::mir::Status::Returned){
   std::cerr<<result.where.file<<":"<<result.where.line<<":"<<result.where.column<<": ON1X-MIR-EVAL: "<<result.diagnostic<<"\n";
   return result.status==on1x::mir::Status::Trapped?70:75;
  }
  return static_cast<int>(result.value);
 }
 write_all(out,mode=="--emit-ir"?on1x::mir::serialize(module):mode=="--ir-cfg"?on1x::mir::cfg_dot(module):on1x::mir::emit_c(module));return 0;
}
on1x::Unit parse_with_imports(const std::filesystem::path& filename,
                              std::set<std::filesystem::path>& active,
                              std::set<std::filesystem::path>& visited,
                              int depth){
 if(depth>64)throw std::runtime_error("import recursion limit exceeded");
 auto resolved=std::filesystem::absolute(filename).lexically_normal();
 if(active.contains(resolved))throw std::runtime_error("import cycle: "+resolved.string());
 if(visited.contains(resolved))return {};
 active.insert(resolved);
 auto tokens=on1x::scan(on1x::read_source_file(resolved.string()),resolved.string());
 auto current=on1x::parse(tokens);
 on1x::Unit out;
 for(const auto& name:current.imports){
  auto child=parse_with_imports(resolved.parent_path()/name,active,visited,depth+1);
   for(auto& fn:child.functions)out.functions.push_back(std::move(fn));
   for(auto& macro:child.macros)out.macros.push_back(std::move(macro));
   for(auto& ppx:child.ppx)out.ppx.push_back(std::move(ppx));
 }
  for(auto& fn:current.functions)out.functions.push_back(std::move(fn));
  for(auto& macro:current.macros)out.macros.push_back(std::move(macro));
  for(auto& ppx:current.ppx)out.ppx.push_back(std::move(ppx));
 active.erase(resolved);visited.insert(resolved);
 return out;
}
void usage(){std::cout<<"on1xc - a small strict ML-family frontend targeting ISO C11\n"
 "usage: on1xc [--check | --emit-c | --tokens | --types | --emit-ast] INPUT.on1x [-o OUTPUT]\n"
 "       [--ppx LABEL PROGRAM] [--ppx-schema LABEL FILE] [--allow-source-ppx]\n"
  "       [--ppx-timeout-ms MILLISECONDS]\n"
  "       on1xc [--emit-ir | --emit-ir-c | --ir-cfg | --run-ir] INPUT [-o OUTPUT] [--ir-input]\n"
  "       on1xc [--emit-ir-facts | --analyze-ir] INPUT [-o OUTPUT] [--ir-input]\n"
  "       [--analysis-work UNITS] (Novostone opt-in build; --analyze-ir only)\n"
 "       on1xc --help\n"
  "--emit-ast expands macros and exports pre-PPX AST; it never executes programs.\n"
  "MIR modes support i64/bool/void, direct On1x calls, control flow and scalar print/assert.\n"
  "--run-ir is a bounded reference evaluator, not a native or VM backend.\n"
  "--ir-input reads verified serialized MIR instead of On1x source (MIR modes only).\n"
 "Output defaults to stdout. Use tools/on1x-build.py to link a native executable.\n";}
}
int main(int argc,char**argv){
 try{
     std::string mode="--emit-c",source,out;bool ir_input=false;
     std::size_t analysis_work=10000000;bool analysis_limit=false;
   on1x::meta::PpxOptions ppx;
  for(int i=1;i<argc;++i){std::string arg=argv[i];
   if(arg=="--help"||arg=="-h"){usage();return 0;}
     if(arg=="--check"||arg=="--emit-c"||arg=="--tokens"||arg=="--types"||arg=="--emit-ast"||arg=="--emit-ir"||arg=="--emit-ir-c"||arg=="--ir-cfg"||arg=="--run-ir"||arg=="--emit-ir-facts"||arg=="--analyze-ir")mode=arg;
      else if(arg=="--ir-input")ir_input=true;
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
     bool analysis_mode=mode=="--emit-ir-facts"||mode=="--analyze-ir";
     if(analysis_limit&&mode!="--analyze-ir")throw std::runtime_error("--analysis-work requires --analyze-ir");
#ifndef ON1X_WITH_NOVOSTONE
     if(analysis_mode)throw std::runtime_error("MIR analysis unavailable: configure with ON1X_WITH_NOVOSTONE=ON");
#endif
     bool mir_mode=mode=="--emit-ir"||mode=="--emit-ir-c"||mode=="--ir-cfg"||mode=="--run-ir"||analysis_mode;
    if(ir_input){
     if(!mir_mode||!ppx.programs.empty()||!ppx.schemas.empty()||ppx.allow_source)throw std::runtime_error("--ir-input requires a MIR mode without PPX options");
      return mir_output(on1x::mir::deserialize(read_ir(source),source),mode,out,analysis_work);
    }
   if((mode=="--tokens"||mode=="--emit-ast")&&(!ppx.programs.empty()||!ppx.schemas.empty()))throw std::runtime_error("inspection modes cannot be combined with --ppx/--ppx-schema");
   if(mode=="--tokens"){
    auto tokens=on1x::scan(on1x::read_source_file(source),source);
   std::ostringstream listing;for(auto& t:tokens)listing<<t.where.line<<":"<<t.where.column<<"  "<<t.text<<"\n";
   write_all(out,listing.str());return 0;
  }
  std::set<std::filesystem::path> active,visited;
   auto unit=parse_with_imports(source,active,visited,0);
   if(mode=="--emit-ast"){
    on1x::expand_macros(unit);auto tree=on1x::meta::encode(unit);on1x::meta::base_schema().validate(tree);
    write_all(out,on1x::meta::write_sexpr(tree));return 0;
   }
   on1x::meta::rewrite(unit,ppx);
   on1x::check(unit);
    if(mode=="--check"){std::cout<<"OK: "<<unit.functions.size()<<" functions\n";return 0;}
     if(mir_mode)return mir_output(on1x::mir::lower(unit),mode,out,analysis_work);
   if(mode=="--types"){
    std::ostringstream listing;
     for(const auto& function:unit.functions){
      if(function.name.starts_with("@"))continue;
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
 }catch(const on1x::Error& e){
  std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<e.what()<<"\n";return 1;
 }catch(const std::exception&e){std::cerr<<"on1xc: "<<e.what()<<"\n";return 2;}
}

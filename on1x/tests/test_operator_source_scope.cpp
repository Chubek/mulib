#include <on1x/loader.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs=std::filesystem;
using namespace on1x;

void write(const fs::path& path,const std::string& bytes){
 std::ofstream file(path,std::ios::binary|std::ios::trunc);
 if(!(file<<bytes))throw std::runtime_error("could not write "+path.string());
}

const Expr& application(const Unit& unit){
 if(unit.functions.size()!=1||unit.functions.front().body.size()!=1||
    !unit.functions.front().body.front().value)
  throw std::runtime_error("source query returned dependency bodies or ignored the entry snapshot");
 const auto& expr=*unit.functions.front().body.front().value;
 if(expr.kind!=Expr::Kind::OperatorBinary||expr.text!="%%"||expr.children.size()!=2||
    expr.operator_identity.empty())
  throw std::runtime_error("source query did not resolve the operator expression");
 return expr;
}

int run(const fs::path& root){
 fs::create_directories(root/"std");
 auto input=root/"main.on1x";
 write(input,"fn disk_contents() {}\n");
 const auto declaration=
  "@ppx forbidden \"/on1x-test-do-not-run\"\n"
  "operator (lexeme: \"%%\", arity: \"binary\", formals: a:i64,b:i64) -> i64 {\n"
  "  let =42; # scanning and header discovery succeed, full body parsing must fail\n"
  "}\n";
 write(root/"library.on1x",declaration);
 LoaderOptions opts;
 opts.prelude=false;
 const std::string body="fn main()->i64 { return 4 %% 2; }\n";
 auto imported=parse_source_with_operator_imports(input,
  "from .library import (operator(\"%%\"));\n"+body,opts);
 const auto imported_id=application(imported).operator_identity;
 try{
  (void)parse_source_with_operator_imports(input,body,opts);
  throw std::runtime_error("source query opened an undeclared operator without the prelude");
 }catch(const Error& error){
  if(error.diagnostic_id!="ON1X-OPERATOR-UNBOUND")throw;
 }

 opts.prelude=true;
 opts.stdlib=root/"std";
 opts.stdlib_explicit=true;
 write(opts.stdlib/"pervasive.on1x","fn unused() {}\n");
 write(opts.stdlib/"operators.on1x",declaration);
 auto pervasive=parse_source_with_operator_imports(input,body,opts);
 if(application(pervasive).operator_identity==imported_id)
  throw std::runtime_error("same-spelled declarations share an operator identity");
 return 0;
}
}

int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("expected a test artifact directory");
  return run(argv[1]);
 }catch(const std::exception& e){
  std::cerr<<e.what()<<'\n';
  return 1;
 }
}

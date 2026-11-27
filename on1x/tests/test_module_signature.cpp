#include <on1x/module_signature.hpp>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool test,const char* message){
 if(!test){
  std::cerr<<message<<'\n';
  std::exit(1);
 }
}
template<class F> void rejects(const char* id,F action){
 try{action();}
 catch(const on1x::Error& error){
  if(error.diagnostic_id==id)return;
  std::cerr<<"expected "<<id<<"; got "<<error.diagnostic_id<<": "<<error.what()<<'\n';
  std::exit(1);
 }
 std::cerr<<"accepted invalid input; expected "<<id<<'\n';
 std::exit(1);
}
}

int main(){
 using namespace on1x;
 auto signature=[](const std::string& body){
  return parse_module_signature(scan("abstract module Acme.Vault;\n"+body,"vault.on1xi"));
 };
 auto implementation=[](const std::string& body){
  return parse(scan(body,"/workspace/vault.on1x"));
 };
 const auto good=signature("type Token<T>; fn roundtrip(x: (Token<i64>, bool)) -> Token<i64>;");
 auto original=implementation("record Token<T> { secret: T }"
                              "fn roundtrip(argument: (Token<i64>, bool)) -> Token<i64> {"
                              "return deliberately_unchecked_body;"
                              "}");
 const auto sealed=verify_module_signature(good,original,"Acme.Vault","/workspace/vault.on1x");
 const auto same=verify_module_signature(good,original,"Acme.Vault","/workspace/vault.on1x");
 const auto other=verify_module_signature(good,original,"Acme.Vault","/different/vault.on1x");
 require(sealed.types.size()==1&&sealed.types.at("Token")==same.types.at("Token"),
         "repeated verification must preserve identity");
 require(sealed.types.at("Token")!=other.types.at("Token"),
         "same spelling in different physical modules must not share an abstract identity");
 auto fragment_one=implementation("record Token<T> { secret:T }");
 auto fragment_two=implementation("fn roundtrip(argument:(Token<i64>,bool))->Token<i64> { return x; }");
 std::vector<const Unit*> fragments={&fragment_one,&fragment_two};
 require(verify_module_signature(good,fragments,"Acme.Vault","/workspace/vault.on1x").types==sealed.types,
         "authorized fragments should conform as one declared module");
 fragments.push_back(&fragment_two);
 rejects("ON1X-INTERFACE-MEMBER",[&]{
  verify_module_signature(good,fragments,"Acme.Vault","/workspace/vault.on1x");
 });
 auto conform=[&](const ModuleSignature& sig,const Unit& unit){
  (void)verify_module_signature(sig,unit,"Acme.Vault","/workspace/vault.on1x");
 };

 rejects("ON1X-INTERFACE-MODULE",[&]{verify_module_signature(good,original,"Other.Vault","/workspace/vault.on1x");});
 rejects("ON1X-INTERFACE-MODULE",[&]{verify_module_signature(good,original,"","/workspace/vault.on1x");});
 rejects("ON1X-INTERFACE-TYPE",[&]{conform(good,implementation("record Other<T> { field:T }"));});
 rejects("ON1X-INTERFACE-TYPE",[&]{conform(good,implementation("record Token { field:i64 }"));});
 rejects("ON1X-INTERFACE-TYPE",[&]{conform(signature("type Token<T>; fn roundtrip() -> Token;"),original);});
 rejects("ON1X-INTERFACE-TYPE",[&]{conform(signature("fn roundtrip()->Token<i64>;"),original);});
 rejects("ON1X-INTERFACE-MEMBER",[&]{conform(signature("fn missing()->i64;"),original);});
 rejects("ON1X-INTERFACE-MEMBER",[&]{conform(signature("fn absent()->i64;"),implementation("extern fn absent()->i64;"));});
 rejects("ON1X-INTERFACE-ANNOTATION",[&]{
  conform(signature("fn inferred(x:i64)->i64;"),implementation("fn inferred(x)->i64 { return x; }"));
 });
 rejects("ON1X-INTERFACE-MISMATCH",[&]{
  conform(good,implementation("record Token<T> { secret:T }"
                             "fn roundtrip(argument:(Token<i64>,bool))->Token<bool> { return argument; }"));
 });
 rejects("ON1X-INTERFACE-SHAPE",[&]{conform(signature("fn roundtrip()->fn(i64)->i64;"),original);});
 rejects("ON1X-INTERFACE-SHAPE",[&]{signature("import other;");});
 rejects("ON1X-INTERFACE-SHAPE",[&]{signature("@ppx bad \"/bin/false\";");});
 rejects("ON1X-INTERFACE-SHAPE",[&]{signature("");});
 rejects("ON1X-INTERFACE-DUPLICATE",[&]{signature("type Token; abstract type Token;");});
 rejects("ON1X-INTERFACE-DUPLICATE",[&]{signature("type Token<T,T>;");});
 rejects("ON1X-INTERFACE-DUPLICATE",[&]{signature("fn f()->i64; fn f()->i64;");});
 rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{require_executable_module_signature(good);});

 auto scalar=signature("fn count(x:(i64,bool))->i64;");
 require_executable_module_signature(scalar);
 conform(scalar,implementation("fn count(arg:(i64,bool))->i64 { return 42; }"));
 conform(signature("abstract type Token<T>;"),original);

 auto procedure=signature("proc reset(value:i64);");
 require(procedure.members.front().procedure&&procedure.members.front().signature.result()==Type::Void,
         "procedure signature lost its separate source kind");
 conform(procedure,implementation("proc reset(argument:i64) {}"));
 require_executable_module_signature(procedure);
 rejects("ON1X-INTERFACE-MISMATCH",[&]{
  conform(procedure,implementation("fn reset(argument:i64) -> void {}"));
 });
 rejects("ON1X-INTERFACE-MISMATCH",[&]{
  conform(signature("fn reset(value:i64)->void;"),implementation("proc reset(argument:i64) {}"));
 });
 rejects("ON1X-INTERFACE-MISMATCH",[&]{
  conform(procedure,implementation("proc reset(argument:bool) {}"));
 });
 rejects("ON1X-INTERFACE-MISMATCH",[&]{
  conform(procedure,implementation("proc reset(argument:i64) -> i64 { return 42; }"));
 });
 rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{
  conform(procedure,implementation("proc reset(argument:ref i64) {}"));
 });
 rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{
  conform(procedure,implementation("proc reset<T>(argument:T) {}"));
 });
 rejects("ON1X-REF",[&]{signature("proc reset(argument:ref i64);");});
 rejects("ON1X-PARSE-EXPECTED",[&]{signature("proc reset(value:i64) -> void;");});
 rejects("ON1X-PARSE-EXPECTED",[&]{signature("proc reset(value:i64) {}");});
 rejects("ON1X-INTERFACE-DUPLICATE",[&]{signature("fn reset(value:i64)->void; proc reset(value:i64);");});

 auto legacy_tokens=scan("on1xi 1; fn next(x:i64)->i64;","legacy.on1xi");
 auto legacy=parse_module_signature(legacy_tokens);
 auto legacy_members=parse_interface(legacy_tokens);
 require(!legacy.module&&legacy.types.empty()&&legacy.members.size()==1&&legacy_members.size()==1&&
         legacy.members.front().signature==legacy_members.front().signature,"v1 parsing changed");
 rejects("ON1X-INTERFACE-SHAPE",[&]{
  parse_module_signature(scan("on1xi 1; fn f()->u64;","legacy.on1xi"));
 });
 rejects("ON1X-INTERFACE-SHAPE",[&]{
  parse_interface(scan("on1xi 1; proc reset(value:i64);","legacy.on1xi"));
 });
}

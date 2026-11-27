#include <on1x/module_signature.hpp>
#include <on1x/operator_scope.hpp>
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

  auto named=[](const std::string& source){
   return parse_module_signature(scan(source,"vault.on1xi"));
  };
  auto static_source=[](const std::string& source){
   return parse_with_operators(scan("module Acme.Vault;\n"+source,"/workspace/vault.on1xm"),
                               {},{},OperatorScope{},true);
  };
  const std::string catalog=R"(
   signature Base<P> {
    type Token;
    type View;
    type Payload = P;
    fn make(payload:Payload)->Token;
   };
   signature API<A> {
    include Base<(A, bool)>;
    sharing type View = Token;
    fn reveal(value:View)->Payload;
   };
  )";
  const std::string provider=R"(
   record Token { secret:i64 }
   type Payload = (i64,bool);
   type View = Token;
   fn make(payload:Payload)->View { return unchecked; }
   fn reveal(token:Token)->(i64,bool) { return unchecked; }
  )";
  auto opaque=named(catalog+"abstract module Acme.Vault : API<i64>;");
  auto transparent=named(catalog+"module Acme.Vault : API<i64>;");
  auto flat=resolve_module_signature(opaque);
  require(opaque.definitions.size()==2&&opaque.ascription&&opaque.ascription->name=="API"&&
          opaque.context_keywords.size()>=8&&flat.definitions.empty()&&!flat.ascription&&
          flat.members.size()==2&&flat.types.size()==3,
          "a named signature retains source shape then expands before conformance");
  auto static_unit=static_source(provider);
  auto hidden=verify_module_signature(opaque,static_unit,"Acme.Vault","/workspace/vault.on1xm");
  auto visible=verify_module_signature(transparent,static_unit,"Acme.Vault","/workspace/vault.on1xm");
  auto distinct=verify_module_signature(transparent,static_unit,"Acme.Vault","/elsewhere/vault.on1xm");
  require(hidden.types.contains("Token")&&hidden.transparent_types.empty()&&visible.types.empty()&&
          visible.transparent_types.at("Token")!=distinct.transparent_types.at("Token"),
          "ascription distinguishes opacity without erasing provider identity");
  require(hidden.manifest_types==visible.manifest_types&&
          hidden.manifest_types.at("View")==Type::named("Token")&&
          hidden.manifest_types.at("Payload")==Type::tuple({Type::I64,Type::Bool}),
          "sharing and parameter substitution normalize equations without exposing layout");
  rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{require_executable_module_signature(opaque);});
  rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{require_executable_module_signature(transparent);});
  rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{
   parse(scan("module Acme.Vault; type Payload = i64;","/workspace/vault.on1xm"));
  });
  auto former_identifiers=parse(scan(
      "record sharing { type:i64 }\n"
      "fn signature(include:i64)->i64 { let abstract=include; return abstract; }",
      "legacy.on1x"));
  require(former_identifiers.functions.front().name=="signature"&&
          former_identifiers.nominals.front().name=="sharing","signature words must remain contextual");

  rejects("ON1X-INTERFACE-MISMATCH",[&]{
   conform(named(catalog+"module Acme.Vault : API<u64>;"),static_unit);
  });
  rejects("ON1X-INTERFACE-TYPE",[&]{
   conform(opaque,static_source("record Token{secret:i64} type Payload=(i64,bool);"));
  });
  rejects("ON1X-INTERFACE-MISMATCH",[&]{
   conform(opaque,static_source("record Token{a:i64} record Other{a:i64} "
                               "type Payload=(i64,bool); type View=Other;"));
  });
  rejects("ON1X-INTERFACE-CYCLE",[&]{
   conform(named("signature S { type A=B; type B=A; }; module Acme.Vault : S;"),static_unit);
  });
  rejects("ON1X-INTERFACE-CYCLE",[&]{
   conform(named("signature S { type A=i64; }; module Acme.Vault : S;"),
           static_source("type A=B; type B=(i64,A);"));
  });
  rejects("ON1X-INTERFACE-LIMIT",[&]{
   std::string growing="type A0=i64;";
   for(int i=1;i<20;++i){
    auto branch=std::to_string(i-1);
    growing+="type A"+std::to_string(i)+"=(A"+branch+",A"+branch+");";
   }
   conform(named("signature S { type A0=i64; }; module Acme.Vault : S;"),static_source(growing));
  });
  auto expansion_error=[&](const char* id,const std::string& source){
   rejects(id,[&]{resolve_module_signature(named(source));});
  };
  expansion_error("ON1X-INTERFACE-CYCLE",
                  "signature A { include B; }; signature B { include A; }; module Acme.Vault : A;");
  expansion_error("ON1X-INTERFACE-ARITY",catalog+"module Acme.Vault : API;");
  expansion_error("ON1X-INTERFACE-UNSUPPORTED",catalog+"module Acme.Vault : API<Token>;");
  expansion_error("ON1X-INTERFACE-SIGNATURE","module Acme.Vault : Missing;");
  expansion_error("ON1X-INTERFACE-DUPLICATE",
                  "signature B { type X; }; signature A { include B; type X; }; module Acme.Vault : A;");
  expansion_error("ON1X-INTERFACE-TYPE",
                  "signature S { type T=i64; sharing type T=i64; }; module Acme.Vault : S;");
  auto callable=named("signature S<T> { fn count(x:T)->i64; }; module Acme.Vault : S<i64>;");
  require_executable_module_signature(callable);
  conform(callable,implementation("fn count(input:i64)->i64 { return input; }"));

  const auto permit_signature=signature("type Token;");
  auto permit_provider=static_source("record Bits { data:array<u8,16> } "
                                    "enum Token { None, Some(Bits, bool) }");
  const auto permit_proof=verify_module_signature(permit_signature,permit_provider,
                                                  "Acme.Vault","/workspace/vault.on1xm");
  require(admit_executable_opaque_signature(permit_signature,permit_proof,{&permit_provider},
                                            "Acme.Vault","/workspace/vault.on1xm")==
          std::set<std::string>{"Token"},"opaque permit must export only the authorized abstract type");
  rejects("ON1X-INTERFACE-MODULE",[&]{
   admit_executable_opaque_signature(permit_signature,permit_proof,{&permit_provider},
                                     "Acme.Vault","/elsewhere/vault.on1xm");
  });
  rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{
   admit_executable_opaque_signature(transparent,visible,{&static_unit},"Acme.Vault","/workspace/vault.on1xm");
  });
  for(const auto* field:{"string","Box<i64>","own<Token>","Token"}){
   auto non_copy=static_source(std::string("record Box<T> { item:T } record Token { item:")+field+" }");
   auto proof=verify_module_signature(permit_signature,non_copy,"Acme.Vault","/workspace/vault.on1xm");
   rejects("ON1X-INTERFACE-UNSUPPORTED",[&]{
    admit_executable_opaque_signature(permit_signature,proof,{&non_copy},"Acme.Vault","/workspace/vault.on1xm");
   });
  }
 }

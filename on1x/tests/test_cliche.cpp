#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/cliche.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace on1x;

void require(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}

Unit source(const std::string& text){
 return parse(scan(text,"cliche-test"));
}

const char* positive=R"(
record Box<T> { value: T }
cliche Show {
 fn show(value: Self) -> string;
 let fallback: Self;
}
cliche Code { let code: i64; }
cliche Describe<Other> where Show<Other> {
 fn describe(value: Self, other: Other) -> string;
}
impl Show for i64 {
 fn show(value: i64) -> string { return format("{}", value); }
 let fallback: i64 = 5;
}
impl Show for bool {
 fn show(value: bool) -> string { return "bool"; }
 let fallback: bool = false;
}
impl<T> Show for Box<T> where Show<T> {
 fn show(value: Box<T>) -> string {
  return Show<T>::show(value["value"]);
 }
 let fallback: Box<T> =
  record<Box<T>> { value: Show<T>::fallback };
}
impl Show for Box<i64> {
 fn show(value: Box<i64>) -> string { return "exact"; }
 let fallback: Box<i64> = record<Box<i64>> { value: 42 };
}
impl<T> Code for Box<T> { let code: i64 = 9; }
impl Describe<bool> for i64 {
 fn describe(value: i64, other: bool) -> string {
  return Show<bool>::show(other);
 }
}
fn render<T>(value: T) -> string where Show<T> {
 return Show<T>::show(value);
}
fn main() {
 println(render(7));
 println(render(record<Box<bool>> { value: true }));
 println(Show<Box<i64>>::show(record<Box<i64>> { value: 1 }));
 println(Show<Box<i64>>::fallback["value"]);
 println(Code<Box<bool>>::code);
 println(Describe<i64,bool>::describe(0, true));
}
)";

template<class F>
void reject(F&& action,const std::string& id,const std::string& text){
 try{action();}
 catch(const Error& error){
  require(error.diagnostic_id==id,"wrong cliche diagnostic identity");
  require(std::string(error.what()).find(text)!=std::string::npos,
          "wrong cliche diagnostic text");
  return;
 }
 throw std::runtime_error("invalid cliche program was admitted; expected "+id+
                          " containing '"+text+"'");
}

void walk_expression(const Expr& expression,bool executable,
                     bool& executable_projection,bool& prototype_projection){
 if(expression.kind==Expr::Kind::ClicheMember){
  if(executable)executable_projection=true;
  if(expression.prototype)prototype_projection=true;
 }
 if(expression.callee)
  walk_expression(*expression.callee,executable,executable_projection,
                  prototype_projection);
 for(const auto& child:expression.children)
  walk_expression(*child,executable,executable_projection,prototype_projection);
 for(const auto& proof:expression.proofs)
  walk_expression(*proof,executable,executable_projection,prototype_projection);
 auto walk_body=[&](auto&& self,const std::vector<Stmt>& body)->void{
  for(const auto& statement:body){
   if(statement.value)
    walk_expression(*statement.value,executable,executable_projection,
                    prototype_projection);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
 walk_body(walk_body,expression.body);
}

void walk_function(const Function& function,bool& executable_projection,
                   bool& prototype_projection){
 auto walk=[&](auto&& self,const std::vector<Stmt>& body)->void{
  for(const auto& statement:body){
   if(statement.value)
    walk_expression(*statement.value,!function.prototype,executable_projection,
                    prototype_projection);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
 walk(walk,function.body);
}
}

int main(){
 try{
  auto unit=source(positive);
  check(unit);
  require(unit.cliches_prepared,"cliche declarations were not prepared");
  require(unit.forall_elaborated,"cliche specializations did not reach inference fixed point");

  bool executable_projection=false,prototype_projection=false;
  for(const auto& function:unit.functions)
   walk_function(function,executable_projection,prototype_projection);
  require(!executable_projection,
          "an executable cliche projection survived static elaboration");
  require(prototype_projection,
          "the constrained generic prototype lost its deferred projection");

  const auto box_i64=Type::named("Box",{Type::I64});
  const auto box_bool=Type::named("Box",{Type::Bool});
  auto exact=cliches::resolve(unit,"Show",{box_i64},"show",{},{"cliche-test"});
  auto blanket=cliches::resolve(unit,"Show",{box_bool},"show",{},{"cliche-test"});
  require(exact.implementation&&exact.implementation->type_parameters.empty(),
          "exact implementation did not override the blanket implementation");
  require(blanket.implementation&&blanket.implementation->type_parameters.size()==1,
          "blanket implementation was not selected");
  require(exact.member&&!exact.member->runtime_name.empty()&&
          blanket.member&&!blanket.member->runtime_name.empty(),
          "selected members have no static runtime name");
  auto parameterized=cliches::resolve(unit,"Describe",{Type::I64,Type::Bool},
                                      "describe",{},{"cliche-test"});
  require(parameterized.implementation&&parameterized.member,
          "generic cliche declaration did not resolve");

  auto first=emit_c(unit,"cliche-test");
  check(unit);
  require(first==emit_c(unit,"cliche-test"),
          "cliche checking or C11 output is not deterministic/idempotent");

  std::size_t budget=262144;
  auto cloned=clone_unit(unit,budget);
  require(cloned.cliches_prepared&&
          cloned.cliches.size()==unit.cliches.size()&&
          cloned.cliche_implementations.size()==unit.cliche_implementations.size(),
          "transactional cloning lost cliche metadata");
  require(cloned.cliche_implementations.front().members.front().runtime_name==
           unit.cliche_implementations.front().members.front().runtime_name,
           "transactional cloning lost a generated member identity");

  auto blanket_member=[](Unit& value)->ClicheImplementationMember&{
   auto implementation=std::find_if(
    value.cliche_implementations.begin(),value.cliche_implementations.end(),
    [](const ClicheImplementation& item){
     return item.name=="Show"&&!item.type_parameters.empty();
    });
   if(implementation==value.cliche_implementations.end())
    throw std::runtime_error("checked cliche unit lost its blanket implementation");
   auto member=std::find_if(
    implementation->members.begin(),implementation->members.end(),
    [](const ClicheImplementationMember& item){return item.name=="show";});
   if(member==implementation->members.end())
    throw std::runtime_error("checked cliche unit lost its blanket member");
   return *member;
  };
  auto generated=[](Unit& value,const std::string& name)->Function&{
   auto function=std::find_if(
    value.functions.begin(),value.functions.end(),
    [&](const Function& item){return item.name==name;});
   if(function==value.functions.end())
    throw std::runtime_error("checked cliche unit lost a generated function");
   return *function;
  };

  budget=262144;
  auto bad_signature=clone_unit(unit,budget);
  auto& signature_member=blanket_member(bad_signature);
  generated(bad_signature,signature_member.runtime_name).result=Type::Bool;
  reject([&]{cliches::verify_elaborated(bad_signature);},
         "ON1X-CLICHE-HIR","generated cliche member signature");
  reject([&]{(void)emit_c(bad_signature,"forged-cliche-signature");},
         "ON1X-CLICHE-HIR","generated cliche member signature");
  reject([&]{(void)mir::lower(bad_signature);},
         "ON1X-CLICHE-HIR","generated cliche member signature");

  budget=262144;
  auto bad_generics=clone_unit(unit,budget);
  auto& generic_member=blanket_member(bad_generics);
  generated(bad_generics,generic_member.runtime_name).type_parameters.clear();
  reject([&]{cliches::verify_elaborated(bad_generics);},
         "ON1X-CLICHE-HIR","generic metadata mismatch");
  reject([&]{(void)emit_c(bad_generics,"forged-cliche-generics");},
         "ON1X-CLICHE-HIR","generic metadata mismatch");
  reject([&]{(void)mir::lower(bad_generics);},
         "ON1X-CLICHE-HIR","generic metadata mismatch");

  budget=262144;
  auto bad_constraints=clone_unit(unit,budget);
  auto& constrained_member=blanket_member(bad_constraints);
  generated(bad_constraints,constrained_member.runtime_name).constraints.clear();
  reject([&]{cliches::verify_elaborated(bad_constraints);},
         "ON1X-CLICHE-HIR","generic metadata mismatch");
  reject([&]{(void)emit_c(bad_constraints,"forged-cliche-constraints");},
         "ON1X-CLICHE-HIR","generic metadata mismatch");
  reject([&]{(void)mir::lower(bad_constraints);},
         "ON1X-CLICHE-HIR","generic metadata mismatch");

  budget=262144;
  auto bad_identity=clone_unit(unit,budget);
  auto& identity_member=blanket_member(bad_identity);
  auto& identity_function=generated(bad_identity,identity_member.runtime_name);
  identity_member.runtime_name="@cliche_forged_identity";
  identity_function.name=identity_member.runtime_name;
  reject([&]{cliches::verify_elaborated(bad_identity);},
         "ON1X-CLICHE-HIR","generated cliche member identity");
  reject([&]{(void)emit_c(bad_identity,"forged-cliche-identity");},
         "ON1X-CLICHE-HIR","generated cliche member identity");
  reject([&]{(void)mir::lower(bad_identity);},
         "ON1X-CLICHE-HIR","generated cliche member identity");

  cliches::ResolutionLimits exhausted;
  exhausted.work=0;
  reject([&]{(void)cliches::resolve(unit,"Show",{Type::I64},"show",{},
                                    {"cliche-limit"},exhausted);},
         "ON1X-CLICHE-LIMIT","work budget");
  reject([&]{(void)cliches::resolve(unit,"Code",{Type::Bool},"code",{},
                                    {"cliche-unsatisfied"});},
         "ON1X-CLICHE-UNSATISFIED","unsatisfied cliche obligation");
  cliches::ResolutionLimits shallow;
  shallow.depth=1;
  reject([&]{(void)cliches::resolve(
    unit,"Show",{Type::named("Box",{Type::named("Box",{Type::Bool})})},
    "show",{},{"cliche-depth"},shallow);},
    "ON1X-CLICHE-LIMIT","depth budget");

  budget=262144;
  auto ambiguous=clone_unit(unit,budget);
  auto exact_i64=std::find_if(
   ambiguous.cliche_implementations.begin(),ambiguous.cliche_implementations.end(),
   [](const auto& implementation){
    return implementation.name=="Show"&&implementation.type_parameters.empty()&&
           implementation.arguments==std::vector<Type>{Type::I64};
   });
  require(exact_i64!=ambiguous.cliche_implementations.end(),
          "checked cliche unit lost its exact implementation");
  ClicheImplementation duplicate;duplicate.where=exact_i64->where;
  duplicate.name=exact_i64->name;duplicate.arguments=exact_i64->arguments;
  ambiguous.cliche_implementations.push_back(std::move(duplicate));
  reject([&]{(void)cliches::resolve(ambiguous,"Show",{Type::I64},"show",{},
                                    {"cliche-ambiguous"});},
         "ON1X-CLICHE-AMBIGUOUS","ambiguous exact");

  budget=262144;
  auto recursive=clone_unit(unit,budget);
  auto recursive_i64=std::find_if(
   recursive.cliche_implementations.begin(),recursive.cliche_implementations.end(),
   [](const auto& implementation){
    return implementation.name=="Show"&&implementation.type_parameters.empty()&&
           implementation.arguments==std::vector<Type>{Type::I64};
   });
  require(recursive_i64!=recursive.cliche_implementations.end(),
          "checked cliche unit lost its recursive-test implementation");
  ClicheConstraint cycle;cycle.where={"cliche-recursive"};cycle.name="Show";
  cycle.arguments={Type::I64};recursive_i64->constraints.push_back(std::move(cycle));
  reject([&]{(void)cliches::resolve(recursive,"Show",{Type::I64},"show",{},
                                    {"cliche-recursive"});},
         "ON1X-CLICHE-RECURSIVE","recursive cliche obligation");

  auto wire_source=source("cliche C { let value: Self; } fn main() {}");
  reject([&]{(void)meta::encode(wire_source);},
         "ON1X-CLICHE-UNSUPPORTED","Unit-v1 PPX");
  reject([&]{auto mixed=source(
    "@enable Protocols; cliche C { let value: Self; } fn main() {}");
    check(mixed);},
    "ON1X-CLICHE-UNSUPPORTED","cannot cross PPX/Category");

  budget=262144;
  auto forged=clone_unit(unit,budget);
  auto main=std::find_if(forged.functions.begin(),forged.functions.end(),
                         [](const auto& function){return function.name=="main";});
  require(main!=forged.functions.end(),"checked cliche unit lost main");
  auto& expression=*main->body.front().value;
  expression.kind=Expr::Kind::ClicheMember;
  expression.prototype=true;
  reject([&]{cliches::verify_elaborated(forged);},
         "ON1X-CLICHE-UNSUPPORTED","unresolved concrete cliche member");
  reject([&]{(void)emit_c(forged,"forged-cliche");},
          "ON1X-CLICHE-UNSUPPORTED","unresolved concrete cliche member");
  reject([&]{(void)mir::lower(forged);},
         "ON1X-CLICHE-UNSUPPORTED","unresolved concrete cliche member");

  reject([&]{auto bad=source(R"(
   cliche Show { fn show(value: Self) -> string; }
   fn bad<T>(value: T) -> string { return Show<T>::show(value); }
   fn main() {}
  )");check(bad);},"ON1X-TYPE","add it to the function's where clause");

  reject([&]{auto bad=source(R"(
   cliche Show { fn show(value: Self) -> string; }
   fn main() { println(Show<bool>::show(true)); }
  )");check(bad);},"ON1X-CLICHE-UNSATISFIED","unsatisfied cliche obligation");

  reject([&]{auto bad=source(R"(
   cliche Show { fn show(value: Self) -> string; }
   impl Show for i64 {
    fn show(value: bool) -> string { return "wrong"; }
   }
   fn main() {}
  )");check(bad);},"ON1X-CLICHE-SIGNATURE","signature mismatch");

  reject([&]{auto bad=source(R"(
   record Pair<A,B> { first: A, second: B }
   cliche Mark { let marker: i64; }
   impl<T> Mark for Pair<T,i64> { let marker: i64 = 1; }
   impl<U> Mark for Pair<i64,U> { let marker: i64 = 2; }
   fn main() {}
  )");check(bad);},"ON1X-CLICHE-COHERENCE","overlapping generic");

  std::cout<<"Core cliches: coherence, bounds and static elaboration PASS\n";
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';return 1;
 }
}

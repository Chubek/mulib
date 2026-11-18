#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/inherent.hpp>
#include <on1x/meta.hpp>
#include <on1x/nominal.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace on1x;

void require(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}

Unit source(const std::string& text){
 return parse(scan(text,"core-impl-test"));
}

Unit checked(const std::string& text){
 auto unit=source(text);check(unit);return unit;
}

template<class F>
void reject(F&& action,const std::string& id,const std::string& text){
 try{action();}
 catch(const Error& error){
  if(error.diagnostic_id!=id)
   throw std::runtime_error("wrong intrinsic impl diagnostic identity: expected "+
                            id+", found "+error.diagnostic_id+" ("+
                            error.what()+")");
  require(std::string(error.what()).find(text)!=std::string::npos,
          "wrong intrinsic impl diagnostic text");
  return;
 }
 throw std::runtime_error("invalid intrinsic impl HIR was admitted");
}

void walk_expression(const Expr& expression,bool& projection){
 projection|=expression.kind==Expr::Kind::InherentMember;
 if(expression.callee)walk_expression(*expression.callee,projection);
 if(expression.syntax_tail)walk_expression(*expression.syntax_tail,projection);
 for(const auto& child:expression.children)
  if(child)walk_expression(*child,projection);
 for(const auto& proof:expression.proofs)
  if(proof)walk_expression(*proof,projection);
 auto body=[&](auto&& self,const std::vector<Stmt>& statements)->void{
  for(const auto& statement:statements){
   if(statement.value)walk_expression(*statement.value,projection);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
 body(body,expression.body);
}

void walk_function(const Function& function,bool& projection){
 auto body=[&](auto&& self,const std::vector<Stmt>& statements)->void{
  for(const auto& statement:statements){
   if(statement.value)walk_expression(*statement.value,projection);
   self(self,statement.body);self(self,statement.otherwise);
  }
 };
 body(body,function.body);
}

const char* positive=R"(
record Box<T> { value: T }
impl<T> Box<T> {
 fn make(value: T) -> Box<T> {
  return record<Box<T>> { value: value };
 }
 fn get(self) -> T { return self["value"]; }
 fn choose<U>(left: T, right: U) -> U { return right; }
 value answer: i64 = 42;
 value item(self): T = self["value"];
}
fn main() {
 let integer=Box<i64>::make(7);
 let boolean=Box<bool>::make(true);
 assert(integer::get()==7);
 assert(boolean::get());
 assert(Box<i64>::choose(1,true));
 assert(Box<i64>::answer==42);
 assert(integer::item==7);
 let make=Box<i64>::make;
 assert(make(9)::get()==9);
}
)";
}

int main(){
 try{
  using namespace on1x;
  auto unit=source(positive);
  check(unit);
  require(unit.inherents_prepared,"intrinsic implementations were not prepared");
  require(unit.forall_elaborated,"impl specializations did not reach inference fixed point");

  bool projection=false;
  for(const auto& function:unit.functions)walk_function(function,projection);
  require(!projection,"an inherent projection survived checked HIR elaboration");

  inherent::Catalog members(unit);
  auto get=members.find("Box","get",{"core-impl-test"});
  auto answer=members.find("Box","answer",{"core-impl-test"});
  require(get.member&&get.member->receiver==InherentMember::Receiver::Value,
          "receiver method metadata was lost");
  require(answer.member&&answer.member->kind==InherentMember::Kind::Value&&
          answer.member->receiver==InherentMember::Receiver::None,
          "associated value metadata was lost");
  require(!get.member->runtime_name.empty()&&
          get.member->runtime_name!=answer.member->runtime_name,
          "private member identities are missing or aliased");
  reject([&]{(void)members.find("Box","missing",{"core-impl-member"});},
         "ON1X-IMPL-MEMBER","has no inherent member");

  auto first=emit_c(unit,"core-impl-test");
  check(unit);
  require(first==emit_c(unit,"core-impl-test"),
          "impl checking or C11 publication is not deterministic/idempotent");

  std::size_t budget=262144;
  auto clone=clone_unit(unit,budget);
  require(clone.inherents_prepared&&
          clone.inherent_implementations.size()==
           unit.inherent_implementations.size(),
          "transactional cloning lost impl metadata");
  require(clone.inherent_implementations.front().members.front().runtime_name==
           unit.inherent_implementations.front().members.front().runtime_name,
           "transactional cloning lost private member identity");
  require(emit_c(clone,"core-impl-clone")==first,
           "cloned checked impl unit changed C11 publication");

  auto implementation_member=[](Unit& value,
                                const std::string& name)->InherentMember&{
   auto member=std::find_if(
    value.inherent_implementations.front().members.begin(),
    value.inherent_implementations.front().members.end(),
    [&](const InherentMember& item){return item.name==name;});
   if(member==value.inherent_implementations.front().members.end())
    throw std::runtime_error("checked impl unit lost a member");
   return *member;
  };
  auto generated=[](Unit& value,const std::string& name)->Function&{
   auto function=std::find_if(
    value.functions.begin(),value.functions.end(),
    [&](const Function& item){return item.name==name;});
   if(function==value.functions.end())
    throw std::runtime_error("checked impl unit lost a generated function");
   return *function;
  };

  budget=262144;
  auto bad_signature=clone_unit(unit,budget);
  auto& signature_member=implementation_member(bad_signature,"make");
  generated(bad_signature,signature_member.runtime_name).result=Type::Bool;
  reject([&]{inherent::verify_elaborated(bad_signature);},
         "ON1X-IMPL-HIR","generated inherent member signature");
  reject([&]{(void)emit_c(bad_signature,"forged-impl-signature");},
         "ON1X-IMPL-HIR","generated inherent member signature");
  reject([&]{(void)mir::lower(bad_signature);},
         "ON1X-IMPL-HIR","generated inherent member signature");

  budget=262144;
  auto bad_generics=clone_unit(unit,budget);
  auto& generic_member=implementation_member(bad_generics,"make");
  generated(bad_generics,generic_member.runtime_name).type_parameters.clear();
  reject([&]{inherent::verify_elaborated(bad_generics);},
         "ON1X-IMPL-HIR","generic metadata mismatch");
  reject([&]{(void)emit_c(bad_generics,"forged-impl-generics");},
         "ON1X-IMPL-HIR","generic metadata mismatch");
  reject([&]{(void)mir::lower(bad_generics);},
         "ON1X-IMPL-HIR","generic metadata mismatch");

  budget=262144;
  auto bad_constraints=clone_unit(unit,budget);
  auto& constrained_member=implementation_member(bad_constraints,"make");
  ClicheConstraint forged_constraint;
  forged_constraint.where=constrained_member.where;
  forged_constraint.name="Missing";
  forged_constraint.arguments={Type::bound(0)};
  generated(bad_constraints,constrained_member.runtime_name).
   constraints.push_back(std::move(forged_constraint));
  reject([&]{inherent::verify_elaborated(bad_constraints);},
         "ON1X-IMPL-HIR","generic metadata mismatch");
  reject([&]{(void)emit_c(bad_constraints,"forged-impl-constraints");},
         "ON1X-IMPL-HIR","generic metadata mismatch");
  reject([&]{(void)mir::lower(bad_constraints);},
         "ON1X-IMPL-HIR","generic metadata mismatch");

  budget=262144;
  auto bad_identity=clone_unit(unit,budget);
  auto& identity_member=implementation_member(bad_identity,"make");
  auto& identity_function=generated(bad_identity,identity_member.runtime_name);
  identity_member.runtime_name="@inherent_999_0";
  identity_function.name=identity_member.runtime_name;
  reject([&]{inherent::verify_elaborated(bad_identity);},
         "ON1X-IMPL-HIR","generated inherent member identity");
  reject([&]{(void)emit_c(bad_identity,"forged-impl-identity");},
         "ON1X-IMPL-HIR","generated inherent member identity");
  reject([&]{(void)mir::lower(bad_identity);},
         "ON1X-IMPL-HIR","generated inherent member identity");

  NominalCatalog catalog(unit);
  require(catalog.copy_safe(Type::named("Box",{Type::I64}),
                            {"core-impl-copy"}),
          "copy-safe concrete receiver was rejected");
  require(!catalog.copy_safe(Type::named("Box",{Type::String}),
                             {"core-impl-copy"}),
          "managed concrete receiver was accepted");
  require(!catalog.copy_safe(Type::named("Box",{Type::SExpr}),
                             {"core-impl-copy"}),
          "shared-owner concrete receiver was accepted");

  auto raw=source(positive);
  reject([&]{(void)meta::encode(raw);},
         "ON1X-IMPL-UNSUPPORTED","Unit-v1 PPX");
  reject([&]{(void)emit_c(raw,"unprepared-impl");},
         "ON1X-IMPL","not prepared before backend admission");

  auto prepared=source(positive);
  inherent::prepare(prepared);
  auto function_count=prepared.functions.size();
  inherent::prepare(prepared);
  require(prepared.functions.size()==function_count,
          "impl preparation is not idempotent");

  budget=262144;
  auto forged=clone_unit(unit,budget);
  auto main=std::find_if(forged.functions.begin(),forged.functions.end(),
                         [](const Function& function){
                          return function.name=="main";
                         });
  require(main!=forged.functions.end()&&!main->body.empty()&&
           main->body.front().value,"missing checked expression to mutate");
  main->body.front().value->kind=Expr::Kind::InherentMember;
  main->body.front().value->prototype=true;
  reject([&]{inherent::verify_elaborated(forged);},
         "ON1X-IMPL","unresolved inherent member");
  reject([&]{(void)emit_c(forged,"forged-impl");},
          "ON1X-IMPL","unresolved inherent member");
  reject([&]{(void)mir::lower(forged);},
         "ON1X-IMPL","unresolved inherent member");

  reject([&]{
   auto malformed=source("record A{x:i64} impl A{value n:i64=1;} fn main(){}");
   malformed.inherent_implementations.front().target=Type::I64;
   inherent::prepare(malformed);
  },"ON1X-IMPL","target must be a declared record or enum");

  reject([&]{
   auto collision=source("record A{x:i64} impl A{value n:i64=1;} fn main(){}");
   collision.inherent_implementations.front().members.front().runtime_name="main";
   inherent::prepare(collision);
  },"ON1X-IMPL-COHERENCE","runtime name collision");

  reject([&]{
   auto duplicate=source(R"(
    record Box<T>{value:T}
    impl Box<i64>{value code:i64=1;}
    impl Box<bool>{fn code()->i64{return 2;}}
    fn main(){}
   )");
   check(duplicate);
  },"ON1X-IMPL-DUPLICATE","duplicate inherent member");

  reject([&]{
   auto ambiguous=source(R"(
    record Mark<T>{value:T}
    cliche Mark { let code:i64; }
    impl<T> Mark<T>{value code:i64=1;}
    fn main(){println(Mark<i64>::code);}
   )");
   check(ambiguous);
  },"ON1X-IMPL-AMBIGUOUS","both a cliche and nominal type");

  auto scalar=checked(R"(
   record Marker{unused:i64}
   impl Marker{value answer:i64=42;}
   fn main()->i64{return Marker::answer;}
  )");
  auto module=mir::lower(scalar);mir::verify(module);

  reject([&]{
   auto aggregate=checked(R"(
    record Counter{value:i64}
    impl Counter{fn read(self)->i64{return self["value"];}}
    fn main(){let value=record<Counter>{value:1};println(value::read());}
   )");
   (void)mir::lower(aggregate);
  },"ON1X-MIR-UNSUPPORTED","scalar MIR cannot lower Counter");

  std::cout<<"Core impl: metadata, cloning, static elaboration and backend boundaries PASS\n";
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';
  return 1;
 }
}

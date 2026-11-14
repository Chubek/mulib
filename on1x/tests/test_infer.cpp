#include <on1x/infer.hpp>
#include <iostream>

namespace {
void require(bool condition,const char* message) {
 if(!condition)throw std::runtime_error(message);
}
on1x::Unit parse(const std::string& source) {
 return on1x::parse(on1x::scan(source,"inference-test"));
}
void limited(const on1x::InferenceLimits& limits,const std::string& diagnostic) {
 auto unit=parse("fn increment(x) -> _ { return x + 1; } fn main() { increment(41); }");
 try { on1x::infer_types(unit,limits); }
 catch(const on1x::Error& error) {
  require(std::string(error.what()).find(diagnostic)!=std::string::npos,"incorrect budget diagnostic");
  require(unit.functions[0].params[0].type==on1x::Type::Hole,"failed inference changed a parameter");
  require(unit.functions[0].result==on1x::Type::Hole,"failed inference changed a result");
  require(unit.functions[0].body[0].value->type==on1x::Type::Invalid,"failed inference partially typed the HIR");
  return;
 }
 throw std::runtime_error("inference budget was not enforced");
}
}

int main() {
 try {
  auto unit=parse("fn apply(f, x) -> _ { return f(x); } fn inc(x) -> _ { return x + 1; } "
                  "fn main() { println(apply(inc, 41)); }");
  on1x::check(unit);
   require(unit.functions[0].prototype&&unit.functions[0].params[0].type.has_generics(),"higher-order global was not generalized");
   require(unit.functions.back().params[1].type==on1x::Type::I64,"failed to specialize argument type");
   require(unit.functions.back().result==on1x::Type::I64,"failed to specialize result type");
  for(const auto& function:unit.functions) {
   require(!function.result.has_holes(),"result inference hole escaped grounding");
   for(const auto& parameter:function.params)
    require(!parameter.type.has_holes(),"parameter inference hole escaped grounding");
  }
  auto first=on1x::emit_c(unit,"inference-test");
  on1x::check(unit);
  require(first==on1x::emit_c(unit,"inference-test"),"checking a grounded unit is not idempotent");

  on1x::InferenceLimits limits;
  limits.nodes=1;limited(limits,"node limit");
  limits={};limits.work=1;limited(limits,"work limit");
  limits={};limits.depth=1;limited(limits,"1-level limit");
  limits={};limits.type_nodes=1;limited(limits,"expanded-size limit");
  auto exact=parse("fn main() -> _ {}");
  limits={};limits.nodes=3;limits.depth=2;limits.type_nodes=2;
  on1x::infer_types(exact,limits);
  require(exact.functions[0].result==on1x::Type::Void,"inference budget boundaries are not inclusive");

  auto ambiguous=parse("fn identity(x) -> _ { return x; } fn main() {}");
   on1x::check(ambiguous);
   require(ambiguous.functions[0].prototype&&ambiguous.functions[0].result.has_generics(),"unused global was not generalized");
   bool rejected=false;

  auto cyclic=parse("fn omega(f) -> _ { return f(f); } fn main() {}");
  rejected=false;
  try { on1x::infer_types(cyclic); } catch(const on1x::Error& error) {
   rejected=std::string(error.what()).find("infinite type")!=std::string::npos;
  }
  require(rejected&&cyclic.functions[0].result==on1x::Type::Hole,
          "occurs-check failure was not transactional");
  auto polymorphic=parse("fn main() { let id = fn(x) -> _ { return x; }; println(id(42)); println(id(true)); }");
  on1x::check(polymorphic);
  require(polymorphic.functions[0].body[0].kind==on1x::Stmt::Kind::Template,"let scheme was not retained");
  require(polymorphic.functions[0].body[0].value->type.has_generics(),"generic prototype lost quantified types");
  auto generated=on1x::emit_c(polymorphic,"schemes");on1x::check(polymorphic);
  require(generated==on1x::emit_c(polymorphic,"schemes"),"specialization is not idempotent");
  auto capped=parse("fn main() { let id = fn(x) -> _ { return x; }; id(42); }");
  limits={};limits.specializations=0;rejected=false;
  try{on1x::infer_types(capped,limits);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("specialization limit")!=std::string::npos;}
  require(rejected&&capped.functions[0].body[0].kind==on1x::Stmt::Kind::Let,"failed specialization modified the source AST");
  limits={};limits.expanded_ast=0;rejected=false;
  try{on1x::infer_types(capped,limits);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("AST limit")!=std::string::npos;}
  require(rejected&&capped.functions[0].body[0].kind==on1x::Stmt::Kind::Let,"clone budget failure modified the source AST");
  std::string chain="fn main() { let f0 = fn(x) -> _ { return x; };";
  for(int i=1;i<70;++i)chain+="let f"+std::to_string(i)+" = f"+std::to_string(i-1)+";";
  chain+="f69(1); }";auto nested=parse(chain);limits={};limits.specializations=100000;rejected=false;
  try{on1x::infer_types(nested,limits);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("nesting limit")!=std::string::npos;}
   require(rejected,"specialization recursion was not bounded independently");
   auto higher=parse("fn use(f:forall(a)fn(a)->a){println(f(42));assert(f(true));} fn main(){use(fn(x)->_ {return x;});}");
   on1x::check(higher);
   require(higher.forall_layouts.size()==1&&higher.forall_layouts.begin()->second.size()==2,"universal demand layout was not materialized");
   auto higher_c=on1x::emit_c(higher,"higher");on1x::check(higher);
   require(higher_c==on1x::emit_c(higher,"higher"),"forall checking/elaboration is not idempotent");
   auto higher_capped=parse("fn use(f:forall(a)fn(a)->a){println(f(42));} fn main(){use(fn(x)->_ {return x;});}");
   limits={};limits.specializations=0;rejected=false;
   try{on1x::infer_types(higher_capped,limits);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("limit")!=std::string::npos;}
   require(rejected&&!higher_capped.forall_elaborated&&higher_capped.forall_layouts.empty(),"failed forall elaboration was not transactional");
  std::cout<<"Monotype inference engine: grounding, budgets and transactional failures PASS\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';return 1;
 }
}

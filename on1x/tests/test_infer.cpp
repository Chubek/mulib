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
  require(on1x::type_name(unit.functions[0].params[0].type)=="fn(i64) -> i64","failed to infer higher-order parameter");
  require(unit.functions[0].params[1].type==on1x::Type::I64,"failed to infer argument type");
  require(unit.functions[0].result==on1x::Type::I64,"failed to infer result type");
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
  bool rejected=false;
  try { on1x::check(ambiguous); } catch(const on1x::Error& error) {
   rejected=std::string(error.what()).find("cannot infer parameter x")!=std::string::npos;
  }
  require(rejected,"unconstrained variables were defaulted or generalized");

  auto cyclic=parse("fn omega(f) -> _ { return f(f); } fn main() {}");
  rejected=false;
  try { on1x::infer_types(cyclic); } catch(const on1x::Error& error) {
   rejected=std::string(error.what()).find("infinite type")!=std::string::npos;
  }
  require(rejected&&cyclic.functions[0].result==on1x::Type::Hole,
          "occurs-check failure was not transactional");
  std::cout<<"Monotype inference engine: grounding, budgets and transactional failures PASS\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';return 1;
 }
}

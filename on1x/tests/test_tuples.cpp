#include <on1x/infer.hpp>
#include <iostream>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
on1x::Unit parse(const std::string& source){return on1x::parse(on1x::scan(source,"tuple-test"));}
}
int main(){
 try{
  auto unit=parse("fn swap(pair) -> _ { let (a, b) = pair; return (b, a); } "
                  "fn main() { let (flag, number) = swap((42, true)); println(number); assert(flag); }");
  on1x::check(unit);
   require(unit.functions[0].prototype&&unit.functions[0].params[0].type.is_tuple(),"pattern did not constrain generic tuple parameter");
   require(on1x::type_name(unit.functions.back().result)=="(bool, i64)","tuple result was not specialized");
  auto& binding=unit.functions[0].body[0];
  require(binding.pattern&&binding.pattern->elements.size()==2,"tuple pattern was lost");
  require(binding.value->type.is_tuple()&&!binding.value->type.has_holes(),"tuple expression was not grounded");
  auto emitted=on1x::emit_c(unit,"tuple-test");on1x::check(unit);
  require(emitted==on1x::emit_c(unit,"tuple-test"),"tuple checking/emission is not idempotent");

  auto closure=parse("fn main() { let f = fn(x: i64) -> i64 { return x + 1; }; "
                     "let pair = (f, 41); let g = fn() -> i64 { let (operation, value) = pair; return operation(value); }; "
                     "assert(g() == 42); }");
  on1x::check(closure);auto& lambda=*closure.functions[0].body[2].value;
  require(lambda.captures.size()==1&&lambda.captures[0].name=="pair","tuple capture metadata was lost");
  require(lambda.captures[0].type.is_tuple()&&lambda.captures[0].type.has_functions(),"tuple ownership metadata was lost");

  auto cyclic=parse("fn bad(pair) -> _ { let (f, x) = pair; f(pair); return x; } fn main() {}");
  bool rejected=false;
  try{on1x::infer_types(cyclic);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("infinite type")!=std::string::npos;}
  require(rejected&&cyclic.functions[0].params[0].type==on1x::Type::Hole,"tuple occurs check failed or partially grounded HIR");

  auto bounded=parse("fn make() -> _ { return (1, true); } fn main() {}");
  on1x::InferenceLimits limits;limits.type_nodes=3;rejected=false;
  try{on1x::infer_types(bounded,limits);}catch(const on1x::Error& error){rejected=std::string(error.what()).find("expanded-size limit")!=std::string::npos;}
  require(rejected&&bounded.functions[0].result==on1x::Type::Hole,"outer tuple signature size was not bounded transactionally");
  limits.type_nodes=4;on1x::infer_types(bounded,limits);on1x::check(bounded);
  require(bounded.functions[0].result.is_tuple(),"inclusive tuple type budget failed");
  auto parameters=parse("fn sum((a, b)) -> _ { return a + b; } fn main() { println(sum((40, 2))); }");
  on1x::check(parameters);
  require(parameters.functions[0].params[0].pattern.has_value(),"parameter pattern was lost");
  require(on1x::type_name(parameters.functions[0].params[0].type)=="(i64, i64)","parameter shape was not inferred");
  auto before=on1x::emit_c(parameters,"tuple-test");on1x::check(parameters);
  require(before==on1x::emit_c(parameters,"tuple-test"),"parameter patterns are not idempotent");
  std::cout<<"Tuple HIR: structural grounding, patterns, captures and budgets PASS\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

#include <on1x/infer.hpp>
#include <iostream>

namespace {
void require(bool condition,const char* message) {
 if(!condition)throw std::runtime_error(message);
}
on1x::Unit parse(const std::string& source) {
 return on1x::parse(on1x::scan(source,"closure-test"));
}
}

int main() {
 try {
  auto unit=parse("fn factory(x) -> _ { let y = 2; "
                  "return fn(z) -> _ { return fn(w) -> _ { return x + y + z + w + x; }; }; } "
                  "fn main() { println(factory(10)(20)(0)); }");
  on1x::check(unit);
  auto& outer=*unit.functions[0].body[1].value;
  auto& inner=*outer.body[0].value;
  require(outer.captures.size()==2&&outer.captures[0].name=="x"&&outer.captures[1].name=="y",
          "transitive captures were not deduplicated in first-use order");
  require(inner.captures.size()==3&&inner.captures[2].name=="z","nested parameter capture was lost");
  for(const auto& capture:inner.captures)require(capture.type==on1x::Type::I64,"capture type was not grounded");
  require(outer.parameters[0].type==on1x::Type::I64,"lambda parameter type was not inferred");
  auto emitted=on1x::emit_c(unit,"closure-test");
  on1x::check(unit);
  require(outer.captures.size()==2&&inner.captures.size()==3,"checking duplicated capture metadata");
  require(emitted==on1x::emit_c(unit,"closure-test"),"closure emission changed on rechecking");

  auto scoped=parse("fn main() { let x = 1; let f = fn() -> i64 { let y = x; "
                    "{ let x = 2; let g = fn() -> i64 { return x; }; assert(g() == 2); } "
                    "return y; }; assert(f() == 1); }");
  on1x::check(scoped);
  auto& parent=*scoped.functions[0].body[1].value;
  auto& shadow=*parent.body[1].body[1].value;
  require(parent.captures.size()==1&&parent.captures[0].name=="x","outer lexical capture was lost");
  require(shadow.captures.size()==1&&shadow.captures[0].name=="x","shadowed local capture was lost");

  auto ambiguous=parse("fn main() { var f = fn(x) -> _ { return x; }; }");
  bool rejected=false;
  try { on1x::infer_types(ambiguous); } catch(const on1x::Error& error) {
   rejected=std::string(error.what()).find("cannot infer anonymous-function parameter x")!=std::string::npos;
  }
  auto& untyped=*ambiguous.functions[0].body[0].value;
  require(rejected&&untyped.type==on1x::Type::Invalid&&untyped.parameters[0].type==on1x::Type::Hole,
          "failed lambda inference partially grounded the HIR");
  std::cout<<"Closure HIR: captures, shadowing, grounding and idempotence PASS\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';return 1;
 }
}

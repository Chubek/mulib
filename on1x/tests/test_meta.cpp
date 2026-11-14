#include <on1x/meta.hpp>
#include <on1x/infer.hpp>
#include <iostream>
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
on1x::Unit parse(const std::string& text){return on1x::parse(on1x::scan(text,"meta-test"));}
}
int main(){
 try{
  auto unit=parse("macro m{rule($x:Expression)=>expression{$x+1};} fn main(){println(m!(41));}");
  on1x::expand_macros(unit);require(unit.macros.empty(),"macro declarations survived expansion");
  auto schema=on1x::meta::base_schema();auto tree=on1x::meta::encode(unit);schema.validate(tree);
  auto wire=on1x::meta::write_sexpr(tree);auto parsed=on1x::meta::read_sexpr(wire,"wire-test");schema.validate(parsed);
  require(on1x::meta::write_sexpr(parsed)==wire,"wire codec is not canonical");
  auto copy=on1x::meta::decode(parsed);on1x::check(unit);on1x::check(copy);
  require(on1x::emit_c(unit,"a")==on1x::emit_c(copy,"b"),"AST round trip changed generated C");
  on1x::check(copy);require(on1x::emit_c(unit,"a")==on1x::emit_c(copy,"b"),"metadata checking is not idempotent");
  auto bad=parse("macro m{rule()=>expression{m!()};} fn main(){m!();}");bool rejected=false;
  try{on1x::expand_macros(bad);}catch(const on1x::Error&){rejected=true;}
  require(rejected&&bad.macros.size()==1&&bad.functions[0].body[0].value->kind==on1x::Expr::Kind::MacroCall,"failed macro expansion was not transactional");
  auto ppx_bad=parse("macro m{rule()=>expression{42};} fn main(){m!();}");
  on1x::meta::PpxOptions opts;opts.programs.push_back({{"meta-test"},"missing","/on1x-nonexistent-ppx-for-transaction-test"});rejected=false;
  try{on1x::meta::rewrite(ppx_bad,opts);}catch(const on1x::Error&){rejected=true;}
  require(rejected&&ppx_bad.macros.size()==1&&ppx_bad.functions[0].body[0].value->kind==on1x::Expr::Kind::MacroCall,"failed PPX rewrite was not transactional");
  rejected=false;try{(void)on1x::meta::read_sexpr("()",std::string(4097,'x'));}catch(const on1x::Error&){rejected=true;}
  require(rejected,"wire diagnostic filenames were not bounded");
  for(const auto& malformed:{"() ()","(unclosed","\"bad\\q\""}){
   rejected=false;try{(void)on1x::meta::read_sexpr(malformed,"bad-wire");}catch(const on1x::Error&){rejected=true;}
   require(rejected,"malformed wire was accepted");
  }
   require(schema.subtype("HexLiteral","NumericLiteral")&&schema.subtype("IntegerLiteral","Expression")&&!schema.subtype("Literal","IntegerLiteral"),"syntax subtype hierarchy is incorrect");
   auto staged=parse("fn make(x:i64)->code<i64>{return quote{lift(x)+2};}fn main(){println(run(make(40)));}");
   auto stage_wire=on1x::meta::write_sexpr(on1x::meta::encode(staged));
   auto stage_copy=on1x::meta::decode(on1x::meta::read_sexpr(stage_wire,"stages"));
   on1x::check(staged);on1x::check(stage_copy);auto emitted=on1x::emit_c(staged,"a");
   require(staged.stages_lowered&&staged.functions[0].result==on1x::Type::code(on1x::Type::I64),"typed staging was not lowered");
   require(emitted==on1x::emit_c(stage_copy,"b"),"stage wire round trip changed C");
   on1x::check(staged);require(emitted==on1x::emit_c(staged,"a"),"staging checking is not idempotent");
   auto stage_bad=parse("fn main(){let x=1;let c=quote{x};}");
   auto before=on1x::meta::write_sexpr(on1x::meta::encode(stage_bad));rejected=false;
   try{on1x::infer_types(stage_bad);}catch(const on1x::Error&){rejected=true;}
   require(rejected&&!stage_bad.stages_lowered&&before==on1x::meta::write_sexpr(on1x::meta::encode(stage_bad)),"staging failure was not transactional");
   auto stage_budget=parse("fn main(){let c=quote{42};println(run(c));}");auto limits=on1x::InferenceLimits{};limits.nodes=0;rejected=false;
   try{on1x::infer_types(stage_budget,limits);}catch(const on1x::Error&){rejected=true;}
   require(rejected&&!stage_budget.stages_lowered&&stage_budget.functions[0].body[0].value->children[0]->kind==on1x::Expr::Kind::Integer,"post-staging inference failure changed the source tree");
  std::cout<<"Metaprogramming HIR, schema and codec: PASS\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

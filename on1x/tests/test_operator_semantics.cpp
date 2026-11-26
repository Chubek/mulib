#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/infer.hpp>
#include <on1x/meta.hpp>
#include <on1x/operator_lowering.hpp>
#include <on1x/sema.hpp>
#include "../hir/inspect_hir.hpp"
#include <algorithm>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;
using K=Expr::Kind;

Unit source(const std::string& text){return parse(scan(text,"operator-semantics.on1x"));}
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

template<class F>
void reject(F&& check,std::string_view id={},std::string_view message={}){
 try{check();}
 catch(const Error& e){
  if((!id.empty()&&e.diagnostic_id!=id)||std::string_view(e.what()).find(message)==std::string_view::npos)
   throw std::runtime_error("expected "+std::string(id)+" "+std::string(message)+
                            ", got "+e.diagnostic_id+": "+e.what());
  require(e.where.file=="operator-semantics.on1x","operator diagnostic lost its source location");
  return;
 }
 throw std::runtime_error("invalid operator was accepted: "+std::string(message));
}

const std::string binary=R"(
 operator (lexeme: "<+>", arity: "binary", precedence: 5,
           formals: left: i64, right: i64) -> i64 {
   return left*10+right;
 }
 fn main() -> i64 { return 4 <+> 2; }
)";

Expr& application(Unit& unit){return *unit.functions.front().body.front().value;}

void malformed_candidates(){
 const auto id="ON1X-OPERATOR-MALFORMED";
 const auto unresolved="ON1X-OPERATOR-UNRESOLVED";
 auto wrong_operand_count=source(binary);
 application(wrong_operand_count).children.pop_back();
 reject([&]{check(wrong_operand_count);},id,"invalid source expression");
 require(application(wrong_operand_count).kind==K::OperatorBinary,"malformed lowering published an ordinary call");

 auto bogus_callee=source(binary);
 auto extra=std::make_unique<Expr>();
 extra->kind=K::Integer;extra->where=application(bogus_callee).where;extra->text="1";
 application(bogus_callee).callee=std::move(extra);
 reject([&]{check(bogus_callee);},id,"invalid source expression");

 auto forged_spelling=source(binary);
 application(forged_spelling).text="<*>";
 reject([&]{check(forged_spelling);},id,"invalid source expression");
 auto forged_type=source(binary);
 application(forged_type).type=Type::I64;
 reject([&]{check(forged_type);},id,"invalid source expression");

 auto missing_identity=source(binary);
 application(missing_identity).operator_identity.clear();
 reject([&]{check(missing_identity);},unresolved,"unresolved operator");
 auto wrong_identity=source(binary);
 application(wrong_identity).operator_identity="@operator_wrong";
 reject([&]{infer_types(wrong_identity);},unresolved,"unresolved operator");

 auto orphan=source(binary);
 orphan.operators.clear();
 reject([&]{infer_types(orphan);},unresolved,"unresolved operator");
 reject([&]{check(orphan);},unresolved,"unresolved operator");
 auto malformed_declaration=source(binary);
 malformed_declaration.operators.front().function.external=true;
 reject([&]{check(malformed_declaration);},id,"invalid callable declaration");
 auto long_declaration=source(binary);
 long_declaration.operators.front().header.lexeme="^^^^^^^";
 reject([&]{check(long_declaration);},id,"invalid operator header");

 auto bad_after_valid=source(binary+"fn another() -> i64 { return 7 <+> 8; }");
 auto* untouched=application(bad_after_valid).children[0].get();
 bad_after_valid.functions[1].body.front().value->operator_identity.clear();
 reject([&]{(void)meta::desugar_source_operators(bad_after_valid);},unresolved,"unresolved operator");
 require(bad_after_valid.operators.size()==1&&application(bad_after_valid).kind==K::OperatorBinary&&
         application(bad_after_valid).children[0].get()==untouched&&application(bad_after_valid).operator_parse,
         "late validation failure published a partial source transaction");

 auto pending=source(binary);
 pending.ppx.push_back({application(pending).where,"test","must-not-execute"});
 reject([&]{infer_types(pending);},{},"source PPX registrations require");
 require(pending.operators.size()==1&&application(pending).kind==K::OperatorBinary,
         "direct inference consumed source before pending PPX admission");
}

void source_rule_program(){
 auto unit=source(R"(
  operator(lexeme:"~>", arity:"unary", position:"postfix", formals:n:i64) -> i64 {return n;}
  operator(lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 {return a+b;}
  operator(lexeme:"??", lexeme2:"|>", arity:"ternary", precedence:2,
           formals:test:bool,a:i64,b:i64) -> i64 {if(test){return a;} return b;}
  infix 4 <+>;
  fn mark(x:i64) -> i64 {println(x);return x;}
  fn main() -> i64 {
    let choose:fn(bool,i64,i64)->i64=operator("??");
    return false ?? (mark(1) ~>) |> (mark(40) <+> mark(2));
  }
 )");
 auto& body=unit.functions[1].body;
 auto& use=*body[1].value;
 const auto identity=unit.operators.back().identity;
 const auto where=use.where.offset;
 auto* condition=use.children[0].get();
 auto* first=use.children[1].get();
 auto* second=use.children[2].get();
 const auto report=meta::desugar_source_operators(unit);
 require(report.verified&&report.program_sha256.size()==64&&report.rules.size()==6&&
         report.rewrites==8&&report.definitions==3&&report.fixities==1,
         "On1xMETA did not report all mandatory source forms");
 auto hits=[&](std::string_view name){
  const auto hit=std::find_if(report.rules.begin(),report.rules.end(),[&](const auto& r){return r.name==name;});
  require(hit!=report.rules.end()&&hit->file=="operators.o1m"&&hit->line>0,"missing authored source rule provenance");
  return hit->count;
 };
 require(hits("operator_definition")==3&&hits("operator_reference")==1&&hits("operator_ternary")==1,
         "source rule hit counts are not structural rewrites");
 require(use.kind==K::Call&&use.type==Type::Invalid&&use.where.offset==where&&
         use.callee->text==identity&&use.callee->reference==Expr::Reference::Unresolved&&
         use.children[0].get()==condition&&use.children[1].get()==first&&use.children[2].get()==second,
         "META did not move original operands in order into an unchecked ordinary call");
 const auto& reference=*unit.functions[1].body[0].value;
 require(reference.kind==K::Variable&&reference.text==identity&&!reference.callee,
         "META did not produce a normal first-class reference");
 require(!use.operator_parse&&!use.operator_grouped&&!first->operator_parse&&!second->operator_grouped&&
         !unit.functions[3].body[0].value->operator_parse,
         "source fixity provenance escaped the terminal phase");
 meta::require_core(unit);
 require(meta::desugar_source_operators(unit).rewrites==0,"source desugaring is not idempotent");
 std::size_t budget=262144;
 auto scalar=clone_unit(unit,budget);
 scalar.functions[1].body.erase(scalar.functions[1].body.begin());
 check(unit);
 require(!emit_c(unit,"operator-semantics.on1x").empty(),"ordinary first-class reference could not emit C11");
 // The scalar MIR profile does not support function-valued locals.
 check(scalar);
 auto output=mir::evaluate(mir::lower(scalar));
 require(output.value==42&&output.output=="1\n40\n2\n","ordinary calls lost eager source operand order");
}

void direct_macro_operator(){
 auto unit=source(R"(
  macro compute {
   rule() => block {
    operator(lexeme:"%%",arity:"binary",formals:a:i64,b:i64)->i64{return a*10+b;}
    return 4 %% 2;
   };
  }
  fn main() -> i64 {compute!();}
 )");
 require(unit.operators.empty()&&!unit.operator_entry.empty(),"macro fixture did not begin with staged syntax");
 infer_types(unit);
 require(unit.operators.empty()&&unit.macros.empty()&&unit.functions.size()==2&&
         unit.functions.back().name.find("_m")!=std::string::npos&&application(unit).kind==K::Call,
         "direct inference skipped source META for a hygienic macro-generated operator");
 check(unit);
 require(mir::evaluate(mir::lower(unit)).value==42,"macro-generated definition lost ordinary callable behavior");
}
void ordinary_checked_abi(){
 auto unit=source(binary);
 const auto identity=unit.operators.front().identity;
 const auto source_location=application(unit).where;
 require(application(unit).kind==K::OperatorBinary,"parser did not retain source operator");
 require(!identity.empty()&&application(unit).operator_identity==identity,"source operator has no host binding");
 reject([&]{meta::require_core(unit);});
 reject([&]{recheck_checked_hir(unit);});
 reject([&]{(void)emit_c(unit,"operator-semantics.on1x");});

 std::size_t budget=262144;
 auto original=clone_unit(unit,budget);
 infer_types(unit);
 require(unit.operators.empty()&&application(unit).kind==K::Call&&
         application(unit).callee->text==identity,"direct inference did not lower to an ordinary call");
 require(application(original).kind==K::OperatorBinary,"source clone was not isolated");

 diagnostic::Registry registry;
 analysis::Context context{diagnostic::Catalog::embedded(),registry,{},{}};
 processor::Report report;
 processor::RuleSelection selection;
 selection.default_enabled=false;
 check(original,context,selection,&report);
 require(!registry.has_errors()&&report.verified,"source desugaring failed with checked rules disabled");
 require(report.source_desugar.verified&&report.source_desugar.rewrites==2&&
         report.source_desugar.definitions==1&&report.source_desugar.program_sha256.size()==64,
         "semantic check did not publish mandatory source statistics");
 const auto json=processor::report(report);
 require(json.find("\"source_desugar\":{\"program_sha256\":\"")!=std::string::npos&&
         json.find("\"name\":\"operator_binary\",\"count\":1,\"file\":\"operators.o1m\"")!=std::string::npos,
         "processor report lost source rule provenance");
 require(original.operators.empty()&&original.fixity_directives.empty()&&
         original.operator_scopes.empty()&&original.operator_owners.empty()&&
         original.operator_entry.empty()&&original.operator_lazy_functions.empty(),
         "source operator metadata escaped into checked HIR");
 const auto& call=application(original);
 require(call.kind==K::Call&&call.callee&&call.callee->kind==K::Variable&&
         call.callee->reference==Expr::Reference::Function&&call.callee->text==identity&&
         call.operator_identity.empty()&&call.children.size()==2&&call.type==Type::I64,
         "checked application is not an ordinary resolved call");
 require(call.where.file==source_location.file&&call.where.offset==source_location.offset&&
         call.callee->where.offset==source_location.offset,"checked call lost its application location");
 const auto hidden=std::find_if(original.functions.begin(),original.functions.end(),
                                [&](const Function& f){return f.name==identity;});
 require(hidden!=original.functions.end()&&hidden->params[0].name=="left"&&
         hidden->params[1].name=="right"&&!hidden->external&&!hidden->prototype,
         "checked operator lost the original callable's formal parameters");
 recheck_checked_hir(original);
 require(hir::inspection::write_checked(original).find("(call")!=std::string::npos,
         "checked HIR did not expose an ordinary call");
 const auto module=mir::lower(original);
 const auto f=std::find_if(module.functions.begin(),module.functions.end(),
                           [&](const mir::Function& fn){return fn.name==identity;});
 require(f!=module.functions.end(),"operator callable not lowered to MIR");
 const auto main=std::find_if(module.functions.begin(),module.functions.end(),
                              [](const mir::Function& fn){return fn.name=="main";});
 require(main!=module.functions.end(),"main not lowered to MIR");
 bool call_has_effect=false;
 for(const auto& block:main->blocks)for(const auto& instruction:block.instructions)
  if(instruction.op==mir::Op::Call&&instruction.target==f->id)
   call_has_effect=instruction.effect_in&&instruction.effect_out&&
                   instruction.effect_in!=instruction.effect_out&&instruction.operands.size()==2;
 require(call_has_effect,"ordinary MIR call lost its effect chain or operands");
 require(mir::evaluate(mir::deserialize(mir::serialize(module))).value==42,"ordinary MIR call evaluated incorrectly");
}

void inference_profiles(){
 auto unit=source(R"(
  cliche Number { fn number(x: Self) -> i64; }
  impl Number for i64 { fn number(x: i64) -> i64 { return x; } }
  fn add(left:i64,right:i64) -> i64 { return left+right; }
  operator (lexeme: "<+>", arity: "binary", precedence: 5, formals: left, right) {
    return add(left,right);
  }
  operator<T> (lexeme: "%%", arity: "binary", precedence: 4, formals: left:T,right:T) -> T {
    return right;
  }
  operator<T> (lexeme: "<*>", arity: "binary", precedence: 6, formals: left:T,right:i64)
   -> i64 where Number<T> {
    return Number<T>::number(left)*right;
  }
  fn main() -> i64 {
    if (false %% true) { return (1 <*> 2) <+> (0 %% 40); }
    return 0;
  }
 )");
 auto inferred=unit.operators[0].identity;
 auto generic=unit.operators[1].identity;
 check(unit);
 auto lookup=[&](const std::string& name)->const Function&{
  auto f=std::find_if(unit.functions.begin(),unit.functions.end(),[&](const Function& item){return item.name==name;});
  require(f!=unit.functions.end(),"operator declaration did not survive as a function");
  return *f;
 };
 require(!lookup(inferred).prototype&&lookup(inferred).params[0].type==Type::I64&&
         lookup(inferred).result==Type::I64,"unannotated operator did not infer its callable signature");
 require(lookup(generic).prototype&&lookup(generic).result.has_generics(),
         "explicit generic operator lost its checked prototype");
 require(mir::evaluate(mir::lower(unit)).value==42,"generic/constraint specialization changed ordinary MIR behavior");

 auto first_class=source(R"(
  operator<T>(lexeme: "%%", arity: "binary", formals: left:T,right:T) -> T { return right; }
  fn main() -> i64 {
    let choose:fn(i64,i64)->i64=operator("%%");
    return choose(0,42);
  }
 )");
 check(first_class);
 require(!emit_c(first_class,"operator-semantics.on1x").empty(),"first-class operator did not emit direct C11");
}

void nominal_ownership(){
 const std::string declaration=R"(
  record Cell { n:i64 }
  operator (lexeme: "%%", arity: "binary", formals: owner:own<Cell>,delta:i64) -> own<Cell> {
    let old=take(owner);
    return box(record<Cell>{n:old["n"]+delta});
  }
 )";
 auto unit=source(declaration+R"(
  fn main() -> i64 {
    let first=box(record<Cell>{n:39});
    let second=move(first) %% 3;
    let payload=take(second);
    return payload["n"];
  }
 )");
 check(unit);
 require(!emit_c(unit,"operator-semantics.on1x").empty(),"owned operator did not emit direct C11");
 auto duplicate=source(declaration+R"(
  fn main() {
    let first=box(record<Cell>{n:39});
    let second=move(first) %% 3;
    drop(first);
    drop(second);
  }
 )");
 reject([&]{check(duplicate);},{},"use after move");

 auto unused=source(R"(
  record Cell { n:i64 }
  operator (lexeme: "%%", arity:"unary", formals: owner:own<Cell>) -> i64 {
   drop(owner);
   drop(owner);
   return 0;
  }
  fn main(){}
 )");
 reject([&]{check(unused);},{},"use after move");
}

void lexical_loans(){
 const std::string declaration=R"(
  operator (lexeme:"<+>",arity:"binary",formals:value:ref i64,delta:i64) -> i64 {
    value:=value+delta;
    return value;
  }
 )";
 auto unit=source(declaration+R"(
  fn main() -> i64 {
   var value=40;
   let result=value <+> 2;
   return value+result-42;
  }
 )");
 check(unit);
 const auto& call=*unit.functions.front().body[1].value;
 require(call.kind==K::Call&&call.callee->reference==Expr::Reference::BorrowFunction,
         "borrowed operator did not retain ordinary direct loan ABI");
 require(!emit_c(unit,"operator-semantics.on1x").empty(),"borrowed operator did not emit direct C11");
 reject([&]{(void)mir::lower(unit);},"ON1X-MIR-UNSUPPORTED","borrowed parameters");

 auto overlap=source(declaration+"fn main(){var n=1;println(n <+> n);}");
 reject([&]{check(overlap);},"ON1X-REF","exclusive");
 auto escaping=source(declaration+"fn main(){let f=operator(\"<+>\");println(f(1,2));}");
 reject([&]{check(escaping);},"ON1X-REF","direct named call");
}

void unused_operators(){
 auto bad=source(R"(
  operator (lexeme: "<+>",arity:"binary",formals:left:i64,right:i64) -> i64 {
    return false;
  }
  fn main(){}
 )");
 reject([&]{check(bad);});
 auto bad_generic=source(R"(
  operator<T>(lexeme: "%%",arity:"binary",formals:left:T,right:T) -> T {
    return 0;
  }
  fn main(){}
 )");
 reject([&]{check(bad_generic);},{},"generic function body specializes type parameter");
}

void library_check(){
 auto unit=source(R"(
  operator(lexeme:"<+>",arity:"binary",formals:left:i64,right:i64)->i64 {
   return left+right;
  }
 )");
 const auto identity=unit.operators.front().identity;
 diagnostic::Registry registry;
 analysis::Context context{diagnostic::Catalog::embedded(),registry,{},{}};
 check_library(unit,context,{});
 require(!registry.has_errors()&&unit.operators.empty()&&unit.functions.size()==1&&
         unit.functions.front().name==identity&&unit.functions.front().result==Type::I64,
         "library checking did not publish the ordinary operator signature");
}
}

int main(){
 try{
  malformed_candidates();
  source_rule_program();
  direct_macro_operator();
  ordinary_checked_abi();
  inference_profiles();
  nominal_ownership();
  lexical_loans();
  unused_operators();
  library_check();
  return 0;
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';
  return 1;
 }
}

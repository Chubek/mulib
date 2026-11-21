#include <on1x/sema.hpp>
#include <on1x/clone.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;
void require(bool ok,const char* message){
 if(!ok){std::cerr<<message<<'\n';std::exit(1);}
}
Unit source(const std::string& text){
 return parse(scan(text,"semantic-fixture.on1x"));
}
Unit checked(const std::string& text){
 auto unit=source(text);check(unit);return unit;
}
void rejects(const std::function<void()>& operation,std::string_view id,
             std::string_view message,std::size_t line=1){
 try{operation();}
 catch(const Error& e){
  require(e.diagnostic_id==id,"wrong semantic diagnostic code");
  require(std::string_view(e.what()).find(message)!=std::string_view::npos,
          "missing semantic rule diagnostic/provenance");
  require(e.where.file=="semantic-fixture.on1x"&&e.where.line==line,
          "semantic diagnostic lost source location");
  return;
 }
 std::cerr<<"expected rejection: "<<message<<'\n';
 require(false,"malformed semantic HIR was accepted");
}
ExprPtr literal(Expr::Kind kind,std::string text){
 auto e=std::make_unique<Expr>();e->kind=kind;e->text=std::move(text);
 e->where={"semantic-fixture.on1x",7,2,3};return e;
}
Unit expression(ExprPtr value){
 Unit unit;Function f;f.name="main";f.where={"semantic-fixture.on1x"};
 Stmt s;s.kind=Stmt::Kind::Expr;s.where=value->where;s.value=std::move(value);
 f.body.push_back(std::move(s));unit.functions.push_back(std::move(f));return unit;
}
void structure(){
 auto malformed=literal(Expr::Kind::PatternMatch,"");
 auto unit=expression(std::move(malformed));
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural);},
         "ON1X-PROCESSOR-SEMANTIC","match_arms",2);
 unit=expression(literal(Expr::Kind::Case,""));
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural);},
         "ON1X-PROCESSOR-SEMANTIC","case_operands",2);
 auto unary=literal(Expr::Kind::Unary,"!");
 unary->children.push_back(literal(Expr::Kind::Boolean,"true"));
 Stmt hidden;hidden.kind=Stmt::Kind::Return;hidden.where=unary->where;
 unary->body.push_back(std::move(hidden));unit=expression(std::move(unary));
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural);},
         "ON1X-PROCESSOR-SEMANTIC","unary_no_body",2);
 auto binary=literal(Expr::Kind::Binary,"+");
 binary->children.push_back(literal(Expr::Kind::Integer,"1"));
 binary->children.push_back(nullptr);unit=expression(std::move(binary));
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural);},
         "ON1X-PROCESSOR-SEMANTIC","missing semantic expression operand",2);
 unit=source("fn main() { if true {} }");
 unit.functions[0].body[0].value.reset();
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural);},
         "ON1X-PROCESSOR-SEMANTIC","if_condition");
}
void typed(){
 auto binary=literal(Expr::Kind::Binary,"==");binary->type=Type::Bool;
 binary->children.push_back(literal(Expr::Kind::Integer,"1"));
 binary->children.push_back(literal(Expr::Kind::Boolean,"true"));
 binary->children[0]->type=Type::I64;binary->children[1]->type=Type::Bool;
 auto unit=expression(std::move(binary));
 // Same shapes are valid before inference; typed annotations are not.
 processor::validate(unit,processor::SemanticStage::Structural);
 rejects([&]{processor::validate(unit,processor::SemanticStage::Typed);},
         "ON1X-PROCESSOR-SEMANTIC","binary_operand_types",2);
 auto unary=literal(Expr::Kind::Unary,"!");unary->type=Type::I64;
 unary->children.push_back(literal(Expr::Kind::Boolean,"true"));
 unary->children[0]->type=Type::Bool;unit=expression(std::move(unary));
 rejects([&]{processor::validate(unit,processor::SemanticStage::Typed);},
         "ON1X-PROCESSOR-SEMANTIC","unary_result_type",2);
 unit=checked("fn main() { let flag = true; assert(flag); }");
 unit.functions[0].body[0].value->type=Type::I64;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Typed);},
         "ON1X-PROCESSOR-SEMANTIC","boolean_type");
 require(unit.functions[0].body[0].value->type==Type::I64,"validation repaired forged type");
 unit=checked("fn main() { let flag = true; assert(flag); }");
 unit.functions[0].body[1].value->children[0]->reference=Expr::Reference::Unresolved;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Typed);},
         "ON1X-PROCESSOR-SEMANTIC","variable_resolved");
 unit=checked("fn main() { if true { println(1); } }");
 unit.functions[0].body[0].value->type=Type::I64;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Typed);},
         "ON1X-PROCESSOR-SEMANTIC","if_condition");
}
void budgets(){
 auto unit=source("fn main() { println(1); }");
 processor::Limits limits;limits.nodes=0;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural,limits);},
         "ON1X-PROCESSOR-LIMIT","node budget");
 limits={};limits.steps=0;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural,limits);},
         "ON1X-PROCESSOR-LIMIT","work budget");
 limits={};limits.depth=0;
 rejects([&]{processor::validate(unit,processor::SemanticStage::Structural,limits);},
         "ON1X-PROCESSOR-LIMIT","depth/node budget");
}
void integration(){
 diagnostic::Registry registry;
 analysis::Context context{diagnostic::Catalog::embedded(),registry,{},{}};
 processor::Report evidence;
 const auto text="fn main() { let x = 40 + 0; assert(!!true && true); println(x + 2); }";
 auto unit=source(text);check(unit,context,evidence);
 require(evidence.verified,"processor evidence did not commit");
 require(evidence.structural.checks>0&&evidence.typed_before.checks>0&&
         evidence.typed_after.checks>0,"semantic constraints were not executed");
 require(evidence.desugar.rewrites+evidence.canonicalize.rewrites>0,"source rules did not execute");
 const auto first=processor::report(evidence);
 require(first.find("\"verified\":true")!=std::string::npos&&
         first.find("semantic.o1m")!=std::string::npos,"processor report lost rule provenance");
 require(first.find("\"program_sha256\":\"")!=std::string::npos&&
         first.find("\"parser_sha256\":\"")!=std::string::npos,"processor report lost build fingerprints");
 auto second=source(text);processor::Report repeated;check(second,context,repeated);
 require(first==processor::report(repeated),"processor report is nondeterministic");
 auto invalid=source("fn main() { if false { println(missing + 0); } }");
 try{check(invalid,context,evidence);require(false,"dead invalid source was erased");}
 catch(const Error&){require(!evidence.verified,"failure published stale successful evidence");}
 // A canonical round trip exercises the SDK's already-checked-HIR route.
 const auto emitted=emit_c(unit,"semantic-fixture.on1x");
 require(!emitted.empty(),"checked processor result cannot emit C11");
}
}
int main(){
 structure();typed();budgets();integration();
 std::cout<<"semantic processor contracts: OK\n";
}

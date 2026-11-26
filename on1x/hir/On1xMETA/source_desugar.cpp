#include "source_desugar.hpp"
#include <on1x_meta_source.hpp>

namespace on1x::meta::source {
namespace {
using K=Expr::Kind;
[[noreturn]] void invalid(const Location& at){
 throw Error(at,"invalid generated On1xMETA source desugar recipe","ON1X-PROCESSOR");
}
}

Form form_of(K kind){
 switch(kind){
  case K::OperatorReference:return Form::OperatorReference;
  case K::OperatorUnary:return Form::OperatorUnary;
  case K::OperatorBinary:return Form::OperatorBinary;
  case K::OperatorTernary:return Form::OperatorTernary;
  default:invalid({"<On1xMETA source>"});
 }
}
const Rule& select(Form form,const Location& at){
 for(const auto& rule:generated::rules)if(rule.form==form)return rule;
 invalid(at);
}
Budgets limits(){return generated::budgets;}

ExpressionEdit Program::expression(Expr& value,const std::string& binding,const Rule& rule){
 if(rule.form!=form_of(value.kind)||rule.captures!=value.children.size())
  invalid(value.where);
 Expr variable;
 variable.kind=K::Variable;
 variable.where=value.where;
 variable.text=binding;
 if(rule.replacement==Replacement::BoundOperator&&rule.captures==0){
  variable.syntax_depth=value.syntax_depth;
  hit(rule);
  return {&value,std::move(variable),&rule};
 }
 if(rule.replacement!=Replacement::Call||!rule.captures||rule.captures>rule.order.size())
  invalid(value.where);
 Expr call;
 call.kind=K::Call;call.where=value.where;call.syntax_depth=value.syntax_depth;
 call.callee=std::make_unique<Expr>(std::move(variable));
 call.children.resize(rule.captures);
 for(std::size_t i=0;i<rule.captures;++i)
  if(rule.order[i]!=i)invalid(value.where); // eager affine capture trace
 hit(rule);
 return {&value,std::move(call),&rule};
}

void ExpressionEdit::publish() noexcept{
 // Slots already exist, and the original operands are transferred unchanged.
 for(std::size_t i=0;i<ordinary.children.size();++i)
  ordinary.children[i]=std::move(target->children[rule->order[i]]);
 *target=std::move(ordinary);
}

void Program::hit(const Rule& rule){
 static_assert(generated::rules.size()==std::tuple_size_v<decltype(hits)>);
 ++hits[static_cast<std::size_t>(&rule-generated::rules.data())];
}
DefinitionEdit Program::definition(OperatorDeclaration& value){
 const auto& rule=select(Form::OperatorDeclaration,value.header.where);
 if(rule.replacement!=Replacement::DefineOperator||rule.captures!=1||rule.order[0]!=0)
  invalid(value.header.where);
 hit(rule);
 return {&value};
}
void Program::fixity(const FixityDirective& value){
 const auto& rule=select(Form::FixityDirective,value.where);
 if(rule.replacement!=Replacement::Consumed||rule.captures!=0)invalid(value.where);
 hit(rule);
}
SourceDesugarReport Program::report(std::size_t definitions) const{
 SourceDesugarReport result;
 result.program_sha256=generated::program_sha256;
 for(std::size_t i=0;i<hits.size();++i)if(hits[i]){
  const auto& rule=generated::rules[i];
  result.rules.push_back({rule.name,rule.file,hits[i],rule.line,rule.column});
  result.rewrites+=hits[i];
  if(rule.form==Form::FixityDirective)result.fixities=hits[i];
 }
 result.definitions=definitions;
 result.verified=true;
 return result;
}
void DefinitionEdit::publish(std::vector<Function>& output) noexcept{
 target->function.name=std::move(target->identity);
 output.push_back(std::move(target->function));
}
}

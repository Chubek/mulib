#include <on1x/operator_lowering.hpp>
#include <on1x/operator_scan.h>
#include <on1x/operator_scope.hpp>
#include "../hir/On1xMETA/source_desugar.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace on1x {
namespace {
using K=Expr::Kind;
namespace source=meta::source;

[[noreturn]] void malformed(const Location& at,const std::string& message){
 throw Error(at,message,"ON1X-OPERATOR-MALFORMED");
}
[[noreturn]] void unresolved(const Expr& expression){
 throw Error(expression.where,"unresolved operator '"+expression.text+"'",
             "ON1X-OPERATOR-UNRESOLVED");
}
bool valid_spelling(const std::string& text){
 // The scanner can consume a longer punctuation run in one token, but the
 // declaration grammar admits only spellings of two to six bytes.
 return text.size()>=2&&text.size()<=6&&
        on1x_operator_width(text.data(),text.size())==text.size();
}
bool valid_associativity(OperatorAssociativity binds){
 return binds==OperatorAssociativity::Left||binds==OperatorAssociativity::Right;
}
bool valid_position(OperatorPosition position){
 return position==OperatorPosition::Prefix||position==OperatorPosition::Postfix||
        position==OperatorPosition::Infix;
}
std::size_t arity(const OperatorHeader& header){
 switch(header.arity){
  case OperatorArity::Unary:return 1;
  case OperatorArity::Binary:return 2;
  case OperatorArity::Ternary:return 3;
 }
 malformed(header.where,"invalid operator declaration arity");
}
void check_declaration(const OperatorDeclaration& declaration){
 const auto& header=declaration.header;
 const auto& function=declaration.function;
 const auto count=arity(header);
 if(!valid_spelling(header.lexeme)||
    (count==3?(!valid_spelling(header.lexeme2)||header.lexeme==header.lexeme2):!header.lexeme2.empty())||
    !valid_position(header.position)||
    (count==1?header.position==OperatorPosition::Infix:header.position!=OperatorPosition::Infix)||
    header.precedence>9||header.tightness>9||!valid_associativity(header.binds))
  malformed(header.where,"invalid operator header");
 if(declaration.identity.empty()||declaration.identity!=operator_identity(header))
  throw Error(header.where,"operator declaration has no valid host identity: '"+header.lexeme+"'",
              "ON1X-OPERATOR-UNRESOLVED");
 if((!function.name.empty()&&function.name!=declaration.identity)||
    function.external||function.procedure||function.prototype||
    !function.kernel_symbol.empty()||function.kernel_link_only||function.extension_generated||
    function.meta_canonicalized||function.meta_folded_identity||
    !function.attributes.empty()||function.params.size()!=count)
  malformed(header.where,"invalid callable declaration for operator '"+header.lexeme+"'");
 for(const auto& parameter:function.params){
  if(parameter.name.empty()||parameter.pattern||
     (parameter.borrow!=BorrowMode::Value&&parameter.borrow!=BorrowMode::Shared&&
      parameter.borrow!=BorrowMode::Exclusive))
   malformed(parameter.where,"invalid operator formal");
 }
}

// Bridge between resolved host AST and the restricted On1xMETA source forms.
// It owns admission, provider reachability and the transaction, not recipes.
class SourceBridge {
 using Edges=std::set<std::string_view>;
 // Keys point into declarations, which stay in place until rewriting finishes.
 std::map<std::string_view,const OperatorHeader*> declarations;
 // Only the loader's automatic providers are optional. All ordinary source
 // declarations (including unused operators) are roots and must be checked.
 const std::set<std::string>& lazy;
 std::map<std::string_view,Edges> dependencies;
 Edges roots;
 Edges* references=&roots;
 std::vector<source::ExpressionEdit> uses;
 std::vector<Expr*> provenance;
 source::Program program;
 source::Budgets limits=source::limits();

 void tick(const Location& at,std::size_t depth){
  if(depth>std::min<std::size_t>(limits.depth,2*max_syntax_depth+8)||!limits.nodes||!limits.steps)
   throw Error(at,"operator source desugar depth/node budget exhausted","ON1X-OPERATOR-LIMIT");
  --limits.nodes;--limits.steps;
 }
 void reference(const std::string& name){
  if(const auto target=lazy.find(name);target!=lazy.end())
   references->insert(*target);
 }
 void use(Expr& value){
  const auto found=declarations.find(value.operator_identity);
  if(found==declarations.end())unresolved(value);
  const auto& header=*found->second;
   const auto& rule=source::select(source::form_of(value.kind),value.where);
   const bool is_reference=value.kind==K::OperatorReference;
   const auto operands=rule.captures;
  // A reference has no operands and may carry just the primary spelling.
  // PPX must restore the identity, but its wire reference need not encode fixity.
  const bool position_matches=value.operator_position==header.position||
                              (is_reference&&value.operator_position==OperatorPosition::Infix);
  const bool second_matches=value.operator_lexeme2==header.lexeme2||
                            (is_reference&&value.operator_lexeme2.empty());
  if(value.text!=header.lexeme||!second_matches||!position_matches||
     (!is_reference&&operands!=arity(header))||value.children.size()!=operands||
     value.callee||!value.body.empty()||!value.proofs.empty()||
     !value.labels.empty()||!value.patterns.empty()||!value.parameters.empty()||
     !value.type_parameters.empty()||!value.constraints.empty()||
     !value.cliche_arguments.empty()||!value.member_type_arguments.empty()||
     !value.cliche_member.empty()||!value.captures.empty()||value.syntax_tail||
     !value.syntax_definitions.empty()||value.syntax_repeat||
     value.type!=Type::Invalid||value.result!=Type::Void||
     value.reference!=Expr::Reference::Unresolved||value.borrow!=BorrowMode::Value||
      value.prototype||value.noreturn||value.move_out||value.unsafe_region||
      value.statement_match||value.sum_match||value.procedure||value.projection||
      value.control_target||value.context){
    malformed(value.where,"invalid source expression for operator '"+value.text+"'");
   }
   uses.push_back(program.expression(value,value.operator_identity,rule));
  reference(value.operator_identity);
 }
 void expression(Expr& value,std::size_t depth){
  tick(value.where,depth);
  if(is_source_operator(value.kind))use(value);
  else{
   if(value.operator_parse||value.operator_grouped)provenance.push_back(&value);
   if(!lazy.empty()&&value.kind==K::Variable)reference(value.text);
  }
   if(value.syntax_tail||!value.syntax_definitions.empty()||value.kind==K::MacroCall||
      value.kind==K::SyntaxLet||value.kind==K::SyntaxHole||value.kind==K::Extension||
      value.kind==K::SyntaxBlock||value.kind==K::SyntaxList||value.kind==K::SyntaxVector||
      value.kind==K::SyntaxKeyword)
    throw Error(value.where,"unhandled metaprogramming syntax"+(value.text.empty()?"":": "+value.text));
   if(value.callee)expression(*value.callee,depth+1);
  for(auto& child:value.children){
   if(!child)malformed(value.where,"missing expression operand during operator lowering");
   expression(*child,depth+1);
  }
  for(auto& proof:value.proofs){
   if(!proof)malformed(value.where,"missing proof expression during operator lowering");
   expression(*proof,depth+1);
  }
  body(value.body,depth+1);
 }
 void statement(Stmt& value,std::size_t depth){
  tick(value.where,depth);
  if(value.syntax_operator)malformed(value.where,"unexpanded macro operator declaration");
  if(value.value)expression(*value.value,depth+1);
  body(value.body,depth+1);
  body(value.otherwise,depth+1);
  for(auto& instance:value.instances){
   if(!instance)malformed(value.where,"missing statement instance during operator lowering");
   statement(*instance,depth+1);
  }
 }
 void body(std::vector<Stmt>& statements,std::size_t depth){
  for(auto& value:statements)statement(value,depth);
 }
 void function(Function& value){
  tick(value.where,0);
  for(auto& attribute:value.attributes){
   if(!attribute)malformed(value.where,"missing function attribute during operator lowering");
   expression(*attribute,1);
  }
  body(value.body,1);
 }
 void definition(Function& function,const std::string& name){
  if(const auto target=lazy.find(name);target!=lazy.end())
   references=&dependencies[*target];
  this->function(function);
  references=&roots;
 }
 Edges demanded()const{
  Edges result=roots;
  std::vector<std::string_view> pending(result.begin(),result.end());
  while(!pending.empty()){
   auto next=pending.back();pending.pop_back();
   const auto edges=dependencies.find(next);
   if(edges!=dependencies.end())
    for(auto name:edges->second)if(result.insert(name).second)pending.push_back(name);
  }
  return result;
 }
public:
  explicit SourceBridge(const std::set<std::string>& optional):lazy(optional){}
  meta::SourceDesugarReport run(Unit& unit){
   std::set<std::string_view> names;
   std::vector<source::DefinitionEdit> definitions;
   definitions.reserve(unit.operators.size());
    if(!unit.operators.empty())
     for(const auto& existing:unit.functions)names.insert(existing.name);
    for(auto& declaration:unit.operators){
     tick(declaration.header.where,0);
     check_declaration(declaration);
    if(names.contains(declaration.identity)||
       !declarations.emplace(declaration.identity,&declaration.header).second)
     malformed(declaration.header.where,"duplicate or colliding operator declaration identity");
    definitions.push_back(program.definition(declaration));
    }
    for(const auto& directive:unit.fixity_directives){
     tick(directive.where,0);
     if(!valid_spelling(directive.lexeme)||directive.tightness>9||
       !valid_associativity(directive.binds))
     malformed(directive.where,"invalid operator fixity directive");
    program.fixity(directive);
    }
    for(auto& value:unit.functions)definition(value,value.name);
    for(auto& declaration:unit.operators)definition(declaration.function,declaration.identity);
   // Preparation copies member bodies to functions. Desugar the originals so
   // neither executable copies nor retained definition metadata contain sugar.
   for(auto& implementation:unit.cliche_implementations){
    for(auto& member:implementation.members){
     function(member.function);
     if(member.value)expression(*member.value,1);
    }
   }
   for(auto& implementation:unit.inherent_implementations){
    for(auto& member:implementation.members){
     function(member.function);
     if(member.value)expression(*member.value,1);
    }
   }
    // PPX can synthesize references to providers, so compute demand only once
    // the source AST is final. A reached provider brings in its helper closure.
   auto live=demanded();
   auto unused=[&](const std::string& name){return lazy.contains(name)&&!live.contains(name);};
   std::erase_if(definitions,[&](const auto& edit){return unused(edit.target->identity);});
   auto report=program.report(definitions.size());
   // All checks and allocating construction finish before the first mutation.
   if(!definitions.empty())unit.functions.reserve(unit.functions.size()+definitions.size());
   for(auto& edit:uses)edit.publish();
   for(auto* value:provenance){value->operator_parse.reset();value->operator_grouped=false;}
   if(!lazy.empty()){
    std::erase_if(unit.functions,[&](const Function& function){return unused(function.name);});
   }
   for(auto& edit:definitions)edit.publish(unit.functions);
   return report;
 }
};
}

meta::SourceDesugarReport meta::desugar_source_operators(Unit& unit){
 if(!unit.extensions.empty())
  throw Error(unit.extensions.front().where,"ON1X-CAT-PPX: unconsumed declaration carrier","ON1X-CAT-PPX");
 if(!unit.macros.empty())throw Error(unit.macros.front().where,"unexpanded macro definitions");
 if(!unit.ppx.empty())
  throw Error(unit.ppx.front().where,"source PPX registrations require --allow-source-ppx in the compiler driver");
 for(const auto& import:unit.module_imports){
  for(const auto& item:import.names)if(item.is_operator)
   throw Error(import.where,"selective operator import requires module loading: '"+item.name+"'",
               "ON1X-OPERATOR-UNRESOLVED");
 }
 auto report=SourceBridge(unit.operator_lazy_functions).run(unit);
 unit.operators.clear();
 unit.fixity_directives.clear();
 unit.operator_scopes.clear();
 unit.operator_owners.clear();
 unit.operator_entry.clear();
 unit.operator_lazy_functions.clear();
 return report;
}
}

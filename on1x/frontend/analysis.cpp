#include <on1x/analysis.hpp>
#include <on1x/numeric.hpp>
#include <algorithm>
#include <limits>
#include <map>

#ifdef ON1X_WITH_WETSTONE
#include <wetstone/abstract-interp/IntervalAnalysis.hpp>
#include <wetstone/pointer-analysis/Ownership.hpp>
#include <wetstone/repair-suggest/PatternBasedRepair.hpp>
#include <wetstone/verification/ArithmeticSafety.hpp>
#include <wetstone/verification/AssertionChecker.hpp>
#endif

namespace on1x::analysis {
bool wetstone_available() noexcept{
#ifdef ON1X_WITH_WETSTONE
 return true;
#else
 return false;
#endif
}

#ifdef ON1X_WITH_WETSTONE
namespace {
using Operand=wetstone::ir::Operand;
using Opcode=wetstone::ir::Opcode;
using ValueId=wetstone::ir::ValueId;

struct Translation {
 wetstone::ir::Function function;
 std::map<unsigned,Location> locations;
 bool complete=true;
};

std::optional<std::int64_t> checked_add(std::int64_t left,std::int64_t right){
 constexpr auto low=std::numeric_limits<std::int64_t>::min();
 constexpr auto high=std::numeric_limits<std::int64_t>::max();
 if((right>0&&left>high-right)||(right<0&&left<low-right))return {};
 return left+right;
}
std::optional<std::int64_t> checked_sub(std::int64_t left,std::int64_t right){
 constexpr auto low=std::numeric_limits<std::int64_t>::min();
 constexpr auto high=std::numeric_limits<std::int64_t>::max();
 if((right<0&&left>high+right)||(right>0&&left<low+right))return {};
 return left-right;
}
std::optional<std::int64_t> checked_mul(std::int64_t left,std::int64_t right){
 constexpr auto low=std::numeric_limits<std::int64_t>::min();
 constexpr auto high=std::numeric_limits<std::int64_t>::max();
 if(!left||!right)return 0;
 if(left==-1)return right==low?std::optional<std::int64_t>{}:std::optional<std::int64_t>{-right};
 if(right==-1)return left==low?std::optional<std::int64_t>{}:std::optional<std::int64_t>{-left};
 if((left>0&&right>0&&left>high/right)||
    (left>0&&right<0&&right<low/left)||
    (left<0&&right>0&&left<low/right)||
    (left<0&&right<0&&left<high/right))return {};
 return left*right;
}

class LinearHIR {
 const Function& source_;
 std::size_t& work_;
 bool valid_=true,terminated_=false,complete_=true;
 ValueId next_=0;
 unsigned source_id_=0;
 wetstone::ir::Block block_{0,{}};
 std::map<unsigned,Location> locations_;
 std::vector<std::map<std::string,Operand>> scopes_;

 bool spend(std::size_t amount=1){
  if(!valid_||amount>work_){valid_=false;return false;}
  work_-=amount;return true;
 }
 std::optional<ValueId> result(){
  if(next_==wetstone::ir::invalid_value){valid_=false;return {};}
  return next_++;
 }
 void instruction(Opcode opcode,std::optional<ValueId> value,std::vector<Operand> arguments,const Location& where,std::string symbol={}){
  if(!spend())return;
  if(source_id_==std::numeric_limits<unsigned>::max()){valid_=false;return;}
  auto line=++source_id_;locations_.emplace(line,where);
  block_.instructions.push_back({opcode,value,std::move(arguments),{}, {},std::move(symbol),line});
 }
 std::optional<Operand> lookup(const std::string& name)const{
  for(auto scope=scopes_.rbegin();scope!=scopes_.rend();++scope){
   auto found=scope->find(name);if(found!=scope->end())return found->second;
  }
  return {};
 }
 bool assign(const std::string& name,Operand value){
  for(auto scope=scopes_.rbegin();scope!=scopes_.rend();++scope){
   auto found=scope->find(name);if(found!=scope->end()){found->second=value;return true;}
  }
  return false;
 }
  std::optional<Operand> expression(const Expr& expression){
   if(!spend())return {};
   switch(expression.kind){
   case Expr::Kind::Integer:
    if(expression.type!=Type::I64){valid_=false;return {};}
    return Operand::constant(integer_literal_bits(expression.text,expression.where));
   case Expr::Kind::Boolean:return Operand::constant(expression.text=="true"?1:0);
   case Expr::Kind::Variable:{
    auto value=lookup(expression.text);if(!value)valid_=false;return value;
   }
    case Expr::Kind::Unary:
     if(expression.text=="-"){
      if(expression.children.size()!=1){valid_=false;return {};}
      auto input=this->expression(*expression.children[0]);
      if(!input)return {};
      if(input->is_constant&&input->literal!=std::numeric_limits<std::int64_t>::min())
       return Operand::constant(-input->literal);
      complete_=false;return Operand::constant(0);
     }
     if(expression.text=="!"){
      if(expression.children.size()!=1){valid_=false;return {};}
      auto input=this->expression(*expression.children[0]);
      if(!complete_)return Operand::constant(0);
      auto id=result();
     if(!input||!id){valid_=false;return {};}
     instruction(Opcode::Eq,*id,{*input,Operand::constant(0)},expression.where);return Operand::ref(*id);
    }
    valid_=false;return {};
   case Expr::Kind::Binary:{
    // The analysis IR does not encode On1x short-circuit evaluation.
     if(expression.children.size()!=2||expression.text=="&&"||expression.text=="||"){valid_=false;return {};}
     auto left=this->expression(*expression.children[0]);
     if(!complete_)return Operand::constant(0);
     auto right=this->expression(*expression.children[1]);
     if(!complete_)return Operand::constant(0);
     if(!left||!right){valid_=false;return {};}
    static const std::map<std::string,Opcode> operations={
     {"+",Opcode::Add},{"-",Opcode::Sub},{"*",Opcode::Mul},{"/",Opcode::Div},{"%",Opcode::Mod},
     {"==",Opcode::Eq},{"!=",Opcode::Ne},{"<",Opcode::Lt},{"<=",Opcode::Le},{">",Opcode::Gt},{">=",Opcode::Ge}
    };
    auto operation=operations.find(expression.text);if(operation==operations.end()){valid_=false;return {};}
     // Wetstone's scalar interval transfer is signed mathematical arithmetic.
     // Fold only proven-safe checked arithmetic. Dynamic checked add/sub/mul
     // end the analyzed prefix rather than being approximated as unbounded
     // mathematical integers.
     if(expression.children[0]->type!=Type::I64&&expression.children[0]->type!=Type::Bool){valid_=false;return {};}
     if(expression.text=="+"||expression.text=="-"||expression.text=="*"){
      if(!left->is_constant||!right->is_constant){complete_=false;return Operand::constant(0);}
      auto folded=expression.text=="+"?checked_add(left->literal,right->literal):
       expression.text=="-"?checked_sub(left->literal,right->literal):checked_mul(left->literal,right->literal);
      if(!folded){complete_=false;return Operand::constant(0);}
      return Operand::constant(*folded);
     }
     if(expression.text=="/"||expression.text=="%"){
      const bool safe_constant=left->is_constant&&right->is_constant&&right->literal!=0&&
       !(left->literal==std::numeric_limits<std::int64_t>::min()&&right->literal==-1);
      if(safe_constant)return Operand::constant(expression.text=="/"?left->literal/right->literal:left->literal%right->literal);
      if(right->is_constant&&right->literal!=0&&right->literal!=-1){
       auto id=result();if(!id)return {};
       instruction(operation->second,*id,{*left,*right},expression.where);return Operand::ref(*id);
      }
      if(right->is_constant&&right->literal==-1&&left->is_constant){
       complete_=false;return Operand::constant(0);
      }
      auto id=result();if(!id)return {};
      instruction(operation->second,*id,{*left,*right},expression.where);
      complete_=false;return Operand::ref(*id);
     }
     auto id=result();if(!id)return {};
    instruction(operation->second,*id,{*left,*right},expression.where);return Operand::ref(*id);
   }
   case Expr::Kind::Call:{
    if(!expression.callee||expression.callee->kind!=Expr::Kind::Variable){valid_=false;return {};}
    if(expression.callee->reference==Expr::Reference::BorrowFunction){complete_=false;return {};}
    std::vector<Operand> arguments;
     for(const auto& child:expression.children){
      auto value=this->expression(*child);if(!value){valid_=false;return {};}arguments.push_back(*value);
      if(!complete_)return expression.type==Type::Void?std::nullopt:std::optional<Operand>{Operand::constant(0)};
     }
    if(expression.callee->text=="assert"&&expression.callee->reference==Expr::Reference::Intrinsic){
     if(arguments.size()!=1){valid_=false;return {};}
     instruction(Opcode::Assert,{},std::move(arguments),expression.where,"assert");return std::nullopt;
    }
    if(expression.type==Type::Void){
     instruction(Opcode::Call,{},std::move(arguments),expression.where,expression.callee->text);return std::nullopt;
    }
    if(expression.type!=Type::I64&&expression.type!=Type::Bool){valid_=false;return {};}
    auto id=result();if(!id)return {};
    instruction(Opcode::Call,*id,std::move(arguments),expression.where,expression.callee->text);return Operand::ref(*id);
   }
   default:valid_=false;return {};
  }
 }
 void statements(const std::vector<Stmt>& body,bool scope){
  if(scope)scopes_.emplace_back();
   for(const auto& statement:body){
    if(!valid_||terminated_||!complete_||!spend())break;
   switch(statement.kind){
    case Stmt::Kind::Let:{
     if(statement.pattern||statement.prototype||!statement.value){valid_=false;break;}
     auto value=expression(*statement.value);if(!value){valid_=false;break;}
     if(scopes_.back().contains(statement.name)){valid_=false;break;}
     scopes_.back().emplace(statement.name,*value);break;
    }
    case Stmt::Kind::Assign:{
     if(!statement.value){valid_=false;break;}
     auto value=expression(*statement.value);if(!value||!assign(statement.name,*value))valid_=false;
     break;
    }
    case Stmt::Kind::Expr:
     if(!statement.value){valid_=false;break;}
     (void)expression(*statement.value);break;
    case Stmt::Kind::Return:{
     std::vector<Operand> values;
     if(statement.value){
      auto value=expression(*statement.value);if(!value){valid_=false;break;}values.push_back(*value);
     }
     instruction(Opcode::Return,{},std::move(values),statement.where);terminated_=true;break;
    }
    case Stmt::Kind::Block:
     statements(statement.body,true);break;
    default:
     // Branch joins, loops, lambdas, aggregate/resource operations, cleanup,
     // and concurrency need dedicated semantics before entering Wetstone.
     valid_=false;break;
   }
  }
  if(scope)scopes_.pop_back();
 }
public:
 LinearHIR(const Function& source,std::size_t& work):source_(source),work_(work){}
 std::optional<Translation> run(){
   if(source_.external||source_.prototype||takes_borrows(source_)||!spend())
    return {};
  wetstone::ir::Function function;function.name=source_.name;function.entry=0;
  scopes_.emplace_back();
  for(const auto& parameter:source_.params){
   if(parameter.pattern||(parameter.type!=Type::I64&&parameter.type!=Type::Bool)){valid_=false;break;}
   auto id=result();if(!id)break;function.parameters.push_back(*id);
   if(!scopes_.back().emplace(parameter.name,Operand::ref(*id)).second){valid_=false;break;}
  }
  statements(source_.body,false);
  if(valid_&&!terminated_)instruction(Opcode::Return,{}, {},source_.where);
  if(!valid_)return {};
  function.blocks.push_back(std::move(block_));
  try{function.validate();}catch(const std::exception&){return {};}
   return Translation{std::move(function),std::move(locations_),complete_};
 }
};

const diagnostic::ScrutinyPolicy& policy(const Function& function,const Unit& unit,const Context& context){
 if(context.options.command_scrutiny)return context.catalog.scrutiny(*context.options.command_scrutiny,{"<command-line>"});
 for(const auto& directive:unit.scrutiny)if(directive.where.file==function.where.file)
  return context.catalog.scrutiny(directive.level,directive.where);
 if(context.options.project_scrutiny)return context.catalog.scrutiny(*context.options.project_scrutiny,{"<On1xfile>"});
 return context.catalog.scrutiny(context.catalog.default_scrutiny(),{"<diagnostics>"});
}

struct SuggestionKey {
 std::string rule,function;
 std::uint32_t line=0,block=0;
 friend auto operator<=>(const SuggestionKey&,const SuggestionKey&)=default;
};
}
#endif

void run(const Unit& unit,Context& context){
 if(!context.options.work||context.options.work>10000000)
  throw context.catalog.failure(diagnostic::Role::ScrutinyFailure,{"<command-line>"});
 if(context.options.command_scrutiny)(void)context.catalog.scrutiny(*context.options.command_scrutiny,{"<command-line>"});
 if(context.options.project_scrutiny)(void)context.catalog.scrutiny(*context.options.project_scrutiny,{"<On1xfile>"});
 std::set<std::string> directed;
 for(const auto& directive:unit.scrutiny){
  if(!directed.insert(directive.where.file).second)
   throw context.catalog.failure(diagnostic::Role::ScrutinyFailure,directive.where);
  (void)context.catalog.scrutiny(directive.level,directive.where);
 }
 context.statistics={};context.statistics.wetstone_available=wetstone_available();
#ifndef ON1X_WITH_WETSTONE
 const auto& fallback=context.options.command_scrutiny?*context.options.command_scrutiny:
  context.options.project_scrutiny?*context.options.project_scrutiny:context.catalog.default_scrutiny();
 if(context.catalog.scrutiny(fallback).enabled)context.statistics.complete=false;
 (void)unit;return;
#else
 std::size_t remaining=context.options.work;
 for(const auto& function:unit.functions){
  if(function.external||function.prototype)continue;
   ++context.statistics.functions;
   const auto& selected=policy(function,unit,context);
   if(!selected.enabled)continue;
    auto translated=LinearHIR(function,remaining).run();
    if(!translated){++context.statistics.skipped_functions;context.statistics.complete=false;continue;}
    ++context.statistics.analyzed_functions;
    if(!translated->complete){++context.statistics.skipped_functions;context.statistics.complete=false;}
   const auto adapters=context.catalog.adapters(selected);
   const bool division=adapters.contains(diagnostic::AnalysisAdapter::DivisionSafety);
   const bool assertions=adapters.contains(diagnostic::AnalysisAdapter::AssertionSafety);
   const bool advisory=context.options.repairs&&context.catalog.advisory_repairs(selected);
   std::size_t calls=adapters.size()+(division||assertions?1:0)+(advisory?1:0);
   if(!calls)continue;
   if(remaining<calls){context.statistics.complete=false;break;}
   constexpr std::size_t budget=1; // LinearHIR always emits exactly one block.
   if(remaining<calls){context.statistics.complete=false;break;}
  // Reserve each advertised bounded analysis independently. Wetstone does not
    // report exact work from all public warning APIs, so this is a hard upper
    // bound rather than a fabricated consumed-work measurement.
    remaining-=calls;
   if(division||assertions){
    auto convergence=wetstone::ai::analyze_intervals(translated->function,budget);
    if(!convergence.converged)context.statistics.complete=false;
   }
   wetstone::Diagnostics findings;
   std::map<SuggestionKey,std::vector<std::string>> repairs;
   try{
    for(auto adapter:adapters)switch(adapter){
     case diagnostic::AnalysisAdapter::DivisionSafety:{
      auto results=wetstone::verification::division_warnings(translated->function,budget);
      findings.insert(findings.end(),results.begin(),results.end());break;
     }
     case diagnostic::AnalysisAdapter::AssertionSafety:{
      auto report=wetstone::verification::check_assertions(translated->function,budget);
      findings.insert(findings.end(),report.definite_failures.begin(),report.definite_failures.end());
      findings.insert(findings.end(),report.possible_failures.begin(),report.possible_failures.end());break;
     }
     case diagnostic::AnalysisAdapter::OwnershipSafety:{
      auto results=wetstone::pointer::ownership_warnings(translated->function);
      findings.insert(findings.end(),results.begin(),results.end());break;
     }
    }
    if(advisory){
     auto suggestions=wetstone::repair::pattern_based(translated->function,budget);
     for(const auto& suggestion:suggestions.suggestions()){
      auto key=SuggestionKey{suggestion.problem.rule,suggestion.problem.function,suggestion.problem.line,suggestion.problem.block};
      repairs[key].push_back(suggestion.explanation);
      repairs[key].push_back(suggestion.proposed_change);
     }
    }
  }catch(const std::exception&){
   context.statistics.complete=false;continue;
   }
   for(const auto& finding:findings){
    auto source_level=finding.severity==wetstone::Severity::Error?
     diagnostic::UpstreamLevel::Error:diagnostic::UpstreamLevel::Warning;
    auto entry=context.catalog.wetstone(finding.rule,source_level);
    if(!entry)continue;
    if(!context.catalog.selected(selected,*entry))continue;
    auto location=translated->locations.find(finding.line);
    if(location==translated->locations.end()){context.statistics.complete=false;continue;}
    auto record=context.catalog.record(*entry,location->second,{.message=finding.message});
    record.level=context.catalog.level(selected,entry->level);
     auto key=SuggestionKey{finding.rule,finding.function,finding.line,finding.block};
     if(entry->repair.kind==diagnostic::RepairKind::Advisory){
      if(auto suggestions=repairs.find(key);suggestions!=repairs.end()&&suggestions->second.size()==2){
       record.notes.push_back(diagnostic::expand(entry->repair.rationale,{.message=suggestions->second[0]}));
       record.notes.push_back(diagnostic::expand(entry->repair.proposal,{.message=suggestions->second[1]}));
      }
     }
    auto dropped=context.diagnostics.dropped();
    (void)context.diagnostics.emit(std::move(record));
    if(context.diagnostics.dropped()!=dropped)context.statistics.complete=false;
  }
 }
 context.statistics.work=context.options.work-remaining;
#endif
}
}

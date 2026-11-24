#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/literals.hpp>
#include <on1x/nominal.hpp>
#include <on1x/numeric.hpp>
#include <on1x/primitives.hpp>
#include "../../hir/MEMGraph/lexical_slots_detail.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <iterator>
#include <optional>
#include <set>

namespace on1x::canonical {
namespace {
using E=Expr::Kind;using S=Stmt::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-VALUES-VERIFY: "+text,"ON1X-VALUES-VERIFY");}
[[noreturn]] void ref_error(const Location& at,const std::string& text){throw Error(at,text,"ON1X-REF");}
struct State {
   std::set<const Stmt*> initialized;
   std::set<const Stmt*> maybe_initialized;
  std::set<std::pair<std::size_t,std::string>> tags;
  std::set<std::size_t> moved;
  std::set<std::pair<std::size_t,std::string>> moved_parts;
};
using Flow=std::optional<State>;
State meet(const State& a,const State& b){
  State out;std::set_intersection(a.initialized.begin(),a.initialized.end(),b.initialized.begin(),b.initialized.end(),std::inserter(out.initialized,out.initialized.end()));
  std::set_union(a.maybe_initialized.begin(),a.maybe_initialized.end(),b.maybe_initialized.begin(),b.maybe_initialized.end(),std::inserter(out.maybe_initialized,out.maybe_initialized.end()));
  std::set_intersection(a.tags.begin(),a.tags.end(),b.tags.begin(),b.tags.end(),std::inserter(out.tags,out.tags.end()));
  std::set_union(a.moved.begin(),a.moved.end(),b.moved.begin(),b.moved.end(),std::inserter(out.moved,out.moved.end()));
  std::set_union(a.moved_parts.begin(),a.moved_parts.end(),b.moved_parts.begin(),b.moved_parts.end(),std::inserter(out.moved_parts,out.moved_parts.end()));
  return out;
}
Flow merge(Flow a,const Flow& b){if(!a)return b;if(!b)return a;return meet(*a,*b);}
 class Verify {
   const Unit& unit;
   NominalCatalog catalog;
   memgraph::detail::LexicalSlotBuilder* slots;
  bool unsafe_context=false;
  bool generic_context=false;
   bool loan_tracking=false;
   struct Local {
    Type type;bool mut=false;const Stmt* storage=nullptr;std::size_t id=0;
    BorrowMode borrow=BorrowMode::Value;
    std::size_t origin=0,root=0;
    bool addressable=true,prototype=false;
    // Filled only when exporting a plan from executable HIR. The lexical
    // verifier's own identity/lifetime checks never depend on these pointers.
    const Stmt* scalar_root_statement=nullptr;
    const Param* scalar_root_parameter=nullptr;
    const Stmt* planned_binding=nullptr;
   };
   struct Loan {
    BorrowMode mode=BorrowMode::Value;
    std::size_t origin=0,root=0;
   };
 struct Target {std::size_t id;std::vector<State> exits,continues;};
  std::vector<std::map<std::string,Local>> scopes;
  std::map<std::string,const Function*> functions;
  std::map<std::string,const Function*> borrowers;
  std::set<std::pair<Type,bool>> checked_types;
   std::vector<Loan> call_loans;
  std::vector<Target> regions,loops;
  Type returns=Type::Void;
   std::size_t serial=0,budget=262144,state_work=4*1024*1024,
               type_work=262144,borrow_work=4*1024*1024;
  void tick(const Location& at,std::size_t depth){if(depth>520||!budget--)throw Error(at,"canonical value verification budget exhausted","ON1X-VALUES-LIMIT");}
   void charge(const State& state,const Location& at){auto size=state.initialized.size()+state.maybe_initialized.size()+state.tags.size()+state.moved.size()+state.moved_parts.size();if(size>state_work)throw Error(at,"canonical value flow-state budget exhausted","ON1X-VALUES-LIMIT");state_work-=size;}
   void source(const Location& at){
   if(at.file.empty()||at.file.size()>4096||at.file.find('\0')!=std::string::npos||
      !at.line||!at.column||at.offset>max_source_bytes||
      at.line>max_source_bytes+1||at.column>max_source_bytes+1)
    fail(at,"invalid canonical value source point");
  }
   void checked_type(const Type& type,const Location& at,bool generics=false){
   if(checked_types.contains({type,generics}))return;
   if(!type_work)
    throw Error(at,"canonical type-form work budget exhausted","ON1X-VALUES-LIMIT");
   TypeFormOptions options;options.allow_generics=generics;
   options.max_nodes=std::min(options.max_nodes,type_work);
   auto form=check_type_form(type,options);
   type_work-=std::min(type_work,std::max<std::size_t>(1,form.nodes));
   if(form.status==TypeFormStatus::Limit)
    throw Error(at,"canonical type-form validation limit: "+form.message,"ON1X-VALUES-LIMIT");
   if(!form)fail(at,"malformed checked type: "+form.message);
    checked_types.emplace(type,generics);
   }
   void borrow_charge(const Location& at,std::size_t amount=1){
    if(amount>borrow_work)
     throw Error(at,"borrow analysis work budget exhausted","ON1X-BORROW-LIMIT");
    borrow_work-=amount;
   }
    static bool identifier(const std::string& name){
     if(name.empty()||
        !(std::isalpha(static_cast<unsigned char>(name.front()))||
          name.front()=='_'))return false;
     return std::all_of(name.begin(),name.end(),[](unsigned char c){
       return std::isalnum(c)||c=='_';
      });
     }
    static bool valid_borrow_mode(BorrowMode mode){
     return mode==BorrowMode::Value||mode==BorrowMode::Shared||
            mode==BorrowMode::Exclusive;
    }
    static bool move_place(E kind){
     return kind==E::Variable||kind==E::Payload||kind==E::Project||
            kind==E::Field||kind==E::Index;
    }
   static bool same_constraint(const ClicheConstraint& left,
                               const ClicheConstraint& right){
    return left.name==right.name&&left.arguments==right.arguments;
   }
   void metadata_type(const Type& type,const Location& at,
                      std::size_t bound_variables){
    if(!type_work)
     throw Error(at,"canonical type-form work budget exhausted",
                 "ON1X-VALUES-LIMIT");
    TypeFormOptions options;options.bound_variables=bound_variables;
    options.allow_named=true;options.allow_foralls=false;
    options.max_nodes=std::min(options.max_nodes,type_work);
    auto form=check_type_form(type,options);
    type_work-=std::min(type_work,std::max<std::size_t>(1,form.nodes));
    if(form.status==TypeFormStatus::Limit)
     throw Error(at,"canonical generic-metadata type limit: "+form.message,
                 "ON1X-VALUES-LIMIT");
    if(!form)fail(at,"malformed generic metadata type: "+form.message);
   }
   void constraint(const ClicheConstraint& item,std::size_t variables){
    tick(item.where,0);source(item.where);
    if(item.name.empty()||item.name.size()>4096||
       item.name.find('\0')!=std::string::npos)
     fail(item.where,"invalid checked cliche constraint name");
    auto declaration=std::find_if(
     unit.cliches.begin(),unit.cliches.end(),
     [&](const ClicheDeclaration& value){return value.name==item.name;});
    if(declaration==unit.cliches.end())
     fail(item.where,"unknown checked cliche constraint: "+item.name);
    if(item.arguments.size()!=declaration->type_parameters.size()+1)
     fail(item.where,"checked cliche constraint arity mismatch: "+item.name);
    for(const auto& argument:item.arguments)
     metadata_type(argument,item.where,variables);
   }
   void generic_metadata(const Function& function){
    if(function.type_parameters.size()>4096||
       function.constraints.size()>4096)
     throw Error(function.where,
                 "canonical generic-metadata budget exhausted",
                 "ON1X-VALUES-LIMIT");
    std::set<std::string> names;
    for(std::size_t i=0;i<function.type_parameters.size();++i){
     const auto& parameter=function.type_parameters[i];
     source(parameter.where);
     if(!identifier(parameter.name)||!names.insert(parameter.name).second)
      fail(parameter.where,
           "invalid or duplicate checked generic parameter: "+parameter.name);
     for(const auto& bound:parameter.bounds){
      constraint(bound,function.type_parameters.size());
      if(bound.arguments.empty()||
         bound.arguments.front()!=Type::bound(i))
       fail(bound.where,"generic parameter bound has a forged Self argument");
      if(std::none_of(
          function.constraints.begin(),function.constraints.end(),
          [&](const ClicheConstraint& item){
           return same_constraint(item,bound);
          }))
       fail(bound.where,
            "generic parameter bound is absent from function constraints");
     }
     }
    for(const auto& item:function.constraints)
     constraint(item,function.type_parameters.size());
   }
   struct RetainedControl {
    struct Loop {std::size_t id;bool value;};
    std::set<std::size_t> ids;
    std::vector<Loop> loops;
    std::vector<std::size_t> regions;
   };
   void retained_pattern(const Pattern& pattern,std::size_t depth){
    tick(pattern.where,depth);source(pattern.where);
    if(pattern.type!=Type::Invalid)
     checked_type(pattern.type,pattern.where,true);
    for(const auto& child:pattern.elements)
     retained_pattern(child,depth+1);
   }
   void retained_body(const std::vector<Stmt>& statements,
                      RetainedControl& control,std::size_t depth){
    for(const auto& statement:statements){
     tick(statement.where,depth);source(statement.where);
     if(!statement.instances.empty())
      fail(statement.where,
           "unfrozen specialization instances in retained generic source");
     if(statement.declared!=Type::Invalid)
      checked_type(statement.declared,statement.where,true);
     if(statement.pattern)
      retained_pattern(*statement.pattern,depth+1);
     auto value=[&]{
      if(statement.value)
       retained_expression(*statement.value,control,depth+1);
     };
     if(statement.kind==S::While){
      if(!statement.value||!statement.control_target||
         !control.ids.insert(statement.control_target).second)
       fail(statement.where,
            "missing or duplicate retained generic loop identity");
      value();
      control.loops.push_back({statement.control_target,false});
      retained_body(statement.body,control,depth+1);
      control.loops.pop_back();
      retained_body(statement.otherwise,control,depth+1);
      continue;
     }
     if(statement.kind==S::Region){
      if(!statement.control_target||
         !control.ids.insert(statement.control_target).second)
       fail(statement.where,
            "missing or duplicate retained generic region identity");
      value();
      control.regions.push_back(statement.control_target);
      retained_body(statement.body,control,depth+1);
      control.regions.pop_back();
      retained_body(statement.otherwise,control,depth+1);
      continue;
     }
     if(statement.kind==S::Leave){
      if(!statement.control_target||
         std::find(control.regions.rbegin(),control.regions.rend(),
                   statement.control_target)==control.regions.rend())
       fail(statement.where,
            "retained generic region exit target is not active");
     }else if(statement.kind==S::Break||statement.kind==S::Continue){
      auto target=std::find_if(
       control.loops.rbegin(),control.loops.rend(),
       [&](const RetainedControl::Loop& loop){
        return loop.id==statement.control_target;
       });
      if(!statement.control_target||target==control.loops.rend()||
         (statement.kind==S::Continue&&statement.value)||
         (statement.value&&!target->value))
       fail(statement.where,
            "retained generic loop exit target or value is invalid");
     }else if(statement.control_target)
      fail(statement.where,
           "control identity on retained generic non-control statement");
     value();
     retained_body(statement.body,control,depth+1);
     retained_body(statement.otherwise,control,depth+1);
    }
   }
   void retained_expression(const Expr& expression,RetainedControl& control,
                            std::size_t depth,bool callee_position=false){
    tick(expression.where,depth);source(expression.where);
    const bool intrinsic=callee_position&&expression.kind==E::Variable&&
                         expression.reference==Expr::Reference::Intrinsic;
    if(intrinsic){
     if(expression.type!=Type::Invalid)
      fail(expression.where,
           "retained intrinsic callee carries a forged value type");
    }else checked_type(expression.type,expression.where,true);
    checked_type(expression.result,expression.where,true);
     for(const auto& parameter:expression.parameters){
      source(parameter.where);
      if(parameter.borrow!=BorrowMode::Value)
       fail(parameter.where,
            "borrowed parameter in retained anonymous generic source");
      checked_type(parameter.type,parameter.where,true);
     if(parameter.pattern)
      retained_pattern(*parameter.pattern,depth+1);
    }
    for(const auto& capture:expression.captures)
     checked_type(capture.type,expression.where,true);
    for(const auto& pattern:expression.patterns)
     retained_pattern(pattern,depth+1);
    if(expression.syntax_tail||!expression.syntax_definitions.empty()||
       expression.syntax_repeat||!expression.type_parameters.empty()||
       !expression.constraints.empty()||
       !expression.cliche_arguments.empty()||
       !expression.member_type_arguments.empty()||
       !expression.cliche_member.empty())
     fail(expression.where,
          "source-only metadata in retained checked generic source");
    if(expression.syntax_depth==0||
       expression.syntax_depth>max_syntax_depth)
     fail(expression.where,
          "invalid retained generic expression syntax-depth metadata");
     if(expression.kind==E::ClicheMember||
        expression.kind==E::InherentMember)
      fail(expression.where,
           "unresolved member projection in retained generic source");
     if(expression.kind==E::Borrow||
        expression.borrow!=BorrowMode::Value)
      fail(expression.where,
           "borrow metadata in retained generic source");
    if(expression.move_out&&
       (!move_place(expression.kind)||!expression.type.is_move_only()||
        (expression.kind==E::Variable&&
         expression.reference!=Expr::Reference::Local)))
     fail(expression.where,
          "invalid unique transfer metadata in retained generic source");
    if(expression.move_out&&expression.kind==E::Index)
     fail(expression.where,
          "unique array-element move in retained generic source");
    if(expression.kind==E::MacroCall||
       expression.kind==E::SyntaxHole||
       expression.kind==E::SyntaxBlock||
       expression.kind==E::Extension||
       expression.kind==E::SyntaxList||
       expression.kind==E::SyntaxVector||
       expression.kind==E::SyntaxKeyword||
       expression.kind==E::SyntaxLet||
       expression.kind==E::Splice||expression.kind==E::Lift)
     fail(expression.where,
          "unexpanded syntax in retained checked generic source");
    if(expression.reference!=Expr::Reference::Unresolved&&
       expression.kind!=E::Variable)
     fail(expression.where,
          "name-resolution metadata on retained non-variable expression");
    if(expression.callee&&expression.kind!=E::Call)
     fail(expression.where,
          "callee on retained generic non-call expression");
    if((!expression.parameters.empty()||!expression.captures.empty())&&
       expression.kind!=E::Lambda)
     fail(expression.where,
          "closure metadata on retained generic non-lambda expression");
    const bool body_allowed=expression.kind==E::Lambda||
      expression.kind==E::Loop||expression.kind==E::Case||
      expression.kind==E::Region||expression.kind==E::UnsafeBlock;
    if(!expression.body.empty()&&!body_allowed)
     fail(expression.where,
          "body on unrelated retained generic expression");
    if(expression.kind==E::Loop){
     if(!expression.control_target||
        !control.ids.insert(expression.control_target).second||
        expression.callee||!expression.children.empty()||
        !expression.parameters.empty()||!expression.patterns.empty()||
        !expression.labels.empty()||!expression.captures.empty()||
        !expression.proofs.empty()||expression.result!=Type::Void||
        expression.reference!=Expr::Reference::Unresolved||
        expression.projection||expression.statement_match||
        expression.noreturn)
      fail(expression.where,"malformed retained generic value loop");
    }else if(expression.control_target)
     fail(expression.where,
          "control identity on retained generic non-loop expression");
    if(expression.callee)
     retained_expression(*expression.callee,control,depth+1,true);
    for(const auto& child:expression.children){
     if(!child)fail(expression.where,
                    "missing retained generic expression child");
     retained_expression(*child,control,depth+1);
    }
    for(const auto& proof:expression.proofs){
     if(!proof)fail(expression.where,
                    "missing retained generic universal proof");
     retained_expression(*proof,control,depth+1);
    }
    if(expression.kind==E::Lambda){
     auto loops=std::move(control.loops);
     auto regions=std::move(control.regions);
     control.loops.clear();control.regions.clear();
     retained_body(expression.body,control,depth+1);
     control.loops=std::move(loops);
     control.regions=std::move(regions);
    }else if(expression.kind==E::Loop){
     control.loops.push_back({expression.control_target,true});
     retained_body(expression.body,control,depth+1);
     control.loops.pop_back();
    }else retained_body(expression.body,control,depth+1);
   }
   static Type signature(const Function& function){
   std::vector<Type> parameters;
   for(const auto& parameter:function.params)parameters.push_back(parameter.type);
   return Type::function(std::move(parameters),function.result);
  }
  static Expr::Reference function_reference(const Function& function){
   if(function.kernel_link_only)return Expr::Reference::ScalarForeign;
   if(!function.kernel_symbol.empty())return Expr::Reference::UnsafeForeign;
   if(function.external)return Expr::Reference::Extern;
    if(takes_borrows(function))return Expr::Reference::BorrowFunction;
   return Expr::Reference::Function;
   }
   void expression_header(const Expr& e,std::size_t depth,bool callee_position){
    tick(e.where,depth);source(e.where);
    if(e.prototype&&!generic_context&&borrowers.empty())
     fail(e.where,"non-executable prototype marker in executable checked HIR");
    const bool generics=generic_context||e.prototype;
   const bool intrinsic=callee_position&&e.kind==E::Variable&&
                        e.reference==Expr::Reference::Intrinsic;
   if(intrinsic){
    if(e.type!=Type::Invalid)fail(e.where,"intrinsic callee carries a forged value type");
   }else checked_type(e.type,e.where,generics);
   checked_type(e.result,e.where,generics);
   for(const auto& parameter:e.parameters){
    source(parameter.where);checked_type(parameter.type,parameter.where,generics);
   }
   for(const auto& capture:e.captures)checked_type(capture.type,e.where,generics);
   for(const auto& child:e.children)if(!child)fail(e.where,"missing expression child");
   for(const auto& proof:e.proofs)if(!proof)fail(e.where,"missing universal proof expression");
   if(e.syntax_tail||!e.syntax_definitions.empty()||e.syntax_repeat||
      !e.type_parameters.empty()||!e.constraints.empty()||
      !e.cliche_arguments.empty()||!e.member_type_arguments.empty()||
       !e.cliche_member.empty()){
     fail(e.where,"source-only metadata reached checked executable HIR");
    }
    if(e.syntax_depth==0||e.syntax_depth>max_syntax_depth){
      fail(e.where,"invalid checked expression syntax-depth metadata");
     }
     if(!valid_borrow_mode(e.borrow)||
        (e.kind==E::Borrow)!=(e.borrow!=BorrowMode::Value))
      ref_error(e.where,"invalid checked borrow expression mode");
      if(!generic_context){
      if(e.context){
       fail(e.where,"source syntax context reached checked executable HIR");
      }
      if(e.result!=Type::Void&&e.kind!=E::Lambda&&e.kind!=E::Record&&
         e.kind!=E::Variant&&e.kind!=E::Csp)
       fail(e.where,"auxiliary result type on an unrelated canonical expression");
     const bool text_allowed=e.kind==E::Integer||e.kind==E::Float||
       e.kind==E::Boolean||e.kind==E::String||e.kind==E::Variable||
       e.kind==E::ClicheMember||e.kind==E::InherentMember||
       e.kind==E::Unary||e.kind==E::Binary||e.kind==E::Variant||
       e.kind==E::Field||e.kind==E::TagTest||e.kind==E::Payload||
        e.kind==E::Csp||e.kind==E::Character||e.kind==E::ByteBuffer||
        e.kind==E::RegexLiteral||
        (e.kind==E::Index&&e.text==":");
     if(!text_allowed&&!e.text.empty())
       fail(e.where,"text metadata on an unrelated canonical expression");
     }
     if(e.kind==E::Csp){
      if(e.text=="channel"){
       if(e.result==Type::Invalid||e.type.kind()!=Type::Channel||
          e.type.csp_payload()!=e.result)
        fail(e.where,"invalid canonical concurrency payload annotation");
      }else if(e.result!=Type::Void)
       fail(e.where,"invalid canonical concurrency payload annotation");
     }
   if(!e.patterns.empty()||e.statement_match||e.sum_match){
     fail(e.where,"surface pattern metadata reached canonical HIR");
    }
    if(e.move_out&&
       (!move_place(e.kind)||!e.type.is_move_only()||
        (e.kind==E::Variable&&e.reference!=Expr::Reference::Local)))
     fail(e.where,"invalid unique transfer metadata on a canonical expression");
    if(e.move_out&&e.kind==E::Index)
     fail(e.where,"unique array-element moves are not admitted");
   if(e.control_target)fail(e.where,"control identity on a canonical non-loop expression");
   if(e.reference!=Expr::Reference::Unresolved&&e.kind!=E::Variable)
    fail(e.where,"name-resolution metadata on a non-variable expression");
   if(e.callee&&e.kind!=E::Call)fail(e.where,"callee on a non-call expression");
   if((!e.parameters.empty()||!e.captures.empty())&&e.kind!=E::Lambda)
    fail(e.where,"closure metadata on a non-lambda expression");
   if(!e.body.empty()&&e.kind!=E::Lambda&&e.kind!=E::Region)
    fail(e.where,"body on an ordinary canonical expression");
   if(!e.labels.empty()&&e.kind!=E::Record)
    fail(e.where,"labels on a non-record canonical expression");
   if(!e.proofs.empty()&&e.kind!=E::PolyPack)
    fail(e.where,"universal proof on a non-pack expression");
   if(e.unsafe_region&&e.kind!=E::Region)
    fail(e.where,"unsafe marker on a non-region expression");
   if(e.noreturn&&e.kind!=E::Region)
    fail(e.where,"non-returning metadata on an ordinary expression");
   if(e.projection&&e.kind!=E::PolyProjection&&e.kind!=E::TagTest&&
      e.kind!=E::Payload&&e.kind!=E::Project&&e.kind!=E::Variant&&
      e.kind!=E::Field)
    fail(e.where,"projection metadata on an unrelated expression");
  }
  void string_literal(const Expr& e){
   if(e.text.size()<2||e.text.size()>max_source_bytes+2||
      e.text.front()!='"'||e.text.back()!='"')
    fail(e.where,"malformed canonical string literal");
   for(std::size_t i=1;i+1<e.text.size();++i){
    auto c=static_cast<unsigned char>(e.text[i]);
    if(c=='"'||c=='\n'||c=='\r'||c=='\0')
     fail(e.where,"malformed canonical string literal");
    if(c!='\\')continue;
    if(++i+1>=e.text.size())fail(e.where,"truncated canonical string escape");
    c=static_cast<unsigned char>(e.text[i]);
    if(c=='0')fail(e.where,"NUL escape is not supported by the C-string ABI");
    if(c!='\\'&&c!='"'&&c!='$'&&c!='n'&&c!='r'&&c!='t')
     fail(e.where,"unknown canonical string escape");
   }
  }
  const Local* find(const std::string& name)const{for(auto it=scopes.rbegin();it!=scopes.rend();++it)if(auto found=it->find(name);found!=it->end())return &found->second;return nullptr;}
   void bind(const std::string& name,Type type,bool mut,const Stmt* storage=nullptr,
             BorrowMode borrow=BorrowMode::Value,bool addressable=true,
             bool prototype=false,std::size_t origin=0,std::size_t root=0){
     if(name.empty()||scopes.back().contains(name))
      fail(storage?storage->where:Location{"<canonical-values>"},
           "empty/duplicate canonical local");
     const auto id=++serial;
     if(borrow==BorrowMode::Value){origin=0;root=id;}
     else if(!origin||!root)
      ref_error(storage?storage->where:Location{"<canonical-values>"},
                "borrowed local has no origin");
     scopes.back()[name]={type,mut,storage,id,borrow,origin,root,
                          addressable,prototype};
   }
    void plan_statement_root(const Stmt& statement,const Type& type){
     if(slots&&!generic_context&&memgraph::detail::lexical_copy_scalar(type))
      scopes.back().at(statement.name).scalar_root_statement=&statement;
    }
   void pattern(const Pattern& p,const Type& t,std::size_t depth=0){
     tick(p.where,depth);source(p.where);checked_type(t,p.where,generic_context);
     if(p.context)fail(p.where,"source pattern context reached canonical HIR");
    if(p.type!=Type::Invalid){
     checked_type(p.type,p.where,generic_context);
     if(p.type!=t)fail(p.where,"canonical binding pattern metadata/type mismatch");
    }
    if((p.kind!=Pattern::Kind::Name&&p.kind!=Pattern::Kind::Tuple)||
       !p.labels.empty()||p.open)
     fail(p.where,"refutable or labeled pattern in a canonical binding");
    if(p.is_tuple()){
     if(!t.is_tuple()||p.elements.size()<2||p.elements.size()!=t.elements().size())
      fail(p.where,"canonical tuple binding pattern/type mismatch");
     for(std::size_t i=0;i<p.elements.size();++i)
      pattern(p.elements[i],t.elements()[i],depth+1);
    }else{
     if(!p.elements.empty()||p.name.empty())fail(p.where,"invalid canonical name binding");
     bind(p.name,t,false);
    }
   }
   void params(const std::vector<Param>& ps,std::size_t depth=0){
    for(const auto& p:ps){
      source(p.where);checked_type(p.type,p.where,generic_context);
      if(!valid_borrow_mode(p.borrow))
       ref_error(p.where,"invalid checked borrowed parameter mode");
     if(p.context)fail(p.where,"source parameter context reached canonical HIR");
     if(p.pattern){
      if(p.borrow!=BorrowMode::Value)
       ref_error(p.where,"borrowed parameters cannot use a pattern");
      pattern(*p.pattern,p.type,depth+1);
     }else{
      bind(p.name,p.type,false);
      if(p.borrow!=BorrowMode::Value){
       auto& local=scopes.back().at(p.name);
       local.borrow=p.borrow;local.origin=local.id;local.root=local.id;
       loan_tracking=true;
      }else if(slots&&!generic_context&&memgraph::detail::lexical_copy_scalar(p.type))
       scopes.back().at(p.name).scalar_root_parameter=&p;
     }
    }
   }
   const Local* by_id(std::size_t id,const Location& at){
    for(auto scope=scopes.rbegin();scope!=scopes.rend();++scope)
     for(const auto& [name,local]:*scope){
      borrow_charge(at);
      (void)name;if(local.id==id)return &local;
     }
    return nullptr;
   }
   bool ancestor(std::size_t possible,std::size_t node,const Location& at){
    // Reborrows declared sequentially in one block can form a deeper chain
    // than the lexical-scope count. Binding identity still gives a finite
    // acyclic bound, while borrow_charge caps malformed or adversarial work.
    for(std::size_t depth=0;node&&depth<=serial+1;++depth){
     borrow_charge(at);
     if(node==possible)return true;
     auto local=by_id(node,at);
     if(!local||local->borrow==BorrowMode::Value||local->origin==node)
      return false;
     node=local->origin;
    }
    if(node)
     throw Error(at,"borrow ancestry depth budget exhausted",
                 "ON1X-BORROW-LIMIT");
    return false;
   }
   template<class F> void each_loan(const Location& at,F&& operation){
    if(!loan_tracking&&call_loans.empty())return;
    for(const auto& scope:scopes)for(const auto& [name,local]:scope){
     (void)name;borrow_charge(at);
     if(local.borrow!=BorrowMode::Value)
      operation(Loan{local.borrow,local.origin,local.root},local.id);
    }
    for(const auto& loan:call_loans){
     borrow_charge(at);operation(loan,std::size_t{0});
    }
   }
   enum class Access { Read, Write, Consume };
   void access(const Local* binding,const Location& at,const std::string& name,
               Access operation=Access::Read){
    if(!binding)return;
    each_loan(at,[&](const Loan& loan,std::size_t loan_id){
     if(loan_id==binding->id||loan.root!=binding->root)return;
     bool conflict=false;
     if(binding->borrow==BorrowMode::Value)
      conflict=operation!=Access::Read||
               loan.mode==BorrowMode::Exclusive;
     else if(ancestor(binding->id,loan.origin,at))
      conflict=binding->borrow==BorrowMode::Exclusive||
               operation!=Access::Read||
               loan.mode==BorrowMode::Exclusive;
     if(conflict)
      ref_error(at,"cannot "+std::string(operation==Access::Read?"access ":
        operation==Access::Write?"mutate ":"consume ")+"'"+name+
        "' during its "+(loan.mode==BorrowMode::Exclusive?
        std::string("exclusive"):std::string("shared"))+" borrow");
    });
   }
  static bool has_moved_part(const State& state,std::size_t id){
   return std::any_of(state.moved_parts.begin(),state.moved_parts.end(),
                      [&](const auto& part){return part.first==id;});
  }
  static void clear_moves(State& state,std::size_t id){
   state.moved.erase(id);
   for(auto it=state.moved_parts.begin();it!=state.moved_parts.end();)
    if(it->first==id)it=state.moved_parts.erase(it);else ++it;
  }
  static std::string part_name(const Expr& e){
   const char prefix=e.kind==E::Payload?'s':e.kind==E::Project?'t':
                     e.kind==E::Field?'r':'a';
   return std::string(1,prefix)+std::to_string(e.projection);
  }
   void move_part(const Expr& e,State& state){
   if(!e.type.is_move_only()||e.children.size()!=1||
      e.children[0]->kind!=E::Variable)
    fail(e.where,"unique projection transfer requires a direct named place");
   const auto& root=*e.children[0];auto binding=find(root.text);
    if(!binding||root.reference!=Expr::Reference::Local||
       binding->type!=root.type)
     fail(e.where,"unique projection transfer has no checked local root");
    access(binding,e.where,root.text,Access::Consume);
    if(binding->borrow!=BorrowMode::Value)
     ref_error(e.where,"cannot move a field out of borrowed place '"+root.text+"'");
   if(state.moved.contains(binding->id))
    fail(e.where,"projection from moved local: "+root.text);
   auto part=std::pair{binding->id,part_name(e)};
   if(!state.moved_parts.insert(part).second)
    fail(e.where,"unique field is moved more than once from local: "+root.text);
   for(auto it=state.tags.begin();it!=state.tags.end();)
     if(it->first==binding->id)it=state.tags.erase(it);else ++it;
   }
   Type borrowed_type(const Type& source,const Location& at){
    if(!source.is_own())return source;
    if(ipc_kernel_opaque_owner(source))ipc_kernel_opaque_error(at);
    const auto& target=source.owned();
    return catalog.instantiate(target.nominal(),target.arguments(),at);
   }
   const Local& borrow_source(const Expr& argument,State& state,
                              std::size_t depth){
    expression_header(argument,depth,false);
    if(argument.kind!=E::Variable||
       argument.reference!=Expr::Reference::Local||
       argument.move_out||argument.borrow!=BorrowMode::Value||
       argument.callee||!argument.children.empty()||!argument.body.empty()||
       argument.prototype||argument.noreturn)
     ref_error(argument.where,
               "borrow requires one checked named addressable local place");
    auto place=find(argument.text);
    if(!place||place->type!=argument.type||!place->addressable)
      ref_error(argument.where,
                "borrow requires an initialized named addressable local place "
                "or incoming borrowed parameter");
    if(place->prototype)
     ref_error(argument.where,"borrow cannot originate from a non-executable prototype");
    if(place->storage&&!state.initialized.contains(place->storage))
     fail(argument.where,"borrow of uninitialized canonical storage");
    if(state.moved.contains(place->id)||has_moved_part(state,place->id))
     ref_error(argument.where,"borrow of moved or partially moved place '"+argument.text+"'");
    return *place;
   }
   void acquire(const Local& source,BorrowMode mode,const Location& at,
                const std::string& name){
    if(mode==BorrowMode::Value)
     ref_error(at,"borrow acquisition has no borrow mode");
    if(source.borrow==BorrowMode::Shared&&mode==BorrowMode::Exclusive)
     ref_error(at,"cannot take an exclusive reborrow from shared borrowed place '"+name+"'");
    each_loan(at,[&](const Loan& loan,std::size_t loan_id){
     if(loan_id==source.id||loan.root!=source.root)return;
     bool conflict=false;
     if(source.borrow==BorrowMode::Value)
      conflict=mode==BorrowMode::Exclusive||
               loan.mode==BorrowMode::Exclusive;
     else if(source.borrow==BorrowMode::Exclusive)
      conflict=ancestor(source.id,loan.origin,at);
     else
      // An exclusive ancestor is already suspended by this shared reborrow;
      // it must not prevent a further shared descendant. Exclusive siblings,
      // descendants, and call-scoped loans still conflict.
      conflict=loan.mode==BorrowMode::Exclusive&&
               !(loan_id&&ancestor(loan_id,source.id,at));
     if(conflict)
      ref_error(at,(mode==BorrowMode::Exclusive?
        std::string("overlapping exclusive borrow of '"):
        std::string("conflicting shared borrow of '"))+name+
        "' while another borrow is live");
    });
   }
   void invalidate_loan_tags(State& state,std::size_t root,const Location& at){
    for(auto it=state.tags.begin();it!=state.tags.end();){
     const auto* binding=by_id(it->first,at);
     if(!binding||binding->root==root)it=state.tags.erase(it);else ++it;
    }
   }
   void lexical_borrow(const Stmt& statement,State& state,
                       std::size_t depth){
    if(csp::present(unit))
     ref_error(statement.where,
               "lexical borrows cannot cross or coexist with cooperative "
               "suspension/task boundaries");
    const auto& borrow=*statement.value;
    expression_header(borrow,depth,false);
    if(borrow.kind!=E::Borrow||borrow.children.size()!=1||
       !borrow.children[0]||statement.pattern||statement.mutable_binding||
       statement.name.empty())
     ref_error(statement.where,
               "lexical borrow requires a simple immutable let binding");
    const auto& source_expression=*borrow.children[0];
    const auto& source=borrow_source(source_expression,state,depth+1);
    const auto expected=borrowed_type(source.type,borrow.where);
    if(borrow.type!=expected)
     ref_error(borrow.where,"checked lexical borrow referent type mismatch");
    if(statement.declared!=Type::Invalid&&statement.declared!=borrow.type)
     fail(statement.where,"canonical borrow annotation/type mismatch");
    if(borrow.type==Type::Void||borrow.type==Type::Json||
       borrow.type==Type::JsonView||csp::contains(borrow.type))
     ref_error(borrow.where,"unsupported lexical borrow referent type");
    acquire(source,borrow.borrow,borrow.where,source_expression.text);
    bind(statement.name,borrow.type,false,nullptr,borrow.borrow,true,false,
         source.id,source.root);
    loan_tracking=true;
    if(slots&&!generic_context&&source.type==borrow.type&&
       (source.scalar_root_statement||source.scalar_root_parameter)){
     auto& alias=scopes.back().at(statement.name);
     alias.scalar_root_statement=source.scalar_root_statement;
     alias.scalar_root_parameter=source.scalar_root_parameter;
     alias.planned_binding=&statement;
     slots->bind({&statement,&source_expression,source.planned_binding,
                  alias.scalar_root_statement,alias.scalar_root_parameter,
                  borrow.borrow,borrow.type});
    }
   }
  const Function* borrower(const Expr& e){
   if(e.kind!=E::Variable||find(e.text))return nullptr;
   auto found=borrowers.find(e.text);return found==borrowers.end()?nullptr:found->second;
  }
  Type intrinsic(const Expr& call){
   const auto& name=call.callee->text;
   auto arity=[&](std::size_t count){
    if(call.children.size()!=count)fail(call.where,"intrinsic arity mismatch: "+name);
   };
   if(auto primitive=data_primitive(name)){
    arity(primitive->parameters().size());
    for(std::size_t i=0;i<call.children.size();++i)
     if(call.children[i]->type!=primitive->parameters()[i])
      fail(call.children[i]->where,"intrinsic argument type mismatch: "+name);
    return primitive->result();
   }
   if(name=="println"){
    arity(1);
    if(!literals::formattable(call.children[0]->type))
     fail(call.children[0]->where,"invalid println operand");
    return Type::Void;
   }
   if(name=="format"){
    if(call.children.empty()||call.children.size()>literals::max_format_parts+1)
     fail(call.where,"invalid format arity");
    if(call.children[0]->type!=Type::String)
     fail(call.children[0]->where,"format pattern must be string");
    for(std::size_t i=1;i<call.children.size();++i)
     if(!literals::formattable(call.children[i]->type))
      fail(call.children[i]->where,"invalid format operand");
    return Type::String;
   }
   if(name=="assert"){
    arity(1);
    if(call.children[0]->type!=Type::Bool)fail(call.where,"assert requires bool");
     return Type::Void;
    }
    if(name=="box"){
     arity(1);const auto& value=call.children[0]->type;
      if(ipc_kernel_opaque_nominal(value.nominal()))ipc_kernel_opaque_error(call.where);
     if((value.kind()!=Type::Record&&value.kind()!=Type::Sum)||
        value.nominal().empty())
      fail(call.children[0]->where,"box operand is not a checked nominal value");
     auto expected=Type::own(Type::named(value.nominal(),value.arguments()));
     if(call.type!=expected)fail(call.where,"box result does not match its nominal operand");
     return expected;
    }
    if(name=="move"){
     arity(1);const auto& operand=*call.children[0];
     if(operand.kind!=E::Variable||operand.reference!=Expr::Reference::Local||
        !operand.move_out||!operand.type.is_move_only())
      fail(operand.where,"move requires one checked unique local transfer");
     return operand.type;
    }
    if(name=="take"){
     arity(1);const auto& owner=call.children[0]->type;
     if(!owner.is_own())fail(call.children[0]->where,"take requires own<Named>");
     if(ipc_kernel_opaque_owner(owner))ipc_kernel_opaque_error(call.where);
     const auto& target=owner.owned();
     if((call.type.kind()!=Type::Record&&call.type.kind()!=Type::Sum)||
        call.type.nominal()!=target.nominal()||
        call.type.arguments()!=target.arguments())
      fail(call.where,"take result does not match its owning target");
     return call.type;
    }
    if(name=="drop"){
     arity(1);
     if(!call.children[0]->type.is_move_only())
      fail(call.children[0]->where,"drop requires a unique owning value");
     return Type::Void;
    }
   if(name=="json_parse_or_die"){
    arity(1);
    if(call.children[0]->type!=Type::String)
     fail(call.where,"json_parse_or_die requires string");
    return Type::Json;
   }
   if(name=="json_get_or_die"||name=="json_size"||
      name=="json_number"||name=="json_print"){
    arity(name=="json_get_or_die"?2:1);
    const auto owner=call.children[0]->type;
    if(owner!=Type::Json&&owner!=Type::JsonView)
     fail(call.children[0]->where,"invalid JSON operand type");
    if(call.children[0]->kind!=E::Variable)
     fail(call.children[0]->where,"JSON operation requires a named owner or view");
    if(call.children.size()==2&&call.children[1]->type!=Type::String)
     fail(call.children[1]->where,"JSON key must be string");
    if(name=="json_get_or_die")return Type::JsonView;
    if(name=="json_print")return Type::Void;
    return Type::I64;
   }
   fail(call.where,"unknown checked intrinsic: "+name);
  }
  bool borrow_call(const Expr& e,const Function& function,State& state,std::size_t depth){
   const auto& callee=*e.callee;expression_header(callee,depth+1,true);
   if(callee.reference!=Expr::Reference::BorrowFunction||callee.callee||!callee.children.empty()||
      !callee.body.empty()||callee.text!=function.name||function.prototype||
      e.children.size()!=function.params.size()||e.type!=function.result)
     ref_error(e.where,"malformed checked call with borrowed parameters");
  std::vector<Type> parameter_types;
  for(const auto& parameter:function.params)parameter_types.push_back(parameter.type);
  if(callee.type!=Type::function(std::move(parameter_types),function.result))
    ref_error(callee.where,"checked borrowed callee signature mismatch");
   const auto acquired=call_loans.size();
   bool live=true;
   for(std::size_t i=0;i<e.children.size();++i){
     if(!e.children[i])ref_error(e.where,"missing checked call argument");
     const auto& arg=*e.children[i];const auto& param=function.params[i];
     if(param.borrow==BorrowMode::Value){
      if(arg.type!=param.type)
       ref_error(arg.where,"checked call argument type mismatch");
      if(!expression(arg,state,depth+1)){live=false;break;}
      continue;
     }
     const auto& place=borrow_source(arg,state,depth+1);
     if(borrowed_type(place.type,arg.where)!=param.type)
      ref_error(arg.where,"checked borrowed call argument type mismatch");
     acquire(place,param.borrow,arg.where,arg.text);
     call_loans.push_back({param.borrow,place.id,place.root});
   }
   if(live)for(std::size_t i=acquired;i<call_loans.size();++i)
    if(call_loans[i].mode==BorrowMode::Exclusive)
     invalidate_loan_tags(state,call_loans[i].root,e.where);
   call_loans.resize(acquired);
   return live;
  }
 std::pair<std::size_t,std::string> tag(const Expr& e){
  if(e.children.size()!=1||!e.children[0]||e.children[0]->kind!=E::Variable)fail(e.where,"sum decision operand must be a named value");
  const auto& root=*e.children[0];auto binding=find(root.text);if(!binding||binding->mut||binding->type!=root.type||root.type.kind()!=Type::Sum)fail(e.where,"sum decision requires an immutable closed-sum binding");
  auto found=std::find(root.type.labels().begin(),root.type.labels().end(),e.text);if(found==root.type.labels().end()||e.projection!=static_cast<std::size_t>(found-root.type.labels().begin()))fail(e.where,"sum decision tag/projection mismatch");
  return {binding->id,e.text};
 }
   bool expression(const Expr& e,State& state,std::size_t depth,
                    bool callee_position=false,bool unsafe_direct=false,
                    bool place_base=false){
    expression_header(e,depth,callee_position);
    if(e.prototype){
      if(!borrowers.empty())
       ref_error(e.where,"non-executable prototype in a checked borrowed expression");
      return true;
    }
    if(e.kind==E::PatternMatch||e.kind==E::Match||e.kind==E::Case||
       e.kind==E::Loop||e.kind==E::Propagate||e.kind==E::UnsafeBlock||
       e.kind==E::SourceCsp)
     fail(e.where,"surface value construct reached the canonical verifier");
    if(e.kind==E::ClicheMember||e.kind==E::InherentMember)
     fail(e.where,"unresolved member projection reached checked executable HIR");
    if(e.kind==E::MacroCall||e.kind==E::SyntaxHole||e.kind==E::SyntaxBlock||
       e.kind==E::Extension||e.kind==E::SyntaxList||e.kind==E::SyntaxVector||
       e.kind==E::SyntaxKeyword||e.kind==E::SyntaxLet||
       e.kind==E::Splice||e.kind==E::Lift)
     fail(e.where,"unexpanded metaprogramming/staging syntax reached canonical HIR");
    if(e.kind==E::Call&&e.callee)if(auto f=borrower(*e.callee))return borrow_call(e,*f,state,depth);
    if(e.reference==Expr::Reference::BorrowFunction||borrower(e))
     ref_error(e.where,"functions with borrowed parameters require a direct named call");
    if(e.kind==E::Borrow)
     ref_error(e.where,"borrow expression escaped its direct lexical let binding");
   if(e.kind==E::Variable){
    if(e.text.empty()||e.callee||!e.children.empty()||!e.body.empty())
     fail(e.where,"malformed canonical variable");
    auto binding=find(e.text);
     if(binding||e.reference==Expr::Reference::Local){
      if(!binding||e.reference!=Expr::Reference::Local||binding->type!=e.type){
       fail(e.where,"canonical local binding/type mismatch: "+e.text);
      }
      if(binding->prototype&&!generic_context)
       fail(e.where,"non-executable prototype local reached executable HIR: "+e.text);
       access(binding,e.where,e.text,
              e.move_out?Access::Consume:Access::Read);
      if(binding->storage&&!state.initialized.contains(binding->storage))
       fail(e.where,"read of uninitialized canonical storage: "+e.text);
       if(e.move_out){
        if(binding->borrow!=BorrowMode::Value)
         ref_error(e.where,"cannot consume or move borrowed place '"+e.text+"'");
        if(!binding->type.is_move_only())
        fail(e.where,"move metadata on a copyable local: "+e.text);
       if(state.moved.contains(binding->id)||has_moved_part(state,binding->id))
        fail(e.where,"use after move of local: "+e.text);
       state.moved.insert(binding->id);
       for(auto it=state.tags.begin();it!=state.tags.end();)
        if(it->first==binding->id)it=state.tags.erase(it);else ++it;
      }else if(binding->type.is_move_only()){
       if(state.moved.contains(binding->id))
        fail(e.where,"use after move of local: "+e.text);
       if(has_moved_part(state,binding->id)&&!place_base)
        fail(e.where,"whole-value use after partial move of local: "+e.text);
       if(!place_base)
        fail(e.where,"copy of unique local requires move(local): "+e.text);
      }else if(e.move_out)fail(e.where,"move metadata on a copyable local");
      if(slots&&!generic_context&&binding->planned_binding)
       slots->read(e,*binding->planned_binding);
      return true;
    }
    if(e.reference==Expr::Reference::Intrinsic){
     if(!callee_position||e.type!=Type::Invalid||functions.contains(e.text))
      fail(e.where,"intrinsic is not a valid direct callee");
     return true;
    }
    auto found=functions.find(e.text);
    if(found==functions.end())fail(e.where,"unresolved canonical global: "+e.text);
    const auto expected=function_reference(*found->second);
    if(e.reference!=expected||e.type!=signature(*found->second))
     fail(e.where,"canonical global reference/signature mismatch: "+e.text);
     if(expected==Expr::Reference::BorrowFunction)
      ref_error(e.where,"functions with borrowed parameters require a direct named call");
    if(expected==Expr::Reference::Extern||expected==Expr::Reference::ScalarForeign){
     if(!callee_position)fail(e.where,"foreign function is not a first-class value");
    }else if(expected==Expr::Reference::UnsafeForeign){
     if(!callee_position||!unsafe_direct||!unsafe_context)
      fail(e.where,"unsafe foreign reference must be a direct lexically unsafe call");
    }
    return true;
   }
   if(e.kind==E::Lambda){
    std::vector<Type> parameter_types;std::set<std::string> names,captured;
     for(const auto& p:e.parameters){
      if(p.borrow!=BorrowMode::Value)
       ref_error(p.where,"anonymous functions cannot declare borrowed parameters");
     if(!names.insert(p.name).second)fail(p.where,"duplicate lambda parameter");
     parameter_types.push_back(p.type);
    }
    if(e.type!=Type::function(std::move(parameter_types),e.result)||
       e.callee||!e.children.empty()||!e.labels.empty()||!e.proofs.empty())
     fail(e.where,"canonical lambda signature/shape mismatch");
    for(const auto& capture:e.captures){
     if(capture.name.empty()||!captured.insert(capture.name).second){
      fail(e.where,"empty/duplicate closure capture");
     }
     auto b=find(capture.name);
      if(!b||b->mut||b->type!=capture.type||capture.type==Type::Json||
         capture.type==Type::JsonView||capture.type.contains_unique()||
         (b->storage&&!state.initialized.contains(b->storage)))
      fail(e.where,"uninitialized/invalid closure capture");
     access(b,e.where,capture.name);
      if(b->borrow!=BorrowMode::Value)
       ref_error(e.where,"closure cannot capture a borrowed place");
    }
     auto saved_scopes=std::move(scopes);
      auto saved_regions=std::move(regions),saved_loops=std::move(loops);
      auto saved_call_loans=std::move(call_loans);
      auto saved_loan_tracking=loan_tracking;
      scopes.clear();regions.clear();loops.clear();call_loans.clear();
      scopes.emplace_back();
      loan_tracking=false;
     for(const auto& capture:e.captures)
      bind(capture.name,capture.type,false,nullptr,BorrowMode::Value,false);
     auto saved_returns=returns;returns=e.result;params(e.parameters);
     auto saved_unsafe=unsafe_context;unsafe_context=false;
     auto after=body(e.body,State{},depth+1,false);
     if(e.result!=Type::Void&&after)fail(e.where,"lambda lacks a complete return path");
     returns=saved_returns;unsafe_context=saved_unsafe;
      scopes=std::move(saved_scopes);regions=std::move(saved_regions);
      loops=std::move(saved_loops);call_loans=std::move(saved_call_loans);
      loan_tracking=saved_loan_tracking;
      return true;
   }
   if(e.kind==E::Region){
    if(e.children.size()!=1||!e.children[0])fail(e.where,"value region requires one result");
    auto saved_unsafe=unsafe_context;unsafe_context=unsafe_context||e.unsafe_region;
   scopes.emplace_back();auto after=body(e.body,state,depth+1,false);
   if(e.noreturn==after.has_value())fail(e.where,"incorrect value-region completion metadata");
   if(after){if(!expression(*e.children[0],*after,depth+1)||e.children[0]->type!=e.type)fail(e.where,"value-region result mismatch");state=*after;}
   else if(e.type!=Type::Void||e.children[0]->kind!=E::Unit)fail(e.where,"non-returning region must have no value");
    scopes.pop_back();unsafe_context=saved_unsafe;return after.has_value();
  }
   if(e.kind==E::Binary&&(e.text=="&&"||e.text=="||")){
    if(e.children.size()!=2)fail(e.where,"malformed logical expression");
    if(!expression(*e.children[0],state,depth+1))return false;
    charge(state,e.where);auto right=state;
    bool live=expression(*e.children[1],right,depth+1);
    if(e.children[0]->type!=Type::Bool||e.children[1]->type!=Type::Bool||
       e.type!=Type::Bool)
     fail(e.where,"logical expression type mismatch");
    if(live)state=meet(state,right);
    return true;
   }
    if(e.kind==E::Call){
     if(!e.callee)fail(e.where,"canonical call is missing its callee");
     const bool unsafe=e.callee->reference==Expr::Reference::UnsafeForeign;
     if(!expression(*e.callee,state,depth+1,true,unsafe))return false;
     if(e.callee->reference==Expr::Reference::Intrinsic&&e.callee->text=="ref_replace"){
      if(e.children.size()!=2||e.children[0]->type!=e.type||e.children[1]->type!=e.type)
       ref_error(e.where,"invalid checked ref_replace signature");
      const auto& target=*e.children[0];
      const auto& place=borrow_source(target,state,depth+1);
      if(place.borrow!=BorrowMode::Exclusive)
       ref_error(target.where,"ref_replace requires a named exclusive borrowed place");
      const auto root=place.root;
      const auto* planned_binding=place.planned_binding;
      acquire(place,BorrowMode::Exclusive,target.where,target.text);
      const auto acquired=call_loans.size();
      call_loans.push_back({BorrowMode::Exclusive,place.id,root});
      const bool live=expression(*e.children[1],state,depth+1);
      call_loans.resize(acquired);
      if(live){
       invalidate_loan_tags(state,root,e.where);
       if(slots&&!generic_context&&planned_binding)
        slots->exchange(e,*planned_binding);
      }
      return live;
     }
     if(e.callee->reference==Expr::Reference::Intrinsic)
      if(const auto* operation=shared_intrinsic(e.callee->text)){
       if(e.type!=shared_signature(*operation,e))
        shared_error(e.where,"canonical shared call result type mismatch");
       const auto mode=operation->loan();
       if(mode==BorrowMode::Value)return expression(*e.children[0],state,depth+1);
       const auto& owner=*e.children[0];
       const auto& place=borrow_source(owner,state,depth+1);
       acquire(place,mode,owner.where,owner.text);
       const auto acquired=call_loans.size();
       call_loans.push_back({mode,place.id,place.root});
       bool live=true;
       for(std::size_t i=1;i<e.children.size()&&live;++i)
        live=expression(*e.children[i],state,depth+1);
       call_loans.resize(acquired);
       return live;
      }
     const auto kernel=unsafe||e.callee->reference==Expr::Reference::ScalarForeign?
      std::string_view(functions.at(e.callee->text)->kernel_symbol):std::string_view{};
     const bool session_loan=ipc_kernel_session_loan(kernel);
     const auto loans_before=call_loans.size();
     for(std::size_t i=0;i<e.children.size();++i){
      const auto& child=*e.children[i];
      if(i==0&&session_loan){
       const auto& place=borrow_source(child,state,depth+1);
       if(place.type!=ipc_kernel_session_owner()||place.borrow!=BorrowMode::Value)
        ref_error(child.where,"framed session operation requires a named session owner");
       acquire(place,BorrowMode::Exclusive,child.where,child.text);
       call_loans.push_back({BorrowMode::Exclusive,place.id,place.root});
      }else if(!expression(child,state,depth+1)){
       call_loans.resize(loans_before);
       return false;
      }
     }
     call_loans.resize(loans_before);
    Type result=Type::Invalid;
    if(e.callee->reference==Expr::Reference::Intrinsic)result=intrinsic(e);
    else{
     const auto& callable=e.callee->type;
     if(!callable.is_function()||callable.parameters().size()!=e.children.size())
      fail(e.where,"canonical call target/arity mismatch");
      for(std::size_t i=0;i<e.children.size();++i)
       if(kernel_span_argument(kernel,i)?!kernel_span_type(e.children[i]->type):
                                          e.children[i]->type!=callable.parameters()[i])
        fail(e.children[i]->where,"canonical call argument type mismatch");
     result=callable.result();
    }
    if(e.type!=result)fail(e.where,"canonical call result type mismatch");
    return true;
   }
    const bool place_operation=e.kind==E::TagTest||e.kind==E::Payload||
      e.kind==E::Project||e.kind==E::Field||e.kind==E::Index||
      e.kind==E::Slice||e.kind==E::Length;
    if(place_operation&&!e.children.empty()){
     if(!expression(*e.children[0],state,depth+1,false,false,true))return false;
     for(std::size_t i=1;i<e.children.size();++i)
      if(!expression(*e.children[i],state,depth+1))return false;
    }else for(const auto& child:e.children)
     if(!expression(*child,state,depth+1))return false;
   if(e.kind==E::Integer){
    if(!e.children.empty())fail(e.where,"integer literal has operands");
    auto type=numeric_literal_type(e.text);
    (void)integer_literal_bits(e.text,e.where);
    if(e.type!=type)fail(e.where,"integer literal/type mismatch");
   }
   if(e.kind==E::Float){
    if(!e.children.empty())fail(e.where,"float literal has operands");
    auto type=numeric_literal_type(e.text);
    (void)float_literal_bits(e.text,e.where);
    if(!type.is_float()||e.type!=type)fail(e.where,"float literal/type mismatch");
   }
   if(e.kind==E::Boolean){
    if(!e.children.empty()||(e.text!="true"&&e.text!="false")||e.type!=Type::Bool)
     fail(e.where,"invalid canonical boolean literal");
   }
   if(e.kind==E::String){
    if(!e.children.empty()||e.type!=Type::String)
     fail(e.where,"invalid canonical string literal");
    string_literal(e);
   }
   if(e.kind==E::Character){
    if(!e.children.empty()||e.type!=Type::Char)
     fail(e.where,"invalid canonical character literal");
    (void)literals::character(e.text,e.where);
   }
   if(e.kind==E::ByteBuffer){
    if(!e.children.empty()||e.type!=Type::Buffer||!literals::byte_payload(e.text))
     fail(e.where,"invalid canonical byte-buffer literal");
   }
   if(e.kind==E::Interpolated){
    if(e.children.empty()||e.children.size()>literals::max_format_parts||
       e.type!=Type::String)
     fail(e.where,"invalid canonical interpolation");
    for(const auto& child:e.children)
     if(!literals::formattable(child->type))
      fail(child->where,"invalid canonical interpolation operand");
   }
   if(e.kind==E::Tuple){
    if(e.children.size()<2)fail(e.where,"invalid canonical tuple arity");
    std::vector<Type> elements;
    for(const auto& child:e.children)elements.push_back(child->type);
    if(e.type!=Type::tuple(std::move(elements)))
     fail(e.where,"canonical tuple result type mismatch");
   }
   if(e.kind==E::Unary){
    if(e.children.size()!=1||(e.text!="!"&&e.text!="-"))
     fail(e.where,"invalid canonical unary operator/arity");
    const auto operand=e.children[0]->type;
    if(e.text=="!"){
     if(operand!=Type::Bool||e.type!=Type::Bool)
      fail(e.where,"logical negation type mismatch");
    }else if((operand!=Type::I64&&!operand.is_float())||e.type!=operand)
     fail(e.where,"numeric negation type mismatch");
   }
   if(e.kind==E::Binary&&e.text!="&&"&&e.text!="||"){
    if(e.children.size()!=2)fail(e.where,"invalid canonical binary arity");
    const auto left=e.children[0]->type,right=e.children[1]->type;
    const bool arithmetic=e.text=="+"||e.text=="-"||e.text=="*"||
                          e.text=="/"||e.text=="%";
    const bool compare=e.text=="<"||e.text==">"||e.text=="<="||e.text==">=";
    const bool equality=e.text=="=="||e.text=="!=";
    if(!arithmetic&&!compare&&!equality)fail(e.where,"unknown canonical binary operator");
    if(left!=right)fail(e.where,"canonical binary operand type mismatch");
    if(arithmetic||compare){
     if(!left.is_numeric()||(e.text=="%"&&left.is_float()))
      fail(e.where,"invalid canonical numeric operand");
    }else if(!literals::formattable(left))
     fail(e.where,"invalid canonical equality operand");
    const auto result=arithmetic?left:Type{Type::Bool};
    if(e.type!=result)fail(e.where,"canonical binary result type mismatch");
   }
   if(e.kind==E::Array){
    if(e.type.kind()!=Type::Array||e.type.extent()!=e.children.size())
     fail(e.where,"invalid canonical array construction");
    for(const auto& child:e.children)
     if(child->type!=e.type.elements()[0])
      fail(child->where,"canonical array element type mismatch");
   }
    if(e.kind==E::Index||e.kind==E::Slice||e.kind==E::Length){
    const auto arity=e.kind==E::Length?1u:e.kind==E::Slice?3u:2u;
    if(e.children.size()!=arity)fail(e.where,"canonical index/slice/length arity mismatch");
     const auto subject=e.children[0]->type;
     if(subject==Type::RegexCaptures){
      if(e.kind==E::Index){
       if((!e.text.empty()&&e.text!=":")||
          e.children[1]->type!=Type::I64||e.type!=Type::String)
        fail(e.where,"invalid canonical capture index");
      }else if(e.kind==E::Length){
       if(e.type!=Type::I64)fail(e.where,"invalid canonical capture count");
      }else fail(e.where,"captures do not support slicing");
     }else{
      if(!e.text.empty())
       fail(e.where,"capture index marker on a non-capture expression");
      if(subject.kind()!=Type::Array&&subject.kind()!=Type::Slice)
       fail(e.where,"canonical index/slice/length requires an array or slice");
     for(std::size_t i=1;i<e.children.size();++i)
      if(e.children[i]->type!=Type::I64)
       fail(e.children[i]->where,"canonical index/slice bound is not i64");
     const auto result=e.kind==E::Length?Type{Type::I64}:
       e.kind==E::Slice?Type::data(Type::Slice,subject.elements(),{},subject.extent()):
       subject.elements()[0];
      if(e.type!=result)fail(e.where,"canonical index/slice/length result mismatch");
     }
     if(e.move_out)fail(e.where,"unique array-element moves are not admitted");
    }
   if(e.kind==E::Quote){
    if(!unit.stages_lowered||e.children.size()!=1||
       !e.children[0]->type.is_function()||
       !e.children[0]->type.parameters().empty()||
       e.type!=Type::code(e.children[0]->type.result()))
     fail(e.where,"invalid canonical quotation");
   }
   if(e.kind==E::Run){
    if(e.children.size()!=1||!e.children[0]->type.is_code()||
       e.type!=e.children[0]->type.code_result())
     fail(e.where,"invalid canonical run expression");
   }
   if(e.kind==E::PolyProjection){
    if(e.children.size()!=1||!e.children[0]->type.is_forall()||
       !forall_instance(e.children[0]->type,e.type))
     fail(e.where,"invalid canonical forall projection");
    auto layout=unit.forall_layouts.find(e.children[0]->type);
    if(layout==unit.forall_layouts.end()||e.projection>=layout->second.size()||
       layout->second[e.projection]!=e.type)
     fail(e.where,"forall projection/layout mismatch");
   }
   if(e.kind==E::PolyPack){
    if(!e.type.is_forall()||e.proofs.size()!=1)
     fail(e.where,"invalid canonical forall pack proof shape");
    auto proof_state=state;auto saved_generic=generic_context;generic_context=true;
    bool proof_live=expression(*e.proofs[0],proof_state,depth+1);
    generic_context=saved_generic;
    if(!proof_live||!forall_proof(e.type,e.proofs[0]->type))
     fail(e.where,"invalid canonical rigid forall proof");
    auto layout=unit.forall_layouts.find(e.type);
    const std::vector<Type> empty;
    const auto& expected=layout==unit.forall_layouts.end()?empty:layout->second;
    if(e.children.size()!=expected.size())
     fail(e.where,"canonical forall pack/layout arity mismatch");
    for(std::size_t i=0;i<expected.size();++i)
     if(e.children[i]->type!=expected[i])
      fail(e.children[i]->where,"canonical forall pack element type mismatch");
   }
   if(e.kind==E::RegexLiteral&&(e.type!=Type::Regex||!e.children.empty()||e.callee))
    fail(e.where,"invalid canonical regex literal");
  if(e.kind==E::RegexMatch||e.kind==E::RegexCapture){
   if(e.children.size()!=2||e.callee||e.children[0]->type!=Type::Regex||e.children[1]->type!=Type::String||
      e.type!=(e.kind==E::RegexMatch?Type::Bool:Type::RegexCaptures))
    fail(e.where,"invalid canonical regex operation");
  }
   if(e.kind==E::TagTest){(void)tag(e);if(e.move_out||e.type!=Type::Bool)fail(e.where,"tag test result is not bool");}
   if(e.kind==E::Payload){
    auto fact=tag(e);
    if(!state.tags.contains(fact))fail(e.where,"payload projection lacks a dominating tag test");
    if(e.type!=e.children[0]->type.elements().at(e.projection))fail(e.where,"payload result type mismatch");
    if(e.move_out)move_part(e,state);
    else if(e.type.is_move_only())fail(e.where,"copy of unique sum payload");
   }
    if(e.kind==E::Project){
     if(e.children.size()!=1||!e.children[0]->type.is_tuple()||e.projection>=e.children[0]->type.elements().size()||e.type!=e.children[0]->type.elements()[e.projection])fail(e.where,"invalid canonical tuple projection");
     if(e.move_out)move_part(e,state);
     else if(e.type.is_move_only())fail(e.where,"copy of unique tuple field");
    }
    if(e.kind==E::Field){
     if(e.children.size()!=1)fail(e.where,"invalid canonical field projection");
     const auto& subject=e.children[0]->type;
     if(subject.kind()!=Type::Record||e.projection>=subject.elements().size()||
        subject.labels()[e.projection]!=e.text||e.type!=subject.elements()[e.projection])
      fail(e.where,"invalid canonical field projection");
     if(e.move_out)move_part(e,state);
     else if(e.type.is_move_only())fail(e.where,"copy of unique record field");
    }
   if(e.kind==E::Record){
    if(e.type.kind()!=Type::Record||e.children.size()!=e.type.elements().size()||e.children.size()!=e.labels.size())
     fail(e.where,"invalid canonical record construction");
    std::set<std::string_view> initialized;
    for(std::size_t i=0;i<e.children.size();++i){
     auto label=std::lower_bound(e.type.labels().begin(),e.type.labels().end(),e.labels[i]);
     if(label==e.type.labels().end()||*label!=e.labels[i]||!initialized.insert(e.labels[i]).second||
        e.children[i]->type!=e.type.elements()[static_cast<std::size_t>(label-e.type.labels().begin())])
      fail(e.where,"invalid canonical record field");
    }
   }
   if(e.kind==E::Variant){if(e.type.kind()!=Type::Sum||e.result!=e.type||e.children.size()!=1||e.projection>=e.type.elements().size()||e.text!=e.type.labels()[e.projection]||e.children[0]->type!=e.type.elements()[e.projection])fail(e.where,"invalid variant construction");}
  if(e.kind==E::Unit&&(e.type!=Type::Void||!e.children.empty()||!e.body.empty()))fail(e.where,"invalid unit value");
  if(!e.body.empty())fail(e.where,"unexpected canonical expression body");
  return true;
 }
  Flow body(const std::vector<Stmt>& statements,State initial,std::size_t depth,
            bool scoped=true,bool prototype_scope=false){
  if(scoped)scopes.emplace_back();
  Flow state=std::move(initial);
   for(const auto& s:statements){
      tick(s.where,depth);source(s.where);
      if(s.context)fail(s.where,"source statement context reached canonical HIR");
      if(s.prototype&&!prototype_scope)
       fail(s.where,"specialization prototype outside a generic template");
     if(!s.instances.empty())fail(s.where,"unfrozen specialization instances in checked HIR");
      if(s.declared!=Type::Invalid)
       checked_type(s.declared,s.where,
                    generic_context||s.prototype||s.kind==S::Template);
      if(s.prototype){
       if(s.kind!=S::Let||!s.value||s.mutable_binding||s.pattern||
          !s.body.empty()||!s.otherwise.empty()||s.name.empty())
        fail(s.where,"malformed non-executable specialization prototype");
       auto prototype_state=*state;auto saved_generic=generic_context;
       generic_context=true;
       bool completes=expression(*s.value,prototype_state,depth+1);
       generic_context=saved_generic;
       if(!completes)fail(s.where,"non-completing specialization prototype");
       if(s.value->type==Type::Void)
        fail(s.where,"specialization prototype has void type");
       if(s.declared!=Type::Invalid&&s.declared!=s.value->type)
        fail(s.where,"specialization prototype annotation/type mismatch");
        bind(s.name,s.value->type,false,nullptr,BorrowMode::Value,true,true);
       continue;
      }
     if(!state)fail(s.where,"unreachable canonical statement");
     if(s.ref_store&&s.kind!=S::Assign)ref_error(s.where,"reference store marker on a non-assignment");
     if(s.unsafe_block&&s.kind!=S::Block)fail(s.where,"unsafe marker on a non-block statement");
     if(s.mutable_binding&&s.kind!=S::Let&&s.kind!=S::Template)
      fail(s.where,"mutable-binding metadata on a non-binding statement");
     if(s.pattern&&s.kind!=S::Let&&s.kind!=S::Template)
      fail(s.where,"binding pattern on a non-binding statement");
     if(s.inclusive)fail(s.where,"surface range metadata reached canonical HIR");
     if(!s.otherwise.empty()&&s.kind!=S::If&&s.kind!=S::While)
      fail(s.where,"else/latch body on an unrelated canonical statement");
     if(!s.body.empty()&&s.kind!=S::If&&s.kind!=S::While&&
        s.kind!=S::Block&&s.kind!=S::Template&&s.kind!=S::Region&&
        s.kind!=S::TaskGroup&&s.kind!=S::CspScope)
      fail(s.where,"body on an unrelated canonical statement");
     const bool value_allowed=s.kind==S::Let||s.kind==S::Template||
       s.kind==S::Assign||s.kind==S::Return||s.kind==S::If||
       s.kind==S::While||s.kind==S::Expr||s.kind==S::Initialize||
       s.kind==S::PushDefer||s.kind==S::TaskGroup||s.kind==S::CspScope;
     const bool value_required=s.kind==S::Let||s.kind==S::Template||
       s.kind==S::Assign||s.kind==S::If||s.kind==S::While||
       s.kind==S::Expr||s.kind==S::Initialize||s.kind==S::PushDefer;
     if(s.value&&!value_allowed)fail(s.where,"value on an unrelated canonical statement");
     if(!s.value&&value_required)fail(s.where,"missing canonical statement value");
     if(s.declared!=Type::Invalid&&s.kind!=S::Let&&s.kind!=S::Template&&
        s.kind!=S::Storage)
      fail(s.where,"type annotation on an unrelated canonical statement");
     if(!s.name.empty()&&s.kind!=S::Let&&s.kind!=S::Template&&
        s.kind!=S::Assign&&s.kind!=S::Storage&&s.kind!=S::Initialize)
      fail(s.where,"name on an unrelated canonical statement");
     if((!s.label.empty()||s.label_context)&&s.kind!=S::While&&
        s.kind!=S::Break&&s.kind!=S::Continue)
      fail(s.where,"loop-label metadata on an unrelated statement");
    charge(*state,s.where);
    if(s.kind==S::Storage||s.kind==S::Initialize||s.kind==S::Region||s.kind==S::Leave||s.kind==S::Trap||s.kind==S::PushDefer){
    source(s.where);if(s.pattern||s.mutable_binding||!s.label.empty()||s.label_context||s.context||s.inclusive||!s.otherwise.empty())fail(s.where,"extraneous canonical statement fields");
    if(s.kind!=S::Region&&!s.body.empty())fail(s.where,"body on a canonical non-region statement");
    if((s.kind==S::Region||s.kind==S::Leave||s.kind==S::Trap)&&(!s.name.empty()||s.value||s.declared!=Type::Invalid))fail(s.where,"invalid canonical transfer/region shape");
      }
      if(s.kind==S::Template){
       RetainedControl control;
       retained_expression(*s.value,control,depth+1);
       if(s.value->type==Type::Void)
        fail(s.where,"generic template source has void type");
       if(s.declared!=Type::Invalid&&s.declared!=s.value->type){
        fail(s.where,"generic template annotation/type mismatch");
       }
       state=body(s.body,*state,depth+1,false,true);continue;
       }
     if(s.kind==S::Let&&s.value&&s.value->kind==E::Borrow){
      lexical_borrow(s,*state,depth+1);
      continue;
     }
     // A backedge may have changed an active arm through an exclusive loan.
     if(s.kind==S::While)state->tags.clear();
     auto header_entry=s.kind==S::While?state:Flow{};
   if(s.value&&!expression(*s.value,*state,depth+1)){state.reset();continue;}
     switch(s.kind){
      case S::TaskGroup:case S::CspScope:
       if(s.value&&s.value->type!=Type::I64)
        fail(s.value->where,"task-group duration is not i64");
       state=body(s.body,*state,depth+1);break;
    case S::Defer:fail(s.where,"unlowered defer");
    case S::PushDefer:if(!s.name.empty()||s.declared!=Type::Invalid||!s.value||s.value->kind!=E::Lambda||s.value->type!=Type::function({},Type::Void)||!s.value->parameters.empty()||s.value->result!=Type::Void)fail(s.where,"invalid deferred thunk");break;
    case S::Let:
     if(s.value->type==Type::Void)fail(s.where,"canonical binding has void type");
     if(s.declared!=Type::Invalid&&s.declared!=s.value->type)
      fail(s.where,"canonical binding annotation/type mismatch");
     if(s.pattern)pattern(*s.pattern,s.value->type);
     else{
      bind(s.name,s.value->type,s.mutable_binding);
      plan_statement_root(s,s.value->type);
     }
     break;
   case S::Storage:
    if(s.value||s.pattern||s.declared==Type::Invalid||s.declared==Type::Void||s.declared==Type::Json||s.declared==Type::JsonView||s.declared.has_holes()||s.declared.has_generics())fail(s.where,"invalid canonical storage declaration");
     bind(s.name,s.declared,false,&s);plan_statement_root(s,s.declared);
     state->initialized.erase(&s);state->maybe_initialized.erase(&s);break;
     case S::Initialize:{auto b=find(s.name);if(!b||!b->storage||!s.value||b->type!=s.value->type||state->maybe_initialized.contains(b->storage))fail(s.where,"invalid or repeated canonical initialization");state->initialized.insert(b->storage);state->maybe_initialized.insert(b->storage);clear_moves(*state,b->id);break;}
    case S::Assign:{
     auto b=find(s.name);access(b,s.where,s.name,Access::Write);
     if(s.ref_store||(b&&b->borrow!=BorrowMode::Value)){
      if(!s.ref_store||!b||b->borrow!=BorrowMode::Exclusive||
         !s.value||s.value->type!=b->type)
       ref_error(s.where,"invalid checked exclusive-borrow store");
      }else if(!b||!b->mut)fail(s.where,"canonical assignment to missing/immutable binding");
     if(!b||b->type!=s.value->type)fail(s.where,"canonical assignment type mismatch");
     if(s.ref_store){
      invalidate_loan_tags(*state,b->root,s.where);
      if(slots&&!generic_context&&b->planned_binding)
       slots->store(s,*b->planned_binding);
     }else for(auto it=state->tags.begin();it!=state->tags.end();){
      if(it->first==b->id)it=state->tags.erase(it);else ++it;
     }
     clear_moves(*state,b->id);
     break;
   }
    case S::Expr:break;
   case S::Return:if((s.value?s.value->type:Type{Type::Void})!=returns)fail(s.where,"canonical return type mismatch");state.reset();break;
   case S::Trap:state.reset();break;
    case S::Block:{auto saved=unsafe_context;unsafe_context=unsafe_context||s.unsafe_block;
     state=body(s.body,*state,depth+1);unsafe_context=saved;break;}
   case S::If:{
    if(!s.value||s.value->type!=Type::Bool)fail(s.where,"invalid canonical branch condition");
    auto yes=*state;if(s.value->kind==E::TagTest)yes.tags.insert(tag(*s.value));
    auto a=body(s.body,std::move(yes),depth+1),b=body(s.otherwise,*state,depth+1);state=merge(std::move(a),b);break;
   }
   case S::Region:{
    regions.push_back({s.control_target,{},{}});auto after=body(s.body,*state,depth+1);auto exits=std::move(regions.back().exits);regions.pop_back();for(const auto& exit:exits)after=merge(std::move(after),exit);state=std::move(after);break;
   }
   case S::Leave:{auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==regions.rend())fail(s.where,"region exit target missing");target->exits.push_back(*state);state.reset();break;}
      case S::While:{
       if(s.value->type!=Type::Bool)fail(s.where,"canonical loop condition is not bool");
       std::set<const Stmt*> enclosing;std::set<std::size_t> enclosing_ids;
       for(const auto& scope:scopes)for(const auto& [name,local]:scope){
        (void)name;enclosing_ids.insert(local.id);if(local.storage)enclosing.insert(local.storage);
       }
       auto newly_moved=[&](const State& before,const State& after){
        for(auto id:enclosing_ids)
         if(!before.moved.contains(id)&&after.moved.contains(id))return true;
        for(const auto& part:after.moved_parts)
         if(enclosing_ids.contains(part.first)&&!before.moved_parts.contains(part))return true;
        return false;
       };
       if(newly_moved(*header_entry,*state))
        fail(s.where,"unique local is moved by a repeatable loop condition");
      loops.push_back({s.control_target,{},{}});auto normal=body(s.body,*state,depth+1);for(const auto& c:loops.back().continues)normal=merge(std::move(normal),c);
      if(normal){auto backedge=body(s.otherwise,*normal,depth+1);if(backedge){
       for(auto storage:enclosing)if(!header_entry->maybe_initialized.contains(storage)&&backedge->maybe_initialized.contains(storage))fail(s.where,"canonical storage initialized on a repeatable loop backedge");
       if(newly_moved(*header_entry,*backedge))
        fail(s.where,"unique local remains moved on a repeatable loop backedge");
      }}
    auto exits=std::move(loops.back().exits);loops.pop_back();for(const auto& exit:exits)state=merge(std::move(state),exit);state->tags.clear();break;
   }
    case S::Break:case S::Continue:{if(s.value)fail(s.where,"surface value exit reached the canonical verifier");auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==loops.rend())fail(s.where,"loop target missing");(s.kind==S::Break?target->exits:target->continues).push_back(*state);state.reset();break;}
   case S::Range:fail(s.where,"surface range in canonical values");
   case S::Template:break;
   }
  }
  if(scoped)scopes.pop_back();
  return state;
 }
 public:
   explicit Verify(const Unit& source,memgraph::detail::LexicalSlotBuilder* plan=nullptr)
    :unit(source),catalog(source),slots(plan){}
  void run(){
   for(const auto& [schema,instances]:unit.forall_layouts){
    checked_type(schema,{"<forall-layout>"});
    if(!schema.is_forall())fail({"<forall-layout>"},"non-forall layout key");
    std::set<Type> unique;
    for(const auto& instance:instances){
     checked_type(instance,{"<forall-layout>"});
     if(!forall_instance(schema,instance)||!unique.insert(instance).second)
      fail({"<forall-layout>"},"invalid/duplicate forall layout instance");
    }
   }
    for(const auto& f:unit.functions){
     source(f.where);
     verify_kernel_shape(f);
     if(ipc_kernel_symbol(f.kernel_symbol)&&csp::present(unit))
      fail(f.where,"framed transport cannot run in the cooperative concurrency profile");
     if(f.name.empty()||f.name.find('\0')!=std::string::npos){
      fail(f.where,"empty/invalid checked function name");
     }
      generic_context=f.prototype;
      if(!f.prototype&&
         (!f.type_parameters.empty()||!f.constraints.empty()))
       fail(f.where,"source generic metadata on an executable checked function");
     if(f.prototype)generic_metadata(f);
     checked_type(f.result,f.where,generic_context);
    (void)signature(f);
    if(!f.attributes.empty())fail(f.where,"unhandled function attribute in checked HIR");
    if(f.external&&!f.body.empty())fail(f.where,"external function has an executable body");
    scopes.emplace_back();params(f.params);scopes.pop_back();
    if(!functions.emplace(f.name,&f).second)
     fail(f.where,"duplicate checked function: "+f.name);
   }
   generic_context=false;
   const Function* first=nullptr;
   for(const auto& f:unit.functions)if(takes_borrows(f)){
    if(!first)first=&f;
     if(f.external||f.extension_generated||!f.kernel_symbol.empty()||f.name=="main"||!borrowers.emplace(f.name,&f).second)
      ref_error(f.where,"invalid checked borrowed function declaration");
    for(const auto& p:f.params)
     if(p.borrow!=BorrowMode::Value&&
        (p.pattern||p.type==Type::Void||p.type==Type::Json||
         p.type==Type::JsonView||p.type.is_own()||csp::contains(p.type)))
      ref_error(p.where,"borrowed parameter requires a supported named referent");
   }
    if(first&&csp::present(unit))
     ref_error(first->where,"the borrow profile cannot be combined with cooperative concurrency");
    for(const auto& f:unit.functions){
     if(f.prototype||f.external)continue;
      generic_context=false;returns=f.result;loan_tracking=false;
      call_loans.clear();scopes.emplace_back();params(f.params);
    auto after=body(f.body,State{},0,false);scopes.pop_back();
    if(f.result!=Type::Void&&after)
     fail(f.where,"function lacks a complete return path");
   }
  }
};
}
void verify_values(const Unit& unit){Verify(unit).run();}
void verify_values_with_lexical_slots(const Unit& unit,memgraph::detail::LexicalSlotBuilder& plan){
 Verify(unit,&plan).run();
}
}

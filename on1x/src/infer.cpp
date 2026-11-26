#include <on1x/infer.hpp>
#include <on1x/clone.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/primitives.hpp>
#include <on1x/staging.hpp>
#include <on1x/numeric.hpp>
#include <on1x/literals.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/patterns.hpp>
#include <on1x/nominal.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/cliche.hpp>
#include <on1x/inherent.hpp>
#include <deque>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace on1x {
namespace {
bool data_kind(Type::Kind kind){return kind==Type::Record||kind==Type::Sum||kind==Type::Array||kind==Type::Slice;}
bool channel_kind(Type::Kind kind){return kind==Type::Channel||kind==Type::Sender||kind==Type::Receiver;}
bool compound(Type::Kind kind){return kind==Type::Function||kind==Type::Tuple||kind==Type::Code||kind==Type::Own||channel_kind(kind)||data_kind(kind);}
bool has_result(Type::Kind kind){return kind==Type::Function||kind==Type::Code;}
bool numeric_kind(Type::Kind kind){
 return kind==Type::I64||kind==Type::F64||kind==Type::F32||(kind>=Type::U8&&kind<=Type::U64);
}
struct InferenceScan {
 std::size_t nodes=262144;
 void tick(const Location& at,std::size_t depth){
  if(depth>2*max_syntax_depth+8||!nodes--)
   throw Error(at,"type-inference input scan depth/node budget exhausted",
               "ON1X-TYPE-LIMIT");
 }
};
bool holes_in(const Expr& expression,InferenceScan& scan,std::size_t depth);
bool holes_in(const std::vector<Stmt>& body,InferenceScan& scan,std::size_t depth) {
 bool found=false;
 for(const auto& statement:body){
  scan.tick(statement.where,depth);
  if(statement.kind==Stmt::Kind::Defer)found=true;
   if(has_type_holes(statement.declared)||statement.declared.has_foralls()||
      statement.declared.has_names())found=true;
  if(statement.value)found=holes_in(*statement.value,scan,depth+1)||found;
  if(!statement.instances.empty())
   throw Error(statement.where,
               "stale specialization instances before type inference",
               "ON1X-TYPE-FORM");
  found=holes_in(statement.body,scan,depth+1)||found;
  found=holes_in(statement.otherwise,scan,depth+1)||found;
 }
 return found;
}
bool holes_in(const Expr& expression,InferenceScan& scan,std::size_t depth) {
 scan.tick(expression.where,depth);
 bool found=expression.kind==Expr::Kind::ClicheMember||
            expression.kind==Expr::Kind::InherentMember||
            expression.kind==Expr::Kind::Propagate||
            expression.kind==Expr::Kind::SourceCsp||
            (expression.kind>=Expr::Kind::Record&&
             expression.kind<=Expr::Kind::Length)||
             expression.kind==Expr::Kind::PatternMatch||
             expression.kind==Expr::Kind::Loop||
             expression.kind==Expr::Kind::UnsafeBlock||
             expression.kind==Expr::Kind::Borrow;
 if(expression.kind==Expr::Kind::Lambda){
  found=has_type_holes(expression.result)||expression.result.has_foralls()||
        found;
  for(const auto& parameter:expression.parameters)
   found=has_type_holes(parameter.type)||parameter.type.has_foralls()||found;
 }
 if(expression.callee)
  found=holes_in(*expression.callee,scan,depth+1)||found;
 if(expression.syntax_tail)
  found=holes_in(*expression.syntax_tail,scan,depth+1)||found;
 for(const auto& child:expression.children){
  if(!child)throw Error(expression.where,
                        "null expression child before type inference",
                        "ON1X-TYPE-FORM");
  found=holes_in(*child,scan,depth+1)||found;
 }
 for(const auto& proof:expression.proofs){
  if(!proof)throw Error(expression.where,
                        "null universal proof before type inference",
                        "ON1X-TYPE-FORM");
  found=holes_in(*proof,scan,depth+1)||found;
 }
 return holes_in(expression.body,scan,depth+1)||found;
}
bool needs_inference(const Unit& unit) {
 if(unit.forall_elaborated)return false;
 InferenceScan scan;
 bool needed=!unit.nominals.empty()||cliches::present(unit)||
             inherent::present(unit);
 for(const auto& function:unit.functions) {
   scan.tick(function.where,0);
   needed=!function.type_parameters.empty()||!function.constraints.empty()||
          needed;
   const bool body_needs=holes_in(function.body,scan,1);
   needed=has_type_holes(function.result)||function.result.has_foralls()||
          function.result.has_names()||body_needs||needed;
   for(const auto& parameter:function.params)
    needed=has_type_holes(parameter.type)||parameter.type.has_foralls()||
           parameter.type.has_names()||needed;
 }
 return needed;
}

class Inference {
 using Term = std::size_t;
 struct Node {
  Type::Kind kind;
  Term parent;
  unsigned rank = 0;
  std::vector<Term> parameters;
   Term result = 0;
   std::vector<std::string> labels{};
    std::size_t extent=0;
     std::string nominal{};
     std::size_t arguments=0; // Trailing parameters, never representation fields.
     std::size_t senders=0,receivers=0;
 };
 struct Scheme;
  struct Predicate {std::string name;std::vector<Term> arguments;Location where;};
  struct Global {
   Term signature;bool external;Function* definition;
  Scheme* scheme=nullptr;unsigned state=0;std::size_t index=0,low=0;bool on_stack=false;
   Stmt* source=nullptr;std::size_t first_slot=0,last_slot=0;
   std::vector<std::size_t> declaration_slots;
   std::vector<Term> variables;
   std::vector<Predicate> constraints;
 };
 struct Slot { Type* output; Term term; Location where; std::string description; bool generic=false; };
 struct Ground { Type type; std::size_t identity, depth, size; };
   Unit& unit;
   NominalCatalog catalog;
   inherent::Catalog inherent_members;
   std::set<std::string> nominal_names,cliche_names;
 const InferenceLimits& limits;
 std::size_t work = 0;
 std::vector<Node> nodes;
 std::map<Type::Kind,Term> scalars;
 std::map<Term,Type> atoms;
 std::map<Type,Term> atom_terms;
 struct NominalTerm { Term term; std::size_t field_depth; };
 std::map<std::pair<std::string,std::vector<Term>>,NominalTerm> nominal_terms;
 std::size_t rigid_serial=0;
 std::map<std::string,Global> globals;
 struct Binding { Term type; Scheme* scheme=nullptr; std::string runtime_name; };
 using Scopes=std::vector<std::map<std::string,Binding>>;
 struct Scheme { Stmt* declaration; Scopes environment; Term type; std::set<Term> variables; bool global=false; Expr* source=nullptr; Type annotation=Type::Invalid; const std::vector<std::string>* group=nullptr; };
 Scopes scopes;
 std::deque<Scheme> schemes;
  std::vector<Term> weak;
  std::vector<std::pair<Term,Location>> numeric;
 std::set<Term> quantified;
 struct Instance { Stmt* statement; std::size_t use_slot; };
 std::vector<Instance> instances;
 std::vector<std::pair<Expr*,std::string>> renames;
 std::size_t ast_budget,instance_serial=0;
 std::size_t specialization_depth=0;
 std::size_t scheme_metadata=0;
 std::set<std::string> generalizing_globals;
 std::vector<std::string> global_stack;
 std::vector<std::string> component_stack;
 std::deque<std::vector<std::string>> global_groups;
 std::size_t discovery=0;
  std::vector<std::unique_ptr<Stmt>> global_sources;
  struct Obligation {Predicate predicate;std::vector<Predicate> assumptions;Location where;};
  std::vector<Obligation> obligations;
  struct ReceiverCheck {Term target;Location where;};
  std::vector<ReceiverCheck> receiver_checks;
  struct ClicheUse {
   Expr* expression;
   std::string name,member;
   std::vector<Term> arguments,member_arguments;
   Term type;
   std::size_t slot;
   std::vector<Predicate> assumptions;
   bool associated_value=false;
  };
  std::deque<ClicheUse> cliche_uses;
 struct Projection {Expr* expression;Term schema,instance;std::size_t slot;};
 std::vector<Projection> projections;
 struct Pack {Expr* expression;ExprPtr source;Scopes environment;Term schema;std::size_t slot;};
 std::deque<Pack> packs;
 std::vector<Slot> slots;
 Term returns = 0;
  bool saw_return = false;
  struct LoopValue {std::size_t target;Term type;bool bare=false,valued=false;};
  std::vector<LoopValue> loop_values;
  std::vector<std::optional<Ground>> ground;
 std::vector<unsigned char> state;
 std::map<std::vector<std::size_t>,Ground> canonical;
      std::size_t next_identity = Type::Own + 1;
   std::map<std::vector<std::string>,std::size_t> label_identities;
    std::map<std::string,std::size_t> nominal_identities;
    const std::vector<Term>* active_type_variables=nullptr;
    const std::vector<Predicate>* active_constraints=nullptr;

 [[noreturn]] static void fail(const Location& where,const std::string& message) {
   throw Error(where,message,"ON1X-TYPE");
 }
 void tick(const Location& where) {
  if(work>=limits.work)fail(where,"type inference work limit exceeded");
  ++work;
 }
 Term add(Type::Kind kind,std::vector<Term> parameters,Term result,const Location& where) {
  tick(where);
  if(nodes.size()>=limits.nodes)fail(where,"type inference node limit exceeded");
  Term id=nodes.size();nodes.push_back({kind,id,0,std::move(parameters),result});return id;
 }
 Term fresh(const Location& where) { return add(Type::Hole,{},0,where); }
 Term scalar(Type::Kind kind,const Location& where) {
  if(auto found=scalars.find(kind);found!=scalars.end())return found->second;
  auto id=add(kind,{},0,where);scalars.emplace(kind,id);return id;
 }
    Term function(std::vector<Term> parameters,Term result,const Location& where) {
     return add(Type::Function,std::move(parameters),result,where);
    }
    Term channel(Type::Kind kind,Term payload,const Location& at,
                 std::size_t senders=0,std::size_t receivers=0){
     if(!channel_kind(kind)||(kind!=Type::Channel&&(senders||receivers))||
        ((senders==0)!=(receivers==0))||senders>256||receivers>256||
        senders+receivers>256)
      fail(at,"invalid channel type/cardinality");
     auto id=add(kind,{payload},0,at);
     nodes[id].senders=senders;nodes[id].receivers=receivers;
     return id;
    }
   Term owning(std::string nominal,std::vector<Term> arguments,const Location& at){
    if(nominal.empty())fail(at,"own<T> requires a nominal target");
    auto id=add(Type::Own,std::move(arguments),0,at);
    nodes[id].nominal=std::move(nominal);
    nodes[id].arguments=nodes[id].parameters.size();
    return id;
   }
   Term owned_value(Term owner,const Location& at){
    auto root=head(owner,at);
    if(nodes[root].kind!=Type::Own||nodes[root].nominal.empty())
     fail(at,"owning operation requires own<Named>");
     if(shared_opaque_nominal(nodes[root].nominal))shared_opaque_error(at);
     if(ipc_kernel_opaque_nominal(nodes[root].nominal))ipc_kernel_opaque_error(at);
    const auto& declaration=catalog.find(nodes[root].nominal,at);
    if(nodes[root].parameters.size()!=declaration.parameters.size())
     fail(at,"wrong type argument arity for "+declaration.name);
    auto arguments=nodes[root].parameters;
    std::vector<Term> fields;
    for(const auto& field:declaration.fields)
     fields.push_back(annotation(field,at,&arguments,1));
    fields.insert(fields.end(),arguments.begin(),arguments.end());
    return data(declaration.kind,std::move(fields),declaration.labels,0,at,
                declaration.name,arguments.size());
   }
   Term data(Type::Kind kind,std::vector<Term> fields,std::vector<std::string> labels,std::size_t extent,const Location& at,std::string nominal={},std::size_t arguments=0){
    if(fields.size()<arguments)fail(at,"invalid nominal inference shape");
    std::vector<Term> args(fields.end()-arguments,fields.end());fields.resize(fields.size()-arguments);
   if(kind==Type::Record||kind==Type::Sum){
    if(fields.size()!=labels.size()||fields.size()>256||(kind==Type::Sum&&fields.empty()))fail(at,"invalid labeled data shape");
    std::map<std::string,Term> ordered;
    for(std::size_t i=0;i<labels.size();++i){tick(at);if(!ordered.emplace(labels[i],fields[i]).second)fail(at,"duplicate data label: "+labels[i]);}
    labels.clear();fields.clear();for(auto& [label,term]:ordered){labels.push_back(label);fields.push_back(term);}
   }else if(fields.size()!=1||extent>256)fail(at,"invalid array/slice shape");
    fields.insert(fields.end(),args.begin(),args.end());
    auto id=add(kind,std::move(fields),0,at);nodes[id].labels=std::move(labels);nodes[id].extent=extent;nodes[id].nominal=std::move(nominal);nodes[id].arguments=arguments;return id;
   }
     Term annotation(const Type& type,const Location& where,const std::vector<Term>* variables=nullptr,
                     std::size_t depth=0,std::size_t* deepest=nullptr) {
      if(!variables)variables=active_type_variables;
      if(!depth){
       TypeFormOptions options;
       options.bound_variables=variables?variables->size():0;
       options.allow_holes=true;
       options.allow_generics=true;
       options.allow_named=true;
       auto form=check_type_form(type,options);
       if(form.status==TypeFormStatus::Limit)
        throw Error(where,"type annotation form validation limit: "+form.message,
                    "ON1X-TYPE-LIMIT");
       if(!form)
        throw Error(where,"malformed type annotation: "+form.message,
                    "ON1X-TYPE-FORM");
      }
      tick(where);
     if(depth>limits.depth)fail(where,"nominal/type annotation depth limit exceeded");
     if(deepest)*deepest=std::max(*deepest,depth);
     if(type.is_tuple())for(const auto& element:type.elements()){
      if(element==Type::Void)fail(where,"tuple elements cannot be void");
      if(element==Type::Json||element==Type::JsonView)
       fail(where,"tuple elements cannot contain JSON resources or views");
     }
     if(type.is_function()){
      for(const auto& parameter:type.parameters()){
       if(parameter==Type::Void)fail(where,"function parameter type cannot be void");
       if(parameter==Type::Json||parameter==Type::JsonView)
        fail(where,"function types cannot contain resource parameters");
       if(csp::contains(parameter))fail(where,"concurrency handles cannot cross ordinary function signatures");
      }
      if(type.result()==Type::Json||type.result()==Type::JsonView)
       fail(where,"function types cannot contain resource returns");
      if(csp::contains(type.result()))fail(where,"concurrency handles cannot escape through function results");
     }
      if(type==Type::Hole)return fresh(where);
     if(type.kind()==Type::Bound){if(!variables||type.generic_identity()>=variables->size())fail(where,"quantified type variable outside forall/declaration");return (*variables)[type.generic_identity()];}
      if(type.is_own()){
       const auto& target=type.owned();
       if(target.kind()!=Type::Named)fail(where,"own<T> target is not nominal");
       const auto& declaration=catalog.find(target.nominal(),where);
       if(target.arguments().size()!=declaration.parameters.size())
        fail(where,"wrong type argument arity for "+declaration.name);
       std::vector<Term> arguments;
       for(const auto& argument:target.arguments())
        arguments.push_back(annotation(argument,where,variables,depth+1,deepest));
       return owning(declaration.name,std::move(arguments),where);
      }
       if(type.kind()==Type::Named||!type.nominal().empty()){
       if(shared_opaque_nominal(type.nominal()))shared_opaque_error(where);
      if(auto builtin=catalog.regex_builtin(type);builtin!=Type::Invalid)return scalar(builtin.kind(),where);
      const auto& d=catalog.find(type.nominal(),where);
     if(d.parameters.size()!=type.arguments().size())fail(where,"wrong type argument arity for "+d.name);
     std::vector<Term> args,fields;
     for(const auto& a:type.arguments())args.push_back(annotation(a,where,variables,depth+1,deepest));
     // Declarations have no holes: the layout depends only on its identity and
     // these exact argument terms. Independent source holes have different IDs.
     auto key=std::pair{d.name,args};
     if(auto found=nominal_terms.find(key);found!=nominal_terms.end()){
      const auto height=found->second.field_depth;
      if(height>limits.depth-depth)fail(where,"nominal/type annotation depth limit exceeded");
      if(deepest)*deepest=std::max(*deepest,depth+height);
      return found->second.term;
     }
     std::size_t field_depth=depth;
     for(const auto& f:d.fields)fields.push_back(annotation(f,where,&args,depth+1,&field_depth));
     if(deepest)*deepest=std::max(*deepest,field_depth);
     fields.insert(fields.end(),args.begin(),args.end());
     const auto term=data(d.kind,std::move(fields),d.labels,0,where,d.name,args.size());
     nominal_terms.emplace(std::move(key),NominalTerm{term,field_depth-depth});
     return term;
    }
     if(type.is_code())return add(Type::Code,{},annotation(type.code_result(),where,variables,depth+1,deepest),where);
     if(channel_kind(type.kind()))
      return channel(type.kind(),annotation(type.csp_payload(),where,variables,depth+1,deepest),
                     where,type.kind()==Type::Channel?type.sender_cardinality():0,
                     type.kind()==Type::Channel?type.receiver_cardinality():0);
    if(type.is_forall()||type.kind()==Type::Generic){
     auto resolved=type.is_forall()?catalog.resolve(type,where):type;
     if(auto found=atom_terms.find(resolved);found!=atom_terms.end())return found->second;
     auto id=add(type.kind(),{},0,where);atoms.emplace(id,resolved);atom_terms.emplace(std::move(resolved),id);return id;
   }
   if(type.is_tuple()){
    std::vector<Term> elements;for(const auto& element:type.elements())elements.push_back(annotation(element,where,variables,depth+1,deepest));
   return add(Type::Tuple,std::move(elements),0,where);
  }
   if(type.is_data()){
     std::vector<Term> elements;for(const auto& element:type.elements())elements.push_back(annotation(element,where,variables,depth+1,deepest));
    return data(type.kind(),std::move(elements),type.labels(),type.extent(),where);
   }
   if(!type.is_function())return scalar(type.kind(),where);
  std::vector<Term> parameters;
   for(const auto& parameter:type.parameters())parameters.push_back(annotation(parameter,where,variables,depth+1,deepest));
    auto result=annotation(type.result(),where,variables,depth+1,deepest);
   return function(std::move(parameters),result,where);
  }
  Predicate predicate(const ClicheConstraint& constraint,
                      const std::vector<Term>& variables){
   const auto& target=cliches::declaration(unit,constraint.name,constraint.where);
   if(constraint.arguments.size()!=target.type_parameters.size()+1)
    throw Error(constraint.where,"wrong cliche argument arity for "+constraint.name,
                "ON1X-CLICHE-OBLIGATION");
   Predicate result;result.name=constraint.name;result.where=constraint.where;
   for(const auto& argument:constraint.arguments)
    result.arguments.push_back(annotation(argument,constraint.where,&variables));
   return result;
  }
 Term head(Term id,const Location& where) {
  tick(where);auto root=id;
  while(nodes[root].parent!=root){tick(where);root=nodes[root].parent;}
  while(nodes[id].parent!=id){auto next=nodes[id].parent;nodes[id].parent=root;id=next;}
  return root;
 }
 bool occurs(Term variable,Term value,const Location& where) {
  std::vector<Term> pending{value};std::set<Term> seen;
  while(!pending.empty()) {
   auto id=head(pending.back(),where);pending.pop_back();
   if(id==variable)return true;
   if(!seen.insert(id).second)continue;
   if(compound(nodes[id].kind)) {
    if(has_result(nodes[id].kind))pending.push_back(nodes[id].result);
    for(auto parameter:nodes[id].parameters)pending.push_back(parameter);
   }
  }
  return false;
 }
 // Error rendering is independently bounded, even before a graph is grounded.
 std::string describe(Term id,const Location& where,std::size_t& budget,unsigned depth=0) {
  id=head(id,where);
  if(!budget||depth>=16)return "...";
  --budget;
    if(nodes[id].kind==Type::Hole)return "?"+std::to_string(id);
    if(nodes[id].kind==Type::Code)return "code<"+describe(nodes[id].result,where,budget,depth+1)+">";
    if(nodes[id].kind==Type::Own){
     std::string text="own<"+nodes[id].nominal;
     if(!nodes[id].parameters.empty()){
      text+="<";
      for(std::size_t i=0;i<nodes[id].parameters.size();++i){
       if(i)text+=", ";
       text+=describe(nodes[id].parameters[i],where,budget,depth+1);
      }
      text+=">";
     }
     return text+">";
    }
   if(auto atom=atoms.find(id);atom!=atoms.end())return type_name(atom->second);
    bool tuple=nodes[id].kind==Type::Tuple;
    bool aggregate=data_kind(nodes[id].kind);
    if(channel_kind(nodes[id].kind)){
     std::string text=nodes[id].kind==Type::Channel?"channel<":
                      nodes[id].kind==Type::Sender?"sender<":"receiver<";
     text+=describe(nodes[id].parameters.front(),where,budget,depth+1)+">";
     if(nodes[id].kind==Type::Channel&&nodes[id].senders)
      text+="["+std::to_string(nodes[id].senders)+","+
            std::to_string(nodes[id].receivers)+"]";
     return text;
    }
    if(nodes[id].kind!=Type::Function&&!tuple&&!aggregate)return type_name(Type(nodes[id].kind));
   std::string text=aggregate?"data("+std::to_string(nodes[id].extent)+"; ":tuple?"(":"fn(";
  for(std::size_t i=0;i<nodes[id].parameters.size();++i) {
   if(i)text+=", ";
   if(!budget){text+="...";break;}
   text+=describe(nodes[id].parameters[i],where,budget,depth+1);
  }
   return tuple||aggregate?text+")":text+") -> "+describe(nodes[id].result,where,budget,depth+1);
 }
 std::string describe(Term id,const Location& where) {
  std::size_t budget=64;return describe(id,where,budget);
 }
  void unify(Term actual,Term expected,const Location& where) {
   const auto context_actual=actual,context_expected=expected;
   std::vector<std::pair<Term,Term>> pending{{actual,expected}};
   std::set<std::pair<Term,Term>> compared;
  while(!pending.empty()) {
   auto [a,b]=pending.back();pending.pop_back();
   a=head(a,where);b=head(b,where);if(a==b)continue;
   if(nodes[a].kind==Type::Hole||nodes[b].kind==Type::Hole) {
    if(nodes[a].kind!=Type::Hole)std::swap(a,b);
    if(nodes[b].kind==Type::Hole) {
     if(nodes[a].rank>nodes[b].rank)std::swap(a,b);
     nodes[a].parent=b;if(nodes[a].rank==nodes[b].rank)++nodes[b].rank;
    } else {
     if(occurs(a,b,where))fail(where,"infinite type (occurs check failed)");
     nodes[a].parent=b;
    }
    continue;
   }
     if(nodes[a].kind!=nodes[b].kind){
      auto outer_expected=describe(context_expected,where),outer_actual=describe(context_actual,where);
      auto inner_expected=describe(b,where),inner_actual=describe(a,where);
      auto message="expected "+outer_expected+", got "+outer_actual;
      if(outer_expected!=inner_expected||outer_actual!=inner_actual)
       message+="; nested mismatch: expected "+inner_expected+", got "+inner_actual;
      fail(where,message);
     }
     if(atoms.contains(a)||atoms.contains(b)){
      if(!atoms.contains(a)||!atoms.contains(b)||atoms.at(a)!=atoms.at(b)){
       auto outer_expected=describe(context_expected,where),outer_actual=describe(context_actual,where);
       auto inner_expected=describe(b,where),inner_actual=describe(a,where);
       auto message="incompatible quantified types: expected "+outer_expected+", got "+outer_actual;
       if(outer_expected!=inner_expected||outer_actual!=inner_actual)
        message+="; nested mismatch: expected "+inner_expected+", got "+inner_actual;
       fail(where,message);
      }
     continue;
    }
      if(!compound(nodes[a].kind))continue;
      if(nodes[a].kind==Type::Channel){
       auto& as=nodes[a].senders;auto& ar=nodes[a].receivers;
       auto& bs=nodes[b].senders;auto& br=nodes[b].receivers;
       if(!as&&!ar){as=bs;ar=br;}
       else if(!bs&&!br){bs=as;br=ar;}
       else if(as!=bs||ar!=br)
        fail(where,"incompatible channel endpoint cardinalities: expected "+
                   describe(b,where)+", got "+describe(a,where));
      }
     if(nodes[a].nominal!=nodes[b].nominal||nodes[a].arguments!=nodes[b].arguments)fail(where,"incompatible nominal types: expected "+nodes[b].nominal+", got "+nodes[a].nominal);
    if(data_kind(nodes[a].kind)&&(nodes[a].labels!=nodes[b].labels||nodes[a].extent!=nodes[b].extent))
     fail(where,"incompatible data labels or array/slice extent");
   if(nodes[a].parameters.size()!=nodes[b].parameters.size())
    fail(where,std::string("incompatible ")+(nodes[a].kind==Type::Tuple?"tuple":"function")+" arity: expected "+describe(b,where)+", got "+describe(a,where));
   auto pair=std::minmax(a,b);
   if(!compared.emplace(pair.first,pair.second).second)continue;
   // Only variables acquire parents. Merging composite roots could introduce a
   // cycle before their children have passed the occurs check.
   if(has_result(nodes[a].kind))pending.emplace_back(nodes[a].result,nodes[b].result);
   for(std::size_t i=nodes[a].parameters.size();i>0;--i)
    pending.emplace_back(nodes[a].parameters[i-1],nodes[b].parameters[i-1]);
  }
 }
 void define(const std::string& name,Term type,const Location& where) {
  if(!scopes.back().emplace(name,Binding{type,nullptr,{}}).second)fail(where,"duplicate local: "+name);
 }
 std::set<Term> free(Term type,const Location& at){
  std::set<Term> result,seen;std::vector<Term> pending{type};
  while(!pending.empty()){
   auto id=head(pending.back(),at);pending.pop_back();if(!seen.insert(id).second)continue;
   if(nodes[id].kind==Type::Hole)result.insert(id);
   else if(compound(nodes[id].kind)){
    if(has_result(nodes[id].kind))pending.push_back(nodes[id].result);
    for(auto child:nodes[id].parameters)pending.push_back(child);
   }
  }
  return result;
 }
  bool same_term(Term left,Term right,const Location& at){
   std::vector<std::pair<Term,Term>> pending{{left,right}};
   std::set<std::pair<Term,Term>> seen;
   while(!pending.empty()){
    auto [a,b]=pending.back();pending.pop_back();a=head(a,at);b=head(b,at);
    if(a==b)continue;
    if(!seen.emplace(a,b).second)continue;
    if(nodes[a].kind!=nodes[b].kind||nodes[a].nominal!=nodes[b].nominal||
       nodes[a].arguments!=nodes[b].arguments||nodes[a].labels!=nodes[b].labels||
       nodes[a].extent!=nodes[b].extent||nodes[a].senders!=nodes[b].senders||
       nodes[a].receivers!=nodes[b].receivers||
       nodes[a].parameters.size()!=nodes[b].parameters.size())
     return false;
    if(auto aa=atoms.find(a);aa!=atoms.end()){
     auto bb=atoms.find(b);if(bb==atoms.end()||aa->second!=bb->second)return false;
    }else if(atoms.contains(b))return false;
    if(has_result(nodes[a].kind))pending.emplace_back(nodes[a].result,nodes[b].result);
    for(std::size_t i=0;i<nodes[a].parameters.size();++i)
     pending.emplace_back(nodes[a].parameters[i],nodes[b].parameters[i]);
   }
   return true;
  }
  bool concrete(Term term,const Location& at){
   std::vector<Term> pending{term};std::set<Term> seen;
   while(!pending.empty()){
    auto id=head(pending.back(),at);pending.pop_back();if(!seen.insert(id).second)continue;
    if(nodes[id].kind==Type::Hole)return false;
    if(auto atom=atoms.find(id);atom!=atoms.end()&&atom->second.has_generics())return false;
    if(has_result(nodes[id].kind))pending.push_back(nodes[id].result);
    for(auto child:nodes[id].parameters)pending.push_back(child);
   }
   return true;
  }
  bool assumed(const Predicate& predicate,const std::vector<Predicate>* assumptions,
               const Location& at){
   if(!assumptions)return false;
   for(const auto& candidate:*assumptions){
    if(candidate.name!=predicate.name||candidate.arguments.size()!=predicate.arguments.size())continue;
    bool equal=true;
    for(std::size_t i=0;i<predicate.arguments.size();++i)
     if(!same_term(predicate.arguments[i],candidate.arguments[i],at)){equal=false;break;}
    if(equal)return true;
   }
   return false;
  }
  std::vector<Type> concrete_types(const std::vector<Term>& terms,const Location& at){
   reset_ground();std::vector<Type> result;result.reserve(terms.size());
   for(auto term:terms)result.push_back(resolve(term,at,"cliche obligation").type);
   return result;
  }
  void discharge(const Predicate& predicate,const std::vector<Predicate>* assumptions,
                 const Location& at){
   bool closed=true;for(auto argument:predicate.arguments)closed=closed&&concrete(argument,at);
   if(!closed){
    if(!assumed(predicate,assumptions,at))
     fail(at,"unsatisfied generic cliche bound "+predicate.name+
          "; add it to the function's where clause");
    return;
   }
   ClicheConstraint constraint;constraint.where=predicate.where;constraint.name=predicate.name;
   constraint.arguments=concrete_types(predicate.arguments,at);
   cliches::require(unit,constraint,at);
  }
  void require_later(Predicate predicate,const Location& at){
   obligations.push_back({std::move(predicate),
                          active_constraints?*active_constraints:std::vector<Predicate>{},at});
  }
 static bool value(const Expr& e){
  if(e.kind==Expr::Kind::Quote)return e.children.size()==1&&e.children[0]->kind==Expr::Kind::Call&&e.children[0]->children.empty();
  if(e.kind==Expr::Kind::Lambda||e.kind==Expr::Kind::Variable||e.kind==Expr::Kind::Integer||e.kind==Expr::Kind::Float||e.kind==Expr::Kind::Boolean||e.kind==Expr::Kind::String||e.kind==Expr::Kind::Character)return true;
   if(e.kind!=Expr::Kind::Tuple&&e.kind!=Expr::Kind::Record&&e.kind!=Expr::Kind::Array&&e.kind!=Expr::Kind::Variant)return false;
  for(const auto& child:e.children)if(!value(*child))return false;
  return true;
 }
 std::set<Term> generalizable(Term type,const Location& at){
  auto variables=free(type,at);
  auto exclude=[&](Term term){for(auto v:free(term,at))variables.erase(v);};
  exclude(returns);
   for(const auto& [name,global]:globals){
     if(generalizing_globals.contains(name))continue;
    auto environment=free(global.signature,at);
    if(global.scheme)for(auto variable:global.scheme->variables)environment.erase(head(variable,at));
    for(auto variable:environment)variables.erase(variable);
   }
  for(const auto& scope:scopes)for(const auto& [name,binding]:scope){
   (void)name;auto environment=free(binding.type,at);
   if(binding.scheme)for(auto variable:binding.scheme->variables)environment.erase(head(variable,at));
   for(auto variable:environment)variables.erase(variable);
  }
  for(auto term:weak)exclude(term);
  return variables;
 }
 void account_environment(const Scopes& environment,const Location& at){
  for(const auto& scope:environment)for(const auto& [name,binding]:scope){
   (void)binding;auto bytes=name.size()+64;
   if(bytes>8*1024*1024-scheme_metadata)fail(at,"let scheme environment limit exceeded");
   scheme_metadata+=bytes;
  }
 }
 void generalize(Stmt& s,Term type,std::size_t first){
  if(s.mutable_binding||s.pattern||!value(*s.value))return;
  auto variables=generalizable(type,s.where);
  if(variables.empty())return;
  if(schemes.size()>=512)fail(s.where,"let scheme limit exceeded");
  account_environment(scopes,s.where);
  schemes.push_back({&s,scopes,type,variables,false,s.value.get(),s.declared});s.kind=Stmt::Kind::Template;
  quantified.insert(variables.begin(),variables.end());
  for(std::size_t i=first;i<slots.size();++i)slots[i].generic=true;
  // The environment snapshot excludes this nonrecursive binding.
  define(s.name,type,s.where);scopes.back().at(s.name).scheme=&schemes.back();
 }
 struct Selection { Expr* expression;const Scopes* environment;Stmt* placement; };
 Selection select_tuple(Selection selected,const Location& at){
  for(unsigned depth=0;depth<64;++depth){
   if(selected.expression->kind!=Expr::Kind::Variable)return selected;
   const Binding* found=nullptr;
   for(auto it=selected.environment->rbegin();it!=selected.environment->rend();++it){
    auto hit=it->find(selected.expression->text);if(hit!=it->end()){found=&hit->second;break;}
   }
   if(!found||!found->scheme)return selected;
   auto& scheme=*found->scheme;
   selected={scheme.source,&scheme.environment,scheme.declaration};
  }
  fail(at,"destructuring specialization nesting limit exceeded");
 }
 bool generalize_pattern(Stmt& statement,Term type,std::size_t first){
  if(!statement.pattern||statement.mutable_binding||!value(*statement.value))return false;
  scopes.emplace_back();bind_pattern(*statement.pattern,type);scopes.pop_back();
  struct Leaf {const Pattern* pattern;Term type;std::set<Term> variables;Selection source;};
  std::vector<Leaf> leaves;
  auto gather=[&](auto&& self,const Pattern& p,Term term)->void{
   term=head(term,p.where);
   if(!p.is_tuple()){leaves.push_back({&p,term,generalizable(term,p.where),{nullptr,nullptr,nullptr}});return;}
   if(nodes[term].kind!=Type::Tuple||nodes[term].parameters.size()!=p.elements.size())return;
   for(std::size_t i=0;i<p.elements.size();++i)self(self,p.elements[i],nodes[term].parameters[i]);
  };
  gather(gather,*statement.pattern,type);
  bool any=false;for(const auto& leaf:leaves)any=any||!leaf.variables.empty();
  if(!any)return false;
  account_environment(scopes,statement.where);auto environment=scopes;
  std::size_t cursor=0;
  auto choose=[&](auto&& self,const Pattern& p,Selection selected)->bool{
   if(!p.is_tuple()){leaves.at(cursor++).source=selected;return true;}
   selected=select_tuple(selected,p.where);
   if(selected.expression->kind!=Expr::Kind::Tuple||selected.expression->children.size()!=p.elements.size())return false;
   for(std::size_t i=0;i<p.elements.size();++i)
    if(!self(self,p.elements[i],{selected.expression->children[i].get(),selected.environment,selected.placement}))return false;
   return true;
  };
  if(!choose(choose,*statement.pattern,{statement.value.get(),&environment,&statement}))return false;
  for(const auto& leaf:leaves)define(leaf.pattern->name,leaf.type,leaf.pattern->where);
  statement.kind=Stmt::Kind::Template;
  for(std::size_t i=first;i<slots.size();++i)slots[i].generic=true;
  for(auto& leaf:leaves){
   auto& binding=scopes.back().at(leaf.pattern->name);
   if(!leaf.variables.empty()){
    if(schemes.size()>=512)fail(leaf.pattern->where,"let scheme limit exceeded");
    account_environment(*leaf.source.environment,leaf.pattern->where);
    schemes.push_back({leaf.source.placement,*leaf.source.environment,leaf.type,leaf.variables,false,leaf.source.expression});
    binding.scheme=&schemes.back();quantified.insert(leaf.variables.begin(),leaf.variables.end());
   }else{
    if(instance_serial>=limits.specializations)fail(leaf.pattern->where,"let specialization limit exceeded");
    auto instance=std::make_unique<Stmt>();instance->kind=Stmt::Kind::Let;instance->where=leaf.pattern->where;
    instance->name="@pattern_"+std::to_string(++instance_serial);binding.runtime_name=instance->name;
    instance->value=clone_expression(*leaf.source.expression,ast_budget);source_expression(*instance->value);
    auto saved=std::move(scopes);scopes=*leaf.source.environment;
    auto actual=expression(*instance->value);unify(actual,leaf.type,leaf.pattern->where);
    scopes=std::move(saved);
    instances.push_back({instance.get(),slots.size()-1});
    leaf.source.placement->instances.push_back(std::move(instance));
   }
  }
  return true;
 }
 void share_weak(Term original,Term instance,const Scheme& scheme,const Location& at){
  std::vector<std::pair<Term,Term>> pending{{original,instance}};std::set<std::pair<Term,Term>> seen;
  while(!pending.empty()){
   auto [a,b]=pending.back();pending.pop_back();a=head(a,at);b=head(b,at);
   if(!seen.emplace(a,b).second)continue;
    if(nodes[a].kind==Type::Hole&&scheme.variables.contains(a))continue;
     if(compound(nodes[a].kind)&&nodes[a].kind==nodes[b].kind){
      if(nodes[a].parameters.size()!=nodes[b].parameters.size()||
         nodes[a].nominal!=nodes[b].nominal||nodes[a].arguments!=nodes[b].arguments||
         nodes[a].senders!=nodes[b].senders||nodes[a].receivers!=nodes[b].receivers){
       unify(b,a,at);continue;
      }
    if(has_result(nodes[a].kind))pending.emplace_back(nodes[a].result,nodes[b].result);
    for(std::size_t i=0;i<nodes[a].parameters.size();++i)pending.emplace_back(nodes[a].parameters[i],nodes[b].parameters[i]);
   }else unify(b,a,at);
  }
 }
 Term copy_signature(Term term,const std::set<Term>& variables,std::map<Term,Term>& copies,const Location& at,std::size_t depth=0){
  if(depth>limits.depth)fail(at,"specialization signature depth limit exceeded");
  term=head(term,at);
  if(auto found=copies.find(term);found!=copies.end())return found->second;
  if(variables.contains(term)){auto result=fresh(at);copies.emplace(term,result);return result;}
  if(!compound(nodes[term].kind))return term;
    auto kind=nodes[term].kind;auto children=nodes[term].parameters;auto result=nodes[term].result;
     auto labels=nodes[term].labels;auto extent=nodes[term].extent;auto nominal=nodes[term].nominal;auto arguments=nodes[term].arguments;
     auto senders=nodes[term].senders,receivers=nodes[term].receivers;
   bool changed=false;
   for(auto& child:children){
    const auto specialized=copy_signature(child,variables,copies,at,depth+1);
    changed|=specialized!=child;
    child=specialized;
   }
   if(has_result(kind)){
    const auto specialized=copy_signature(result,variables,copies,at,depth+1);
    changed|=specialized!=result;
    result=specialized;
   }
   // Compound nodes are immutable; unification only links variable roots.
   // A subtree unaffected by quantified substitution can be reused even if it
   // refers to weak (non-quantified) variables, which are already shared.
   if(!changed){copies.emplace(term,term);return term;}
    Term id;
    if(data_kind(kind))id=data(kind,std::move(children),std::move(labels),extent,at,std::move(nominal),arguments);
     else if(kind==Type::Own)id=owning(std::move(nominal),std::move(children),at);
     else if(channel_kind(kind)){
      if(children.size()!=1)fail(at,"invalid copied channel signature");
      id=channel(kind,children.front(),at,senders,receivers);
     }
     else id=add(kind,std::move(children),result,at);
    copies.emplace(term,id);return id;
 }
  void bind_pattern(const Pattern& pattern,Term value){
   tick(pattern.where);
   if(!pattern.is_tuple()){define(pattern.name,value,pattern.where);return;}
   std::vector<Term> elements;
   value=head(value,pattern.where);
   if(nodes[value].kind==Type::Hole){
    for(std::size_t i=0;i<pattern.elements.size();++i)elements.push_back(fresh(pattern.where));
    auto shape=add(Type::Tuple,elements,0,pattern.where);unify(value,shape,pattern.where);
   }else{
    if(nodes[value].kind!=Type::Tuple)
     fail(pattern.where,"tuple binding pattern requires a tuple; expected tuple shape, got "+describe(value,pattern.where));
    if(nodes[value].parameters.size()!=pattern.elements.size())
     fail(pattern.where,"tuple binding pattern has wrong arity (tuple arity mismatch)");
    elements=nodes[value].parameters;
   }
   for(std::size_t i=0;i<pattern.elements.size();++i)bind_pattern(pattern.elements[i],elements[i]);
  }
 std::optional<Binding> local(const std::string& name)const {
  for(auto scope=scopes.rbegin();scope!=scopes.rend();++scope)
   if(auto found=scope->find(name);found!=scope->end())return found->second;
  return {};
 }
 void ensure_global(const std::string& name){
  // Tarjan discovery follows resolved global references, not token spellings:
  // a local shadow never creates a global dependency. Pending component members
  // keep monotypes until their root has finished checking the entire component.
  auto& global=globals.at(name);
  if(global.external)return;
  if(global.state){
   if(global.on_stack&&!global_stack.empty()){
    auto& caller=globals.at(global_stack.back());caller.low=std::min(caller.low,global.index);
   }
   return;
  }
   if(global_stack.size()>=128)fail(global.definition->where,"global inference dependency nesting limit exceeded");
  global.state=1;global.index=global.low=++discovery;global.on_stack=true;
  component_stack.push_back(name);global_stack.push_back(name);auto& f=*global.definition;
  auto source=std::make_unique<Stmt>();source->kind=Stmt::Kind::Let;source->name=f.name;source->where=f.where;
  auto lambda=std::make_unique<Expr>();lambda->kind=Expr::Kind::Lambda;lambda->where=f.where;
   lambda->parameters=f.params;lambda->result=f.result;lambda->procedure=f.procedure;
  for(const auto& s:f.body)lambda->body.push_back(clone_statement(s,ast_budget));
  source->value=std::move(lambda);
  global.source=source.get();global_sources.push_back(std::move(source));
    auto saved_scopes=std::move(scopes);auto saved_returns=returns;
    bool saved_saw=saw_return;auto saved_loops=std::move(loop_values);
    auto* saved_type_variables=active_type_variables;
    auto* saved_constraints=active_constraints;
    active_type_variables=&global.variables;active_constraints=&global.constraints;
    loop_values.clear();
  scopes.emplace_back();auto signature=global.signature;
  for(std::size_t i=0;i<f.params.size();++i){
   const auto& p=f.params[i];auto type=nodes[signature].parameters[i];
   if(p.pattern)bind_pattern(*p.pattern,type);else define(p.name,type,p.where);
  }
   returns=nodes[signature].result;saw_return=false;global.first_slot=slots.size();block(f.body,false);scopes.pop_back();global.last_slot=slots.size();
    if(!saw_return&&nodes[head(returns,f.where)].kind==Type::Hole)
     unify(returns,scalar(Type::Void,f.where),f.where);
  if(global.low==global.index){
   std::vector<std::string> group;
   do{group.push_back(component_stack.back());component_stack.pop_back();globals.at(group.back()).on_stack=false;}while(group.back()!=name);
   std::sort(group.begin(),group.end());
   // A sibling can still constrain a member's binders after its body has
   // finished. Validate the entire component before publishing any schemes.
   for(const auto& member:group){
    const auto& item=globals.at(member);
    std::set<Term> explicit_variables;
    for(std::size_t i=0;i<item.variables.size();++i){
     const auto& parameter=item.definition->type_parameters[i];
     auto variable=head(item.variables[i],parameter.where);
     if(nodes[variable].kind!=Type::Hole)
      fail(parameter.where,"generic function body specializes type parameter "+parameter.name);
     if(!explicit_variables.insert(variable).second)
      fail(parameter.where,"generic function body equates distinct type parameters");
    }
   }
   if(std::find(group.begin(),group.end(),"main")==group.end()){
    returns=scalar(Type::Void,f.where);generalizing_globals={group.begin(),group.end()};
     bool any=false;std::map<std::string,std::set<Term>> variables;
     for(const auto& member:group){
      auto& item=globals.at(member);
      variables[member]=generalizable(item.signature,item.definition->where);
      for(auto variable:item.variables)
       variables[member].insert(head(variable,item.definition->where));
      any=any||!variables[member].empty();
     }
    if(any){
     global_groups.push_back(std::move(group));const auto& members=global_groups.back();
     for(const auto& member:members){
      auto& item=globals.at(member);
      if(schemes.size()>=512)fail(item.definition->where,"global scheme limit exceeded");
      schemes.push_back({item.source,{},item.signature,variables.at(member),true,item.source->value.get(),Type::Invalid,&members});
      item.scheme=&schemes.back();item.definition->prototype=true;
      quantified.insert(item.scheme->variables.begin(),item.scheme->variables.end());
      for(std::size_t i=item.first_slot;i<item.last_slot;++i)slots[i].generic=true;
      for(auto index:item.declaration_slots)slots[index].generic=true;
     }
    }
    generalizing_globals.clear();
   }
  }
    scopes=std::move(saved_scopes);returns=saved_returns;saw_return=saved_saw;
    loop_values=std::move(saved_loops);active_type_variables=saved_type_variables;
    active_constraints=saved_constraints;
  global.state=2;global_stack.pop_back();
  if(!global_stack.empty()){
   auto& caller=globals.at(global_stack.back());caller.low=std::min(caller.low,global.low);
  }
 }
  Term instantiate_global(Scheme& scheme,Expr& use,
                          const std::vector<Term>* forced_variables=nullptr){
  // Allocate every sibling signature before inferring any body. Recursive reads
  // resolve through these monomorphic overrides, not fresh scheme instances.
  auto saved=std::move(scopes);scopes.emplace_back();
   struct Member {
    Global* global;std::unique_ptr<Stmt> instance;Term signature;
    std::vector<Term> type_variables;
    std::vector<Predicate> constraints;
   };
   std::vector<Member> members;
   std::set<Term> variables;std::map<Term,Term> copies;
   for(const auto& name:*scheme.group){
    const auto& global=globals.at(name);
    const auto& member=*global.scheme;
    variables.insert(member.variables.begin(),member.variables.end());
    for(auto variable:global.variables)variables.insert(head(variable,use.where));
   }
   if(forced_variables){
    auto target=std::find_if(scheme.group->begin(),scheme.group->end(),
     [&](const std::string& name){return globals.at(name).scheme==&scheme;});
    if(target==scheme.group->end())fail(use.where,"invalid forced global specialization");
    const auto& declaration=globals.at(*target);
    if(forced_variables->size()!=declaration.variables.size())
     fail(use.where,"wrong explicit type-argument count for generated specialization");
    for(std::size_t i=0;i<forced_variables->size();++i){
     auto key=head(declaration.variables[i],use.where);
     auto [found,inserted]=copies.emplace(key,(*forced_variables)[i]);
     if(!inserted&&!same_term(found->second,(*forced_variables)[i],use.where))
      fail(use.where,"conflicting explicit type arguments for generated specialization");
    }
   }
   for(const auto& name:*scheme.group){
    auto& global=globals.at(name);
   if(instance_serial>=limits.specializations)fail(use.where,"global specialization limit exceeded");
   auto instance=std::make_unique<Stmt>();instance->kind=Stmt::Kind::Let;instance->where=use.where;
   instance->name="@let_"+std::to_string(++instance_serial);
    instance->value=clone_expression(*global.scheme->source,ast_budget);source_expression(*instance->value);
    auto signature=copy_signature(global.signature,variables,copies,use.where);
    define(name,signature,use.where);scopes.back().at(name).runtime_name=instance->name;
    std::vector<Term> type_variables;
    for(auto variable:global.variables)
     type_variables.push_back(copy_signature(variable,variables,copies,use.where));
    std::vector<Predicate> constraints;
    for(const auto& source:global.constraints){
     Predicate constraint;constraint.name=source.name;constraint.where=source.where;
     for(auto argument:source.arguments)
      constraint.arguments.push_back(copy_signature(argument,variables,copies,use.where));
     constraints.push_back(std::move(constraint));
    }
    members.push_back({&global,std::move(instance),signature,
                       std::move(type_variables),std::move(constraints)});
   }
   for(auto& member:members){
    auto* saved_type_variables=active_type_variables;
    auto* saved_constraints=active_constraints;
    active_type_variables=&member.type_variables;
    active_constraints=&member.constraints;
    auto type=expression(*member.instance->value,member.signature);unify(type,member.signature,use.where);
    share_weak(member.global->signature,type,*member.global->scheme,use.where);
    active_type_variables=saved_type_variables;active_constraints=saved_constraints;
   }
   Term result=0;auto use_slot=slots.size();
   for(auto& member:members){
    if(member.global->scheme==&scheme){
     result=member.signature;renames.emplace_back(&use,member.instance->name);
     for(const auto& constraint:member.constraints)require_later(constraint,use.where);
    }
   instances.push_back({member.instance.get(),use_slot});
   member.global->source->instances.push_back(std::move(member.instance));
  }
  scopes=std::move(saved);return result;
 }
  Term name(Expr& expression,bool call_position=false,
            const std::vector<Term>* forced_variables=nullptr) {
  Term type;
  auto found=local(expression.text);
  if(!found){
   if(auto global=globals.find(expression.text);global!=globals.end()&&!global->second.external){
    ensure_global(expression.text);
    found=Binding{global->second.signature,global->second.scheme,{}};
   }
  }
  if(found){
   type=found->type;
   if(found->scheme){
      if(++specialization_depth>64)fail(expression.where,"let specialization nesting limit exceeded");
      if(found->scheme->global){
       type=instantiate_global(*found->scheme,expression,forced_variables);--specialization_depth;
       slots.push_back({&expression.type,type,expression.where,"global instantiation"});return type;
      }
      if(forced_variables)
       fail(expression.where,"explicit generated specialization targeted a local scheme");
    if(instance_serial>=limits.specializations)fail(expression.where,"let specialization limit exceeded");
    auto& scheme=*found->scheme;auto instance=std::make_unique<Stmt>();
    instance->kind=Stmt::Kind::Let;instance->where=expression.where;
    instance->name="@let_"+std::to_string(++instance_serial);
     instance->value=clone_expression(*scheme.source,ast_budget);
    source_expression(*instance->value);
     auto saved=std::move(scopes);scopes=scheme.environment;
     std::map<Term,Term> copies;
     type=expected(*instance->value,copy_signature(scheme.type,scheme.variables,copies,expression.where));
    share_weak(scheme.type,type,scheme,expression.where);
     if(scheme.annotation!=Type::Invalid){
      instance->declared=scheme.annotation;
     auto declared=annotation(instance->declared,expression.where);unify(type,declared,expression.where);
     slots.push_back({&instance->declared,declared,expression.where,"specialization annotation"});
    }
    scopes=std::move(saved);
    renames.emplace_back(&expression,instance->name);
    instances.push_back({instance.get(),slots.size()});
     scheme.declaration->instances.push_back(std::move(instance));
     --specialization_depth;
    }
     else{
      if(forced_variables&&!forced_variables->empty())
       fail(expression.where,"generated specialization target is not generic");
      if(!found->runtime_name.empty())renames.emplace_back(&expression,found->runtime_name);
     }
   }
   else if(auto found=globals.find(expression.text);found!=globals.end()) {
    if(forced_variables&&!forced_variables->empty())
     fail(expression.where,"generated specialization target is not generic");
    if(found->second.external&&!call_position&&
       !load_time_kernel_symbol(found->second.definition->kernel_symbol))
     fail(expression.where,"raw extern functions cannot be used as values; write an On1x wrapper");
   type=found->second.signature;
  } else {
   if(is_intrinsic_name(expression.text))
    fail(expression.where,"intrinsic is not a first-class value; write an On1x wrapper: "+expression.text);
   fail(expression.where,(call_position?"unknown function: ":"undefined variable: ")+expression.text);
  }
   slots.push_back({&expression.type,type,expression.where,"expression type"});return type;
  }
  void elaborate_cliche_uses(){
   for(std::size_t index=0;index<cliche_uses.size();++index){
    if(index>=limits.expanded_ast)
     throw Error(cliche_uses[index].expression->where,
                 "cliche elaboration work budget exhausted","ON1X-CLICHE-LIMIT");
    auto use=cliche_uses[index];
    Predicate obligation{use.name,use.arguments,use.expression->where};
    bool closed=true;
    for(auto argument:use.arguments)closed=closed&&concrete(argument,use.expression->where);
    for(auto argument:use.member_arguments)
     closed=closed&&concrete(argument,use.expression->where);
    if(!closed){
     discharge(obligation,&use.assumptions,use.expression->where);
     if(use.slot>=slots.size()||!slots[use.slot].generic)
      fail(use.expression->where,
           "cannot resolve a concrete cliche specialization; add a type annotation");
     use.expression->prototype=true;
     continue;
    }

    auto arguments=concrete_types(use.arguments,use.expression->where);
    auto member_arguments=concrete_types(use.member_arguments,use.expression->where);
    auto selected=cliches::resolve(unit,use.name,arguments,use.member,
                                   member_arguments,use.expression->where);
    if(!selected.member||selected.member->runtime_name.empty())
     throw Error(use.expression->where,
                 "selected cliche member has no static implementation name",
                 "ON1X-CLICHE-UNSUPPORTED");
    std::vector<Term> forced;
    forced.reserve(selected.implementation_arguments.size()+member_arguments.size());
    for(const auto& argument:selected.implementation_arguments)
     forced.push_back(annotation(argument,use.expression->where));
    for(const auto& argument:member_arguments)
     forced.push_back(annotation(argument,use.expression->where));

    auto variable=[&](Expr& expression){
     expression.kind=Expr::Kind::Variable;
     expression.reference=Expr::Reference::Unresolved;
     expression.text=selected.member->runtime_name;
     expression.cliche_member.clear();
     expression.cliche_arguments.clear();
     expression.member_type_arguments.clear();
     expression.children.clear();
     expression.callee.reset();
     expression.prototype=false;
    };
    if(!use.associated_value){
     variable(*use.expression);
     auto callable=name(*use.expression,false,&forced);
     unify(callable,use.type,use.expression->where);
     continue;
    }

    auto callee=std::make_unique<Expr>();
    callee->where=use.expression->where;
    callee->kind=Expr::Kind::Variable;
    callee->text=selected.member->runtime_name;
    auto callable=name(*callee,true,&forced);
    auto root=head(callable,use.expression->where);
    if(nodes[root].kind!=Type::Function||!nodes[root].parameters.empty())
     throw Error(use.expression->where,
                 "associated cliche value did not elaborate to a nullary function",
                 "ON1X-CLICHE-SIGNATURE");
    unify(nodes[root].result,use.type,use.expression->where);
    auto& expression=*use.expression;
    expression.kind=Expr::Kind::Call;
    expression.reference=Expr::Reference::Unresolved;
    expression.text.clear();
    expression.cliche_member.clear();
    expression.cliche_arguments.clear();
    expression.member_type_arguments.clear();
    expression.children.clear();
    expression.callee=std::move(callee);
    expression.prototype=false;
   }
  }
   Term builtin(Expr& expression,const std::vector<Term>& args) {
    const auto& n=expression.callee->text;const auto& at=expression.where;
     auto arity=[&](std::size_t count){if(args.size()!=count)fail(at,"wrong arity for "+n);};
     if(const auto* op=shared_intrinsic(n)){
      if(args.size()!=op->arity)
       shared_error(at,"wrong arity for "+std::string(op->spelling));
      const auto nominal=shared_nominal(op->kind);
      if(op->op==SharedOp::New){
       (void)catalog.find(nominal,at);
       return owning(nominal,{args[0]},at);
      }
      auto payload=fresh(at);
      auto owner=owning(nominal,{payload},at);
      unify(args[0],owner,at);
      if(op->op==SharedOp::Clone)return owner;
      if(op->op==SharedOp::Read)return payload;
      if(op->op==SharedOp::TryBorrow||op->op==SharedOp::TryBorrowMut){
       (void)catalog.find(shared_borrow_result,at);
       auto guard=owning(shared_nominal(shared_guard(op->op)),{payload},at);
       auto empty=data(Type::Record,{},{},0,at);
       return data(Type::Sum,{empty,guard,guard},{"Busy","Granted"},0,at,
                   shared_borrow_result,1);
      }
      if(op->op==SharedOp::Replace){
       unify(args[1],payload,at);
       return payload;
      }
      unify(args[1],scalar(Type::U8,at),at);
      unify(args[2],scalar(Type::I64,at),at);
      return scalar(Type::I64,at);
     }
    if(n=="box"){
     arity(1);auto value=head(args[0],at);
     if(!data_kind(nodes[value].kind)||nodes[value].nominal.empty())
      fail(at,"box(value) requires a known nominal record or enum value");
     if(nodes[value].arguments>nodes[value].parameters.size())
      fail(at,"invalid nominal inference shape for box");
     std::vector<Term> arguments(nodes[value].parameters.end()-nodes[value].arguments,
                                 nodes[value].parameters.end());
     return owning(nodes[value].nominal,std::move(arguments),at);
    }
    if(n=="move"){arity(1);return args[0];}
    if(n=="take"){arity(1);return owned_value(args[0],at);}
    if(n=="drop"){arity(1);return scalar(Type::Void,at);}
    if(n=="ref_replace"){arity(2);unify(args[0],args[1],at);return args[0];}
    if(auto primitive=data_primitive(n)){
    arity(primitive->parameters().size());for(std::size_t i=0;i<args.size();++i)unify(args[i],annotation(primitive->parameters()[i],at),at);
    return annotation(primitive->result(),at);
   }
  if(n=="println"){arity(1);weak.push_back(args[0]);return scalar(Type::Void,at);}
  if(n=="format"){
   if(args.empty()||args.size()>literals::max_format_parts+1)fail(at,"format requires a pattern and at most 256 arguments");
   unify(args[0],scalar(Type::String,at),at);
   for(std::size_t i=1;i<args.size();++i)weak.push_back(args[i]);
   return scalar(Type::String,at);
  }
  if(n=="assert"){arity(1);unify(args[0],scalar(Type::Bool,at),at);return scalar(Type::Void,at);}
  if(n=="json_parse_or_die") {
   arity(1);unify(args[0],scalar(Type::String,at),at);return scalar(Type::Json,at);
  }
  if(n=="json_get_or_die") {
   if(!args.empty())weak.push_back(args[0]);
   arity(2);unify(args[1],scalar(Type::String,at),at);return scalar(Type::JsonView,at);
  }
  if(n=="json_size"||n=="json_number"){arity(1);weak.push_back(args[0]);return scalar(Type::I64,at);}
  if(n=="json_print"){arity(1);weak.push_back(args[0]);return scalar(Type::Void,at);}
  fail(at,"unsupported intrinsic: "+n);
 }
 Term instantiate(const Type& schema,bool rigid,const Location& at){
  std::vector<Term> variables;
   for(std::size_t i=0;i<schema.generic_identity();++i){
    variables.push_back(rigid?annotation(Type::generic(limits.nodes+(++rigid_serial)),at):fresh(at));
   }
   return annotation(schema.body(),at,&variables);
 }
 ExprPtr wrap(Expr& e,Expr::Kind kind){
  // Inference stores pointers to output fields and stable expression identities.
  // Moving a node into a coercion must redirect those references to the child.
  auto old=std::make_unique<Expr>(std::move(e));
   for(auto& slot:slots){if(slot.output==&e.type)slot.output=&old->type;if(slot.output==&e.result)slot.output=&old->result;}
   for(auto& rename:renames)if(rename.first==&e)rename.first=old.get();
   for(auto& projection:projections)if(projection.expression==&e)projection.expression=old.get();
   for(auto& use:cliche_uses)if(use.expression==&e)use.expression=old.get();
   for(auto& pack:packs)if(pack.expression==&e)pack.expression=old.get();
  e=Expr{};e.where=old->where;e.kind=kind;return old;
 }
  Term project(Expr& e,Term schema){
  auto type=atoms.at(head(schema,e.where));auto instance=instantiate(type,false,e.where);
  auto source=wrap(e,Expr::Kind::PolyProjection);e.children.push_back(std::move(source));
  projections.push_back({&e,schema,instance,slots.size()});
   slots.push_back({&e.type,instance,e.where,"forall instantiation"});return instance;
  }
  struct InherentBinding {
   const InherentMember* member;
   Term callable;
  };
  [[noreturn]] static void inherent_error(const Location& at,
                                          const std::string& message){
   throw Error(at,"ON1X-IMPL: "+message,"ON1X-IMPL");
  }
  void normalize_inherent_owner(Expr& projection){
   if(projection.kind==Expr::Kind::ClicheMember&&
      nominal_names.contains(projection.text)){
    if(cliche_names.contains(projection.text))
     throw Error(projection.where,
                 "ON1X-IMPL-AMBIGUOUS: '::' owner is both a cliche and nominal type: "+
                 projection.text,"ON1X-IMPL-AMBIGUOUS");
    if(!projection.member_type_arguments.empty())
     inherent_error(projection.where,
                    "explicit inherent member type arguments are not supported; infer them from the call");
    auto owner=projection.text;
    projection.kind=Expr::Kind::InherentMember;
    projection.result=Type::named(std::move(owner),
                                  std::move(projection.cliche_arguments));
    projection.text=std::move(projection.cliche_member);
    projection.cliche_arguments.clear();projection.cliche_member.clear();
   }
   if(projection.kind==Expr::Kind::InherentMember&&
      projection.result==Type::Void&&projection.children.size()==1&&
      projection.children[0]&&
      projection.children[0]->kind==Expr::Kind::Variable){
    const auto& owner=projection.children[0]->text;
    if(!local(owner)&&!globals.contains(owner)&&nominal_names.contains(owner)){
     projection.result=Type::named(owner);projection.children.clear();
    }
   }
  }
   InherentBinding bind_inherent(Expr& projection,std::optional<Term> receiver,
                                 bool call_position){
   const auto at=projection.where;
   std::string nominal;
   Term owner=0;
   if(receiver){
    owner=*receiver;auto root=head(owner,at);
    if(!data_kind(nodes[root].kind)||nodes[root].nominal.empty())
     inherent_error(at,"receiver type is not a known nominal record or enum; add an annotation");
    nominal=nodes[root].nominal;
   }else{
    if(projection.result.kind()!=Type::Named&&projection.result.nominal().empty())
     inherent_error(at,"associated member owner is not a nominal type");
    nominal=projection.result.nominal();
    owner=annotation(projection.result,at);
   }
    auto selected=inherent_members.find(nominal,projection.text,at);
   const auto& implementation=*selected.implementation;
   const auto& member=*selected.member;
   const bool takes_receiver=member.receiver!=InherentMember::Receiver::None;
   if(receiver&&!takes_receiver)
    inherent_error(at,"associated member '"+member.name+"' does not accept a receiver");
   if(!receiver&&takes_receiver)
    inherent_error(at,"member '"+member.name+"' requires a receiver");

   std::vector<Term> variables;
   variables.reserve(implementation.type_parameters.size()+member.type_parameters.size());
   for(const auto& parameter:implementation.type_parameters)
    variables.push_back(fresh(parameter.where));
   for(const auto& parameter:member.type_parameters)
    variables.push_back(fresh(parameter.where));
   auto target=annotation(implementation.target,at,&variables);
   unify(owner,target,at);
   if(receiver&&member.receiver==InherentMember::Receiver::Value)
    receiver_checks.push_back({owner,at});

   std::vector<Term> parameters;
   for(const auto& parameter:member.function.params)
    parameters.push_back(annotation(parameter.type,parameter.where,&variables));
   auto output=annotation(member.kind==InherentMember::Kind::Function?
                          member.function.result:member.value_type,at,&variables);
   auto expected_callable=function(std::move(parameters),output,at);

   projection.kind=Expr::Kind::Variable;
   projection.text=member.runtime_name;
   projection.children.clear();
   projection.result=Type::Void;
   projection.cliche_arguments.clear();
   projection.member_type_arguments.clear();
   projection.cliche_member.clear();
   auto actual_callable=name(projection,call_position);
   unify(actual_callable,expected_callable,at);
    return {&member,actual_callable};
   }
   Term inherent_referent(Term source,const Location& at){
    return nodes[head(source,at)].kind==Type::Own?owned_value(source,at):source;
   }
   void check_owned_receiver(const InherentMember& member,Term source,
                             const Location& at){
    if(nodes[head(source,at)].kind==Type::Own&&
       member.receiver!=InherentMember::Receiver::Borrow)
     inherent_error(at,"an owning receiver requires a shared borrowed member");
   }
 bool contains_rigid(Term term,const Location& at,std::set<Term>& seen){
  term=head(term,at);if(!seen.insert(term).second)return false;
  if(nodes[term].kind==Type::Generic)return true;
  if(compound(nodes[term].kind)){
   if(has_result(nodes[term].kind)&&contains_rigid(nodes[term].result,at,seen))return true;
   for(auto child:nodes[term].parameters)if(contains_rigid(child,at,seen))return true;
  }
  return false;
 }
  Term expected(Expr& e,Term expectation){
  expectation=head(expectation,e.where);
  // Bidirectional checking avoids guessing universal types for parameters.
  if(e.kind==Expr::Kind::Tuple&&nodes[expectation].kind==Type::Tuple&&e.children.size()==nodes[expectation].parameters.size()){
   auto parameters=nodes[expectation].parameters;
   try{
    for(std::size_t i=0;i<parameters.size();++i)expected(*e.children[i],parameters[i]);
   }catch(const Error& error){
    std::string message=error.what();
    if(error.diagnostic_id=="ON1X-TYPE"&&message.starts_with("expected "))
     throw Error(error.where,message+" while checking tuple expected "+describe(expectation,e.where),
      error.diagnostic_id,error.expected,error.found);
    throw;
   }
   slots.push_back({&e.type,expectation,e.where,"checked tuple"});return expectation;
  }
  auto first=slots.size();
  auto actual=expression(e,e.kind==Expr::Kind::Lambda&&nodes[expectation].kind==Type::Function?std::optional<Term>{expectation}:std::nullopt);
  expectation=head(expectation,e.where);
  if(nodes[expectation].kind==Type::Forall&&nodes[head(actual,e.where)].kind!=Type::Forall){
   if(!value(e))fail(e.where,"forall introduction requires an immutable nonexpansive value");
   auto source=clone_expression(e,ast_budget);source_expression(*source);
   // A universal proof must not constrain a previously monomorphic environment.
   std::set<Term> protected_variables;
   for(const auto& scope:scopes)for(const auto& [name,binding]:scope){
    (void)name;auto variables=free(binding.type,e.where);
    if(binding.scheme)for(auto variable:binding.scheme->variables)variables.erase(head(variable,e.where));
    protected_variables.insert(variables.begin(),variables.end());
   }
   for(const auto& [name,global]:globals){
    (void)name;auto variables=free(global.signature,e.where);
    if(global.scheme)for(auto variable:global.scheme->variables)variables.erase(head(variable,e.where));
    protected_variables.insert(variables.begin(),variables.end());
   }
   auto proof=instantiate(atoms.at(expectation),true,e.where);unify(actual,proof,e.where);
   for(auto variable:protected_variables){std::set<Term> seen;
    if(contains_rigid(variable,e.where,seen))fail(e.where,"forall proof would specialize a monomorphic environment");
   }
   for(std::size_t i=first;i<slots.size();++i)slots[i].generic=true;
   auto original=wrap(e,Expr::Kind::PolyPack);e.proofs.push_back(std::move(original));
   account_environment(scopes,e.where);
   packs.push_back({&e,std::move(source),scopes,expectation,slots.size()});
   slots.push_back({&e.type,expectation,e.where,"universal value"});return expectation;
  }
   if(nodes[head(actual,e.where)].kind==Type::Forall&&
      nodes[expectation].kind!=Type::Forall&&
      nodes[expectation].kind!=Type::Hole){
    actual=project(e,actual);
   }
   unify(actual,expectation,e.where);return expectation;
  }
  Term call(Expr& e,std::optional<Term> callable_override={},
            std::optional<Term> first_argument={}) {
   auto& callee=*e.callee;
   normalize_inherent_owner(callee);
   if(!callable_override&&callee.kind==Expr::Kind::InherentMember){
    ExprPtr receiver;
    std::optional<Term> receiver_type;
    if(!callee.children.empty()){
      if(callee.children.size()!=1)
       inherent_error(callee.where,"malformed receiver member projection");
      receiver=std::move(callee.children.front());callee.children.clear();
      receiver_type=expression(*receiver);
     }
     std::optional<Term> referent;
     if(receiver_type)referent=inherent_referent(*receiver_type,callee.where);
     auto selected=bind_inherent(callee,referent,true);
    if(selected.member->kind==InherentMember::Kind::Value)
     inherent_error(callee.where,"member value '"+selected.member->name+
                    "' is not callable; remove the argument list");
     callable_override=selected.callable;
     if(receiver){
      check_owned_receiver(*selected.member,*receiver_type,callee.where);
      first_argument=referent;
      e.children.insert(e.children.begin(),std::move(receiver));
    }
   }

   bool intrinsic=!callable_override&&callee.kind==Expr::Kind::Variable&&
                  !local(callee.text)&&!globals.contains(callee.text)&&
                  is_intrinsic_name(callee.text);
   Term callable=0;
   if(callable_override)callable=*callable_override;
   else if(!intrinsic){
    callable=callee.kind==Expr::Kind::Variable?name(callee,true):expression(callee);
    if(nodes[head(callable,e.where)].kind==Type::Forall)callable=project(callee,callable);
    auto kind=nodes[head(callable,e.where)].kind;
    if(kind!=Type::Hole&&kind!=Type::Function)
     fail(callee.where,"cannot call value of type "+describe(callable,e.where));
   }

   std::vector<Term> args;
   auto target=intrinsic?Term{0}:head(callable,e.where);
   const Function* direct_function=nullptr;
   if(!callable_override&&callee.kind==Expr::Kind::Variable&&
      !local(callee.text)){
    auto found=globals.find(callee.text);
    if(found!=globals.end())direct_function=found->second.definition;
   }
   auto policy=[&](std::size_t i){
    return direct_function?kernel_argument_policy(*direct_function,i):KernelArgumentPolicy{};
   };
    for(std::size_t i=0;i<e.children.size();++i){
     const auto borrow=direct_function&&i<direct_function->params.size()?
      direct_function->params[i].borrow:BorrowMode::Value;
     if(i==0&&first_argument){
      args.push_back(*first_argument);
      if(!intrinsic&&nodes[target].kind==Type::Function&&
         i<nodes[target].parameters.size())
       unify(*first_argument,nodes[target].parameters[i],e.children[i]->where);
     }else if(borrow!=BorrowMode::Value){
      auto source=expression(*e.children[i]);
      auto root=head(source,e.children[i]->where);
      auto referent=nodes[root].kind==Type::Own?
       owned_value(source,e.children[i]->where):source;
      args.push_back(referent);
      if(nodes[target].kind==Type::Function&&
         i<nodes[target].parameters.size())
       unify(referent,nodes[target].parameters[i],e.children[i]->where);
     }else if(policy(i).mode==KernelArgumentPolicy::Mode::BoundedSlice)
     args.push_back(expression(*e.children[i]));
    else if(!intrinsic&&nodes[target].kind==Type::Function&&
            i<nodes[target].parameters.size())
     args.push_back(expected(*e.children[i],nodes[target].parameters[i]));
    else args.push_back(expression(*e.children[i]));
   }
   if(intrinsic)return builtin(e,args);
   auto root=head(callable,e.where);
   if(nodes[root].kind==Type::Hole){
    auto result=fresh(e.where);auto shape=function(args,result,e.where);
    unify(callable,shape,e.where);return result;
   }
   if(nodes[root].parameters.size()!=args.size())
    fail(e.where,"wrong number of arguments to function value");
   for(std::size_t i=0;i<args.size();++i){
    const auto& where=e.children[i]->where;
    const auto argument=policy(i);
    if(argument.mode==KernelArgumentPolicy::Mode::BoundedSlice){
     auto actual=head(args[i],where);
     if(nodes[actual].kind!=Type::Slice||nodes[actual].extent>argument.max_extent)
      fail(where,"KernelBind requires a bounded slice<f64,N>");
     auto shape=head(nodes[root].parameters[i],where);
     if(nodes[shape].kind!=Type::Slice||nodes[shape].parameters.size()!=1)
      fail(where,"invalid foreign slice descriptor");
     unify(nodes[actual].parameters[0],nodes[shape].parameters[0],where);
    }else unify(args[i],nodes[root].parameters[i],where);
   }
   return nodes[root].result;
  }
  void reset_ground(){
      ground.assign(nodes.size(),{});state.assign(nodes.size(),0);canonical.clear();next_identity=Type::Own+1;
  }
  void numeric_operand(Term term,const Location& at){
   // Do not quantify numeric overloading. Delay the old i64 choice until all
   // ordinary constraints have been collected, so annotated f64 uses can agree.
   weak.push_back(term);numeric.emplace_back(term,at);
  }
  void default_numeric(){
   for(const auto& [term,at]:numeric){
    tick(at);auto kind=nodes[head(term,at)].kind;
    if(kind==Type::Hole)unify(term,scalar(Type::I64,at),at);
      else if(kind!=Type::I64&&kind!=Type::F64&&kind!=Type::F32&&!(kind>=Type::U8&&kind<=Type::U64))fail(at,"numeric operand requires a numeric scalar, got "+describe(term,at));
   }
  }
 void elaborate_foralls(){
  // A closed forall has one whole-unit bundle layout. Specialized lambda bodies
  // can introduce new demands, so discover layouts before freezing any types.
   for(unsigned round=0;round<128;++round){
    default_numeric();
   reset_ground();bool changed=false;
   for(auto& projection:projections){
    auto schema=atoms.at(head(projection.schema,projection.expression->where));
    auto type=resolve(projection.instance,projection.expression->where,"forall use",slots[projection.slot].generic).type;
    projection.expression->prototype=slots[projection.slot].generic;
    if(type.has_generics())continue;
    if(!forall_instance(schema,type))fail(projection.expression->where,"invalid predicative forall instantiation");
    auto& layout=unit.forall_layouts[schema];
    if(std::find(layout.begin(),layout.end(),type)==layout.end()){
     if(layout.size()>=limits.specializations)fail(projection.expression->where,"forall layout limit exceeded");
     layout.push_back(type);changed=true;
    }
   }
   // std::deque keeps entries stable when a specialization introduces packs.
   for(std::size_t p=0;p<packs.size();++p){
    auto& pack=packs[p];pack.expression->prototype=slots[pack.slot].generic;
    if(pack.expression->prototype)continue;
    auto schema=atoms.at(head(pack.schema,pack.expression->where));auto layout=unit.forall_layouts[schema];
    while(pack.expression->children.size()<layout.size()){
     if(instance_serial++>=limits.specializations)fail(pack.expression->where,"forall specialization limit exceeded");
     auto index=pack.expression->children.size();auto instance=clone_expression(*pack.source,ast_budget);source_expression(*instance);
     auto saved=std::move(scopes);scopes=pack.environment;
     auto actual=expression(*instance);unify(actual,annotation(layout[index],instance->where),instance->where);
     scopes=std::move(saved);pack.expression->children.push_back(std::move(instance));changed=true;
    }
   }
   if(!changed){
    reset_ground();
    for(auto& projection:projections)if(!projection.expression->prototype){
     auto schema=atoms.at(head(projection.schema,projection.expression->where));
     auto type=resolve(projection.instance,projection.expression->where,"forall use").type;
     const auto& layout=unit.forall_layouts.at(schema);
     projection.expression->projection=static_cast<std::size_t>(std::find(layout.begin(),layout.end(),type)-layout.begin());
    }
    return;
   }
  }
  fail(unit.functions.front().where,"forall demand expansion limit exceeded");
 }
  std::map<std::string,Term> match_pattern(Pattern& p,Term input,std::size_t depth=0){
   tick(p.where);if(depth>=256)fail(p.where,"pattern inference depth limit exceeded");
   using P=Pattern::Kind;std::map<std::string,Term> out;
   slots.push_back({&p.type,input,p.where,"match pattern"});
   auto put=[&](const std::string& name,Term t){if(name.empty()||!out.emplace(name,t).second)fail(p.where,"duplicate pattern binder: "+name);};
   auto child=[&](std::size_t i,Term t){for(const auto& [name,type]:match_pattern(p.elements.at(i),t,depth+1))put(name,type);};
   auto container=head(input,p.where);auto kind=nodes[container].kind;
   if(p.is_tuple()){
    if(kind==Type::Hole){std::vector<Term> fields;for(std::size_t i=0;i<p.elements.size();++i)fields.push_back(fresh(p.where));unify(input,add(Type::Tuple,fields,0,p.where),p.where);container=head(input,p.where);}
    auto fields=nodes[container].parameters;if(nodes[container].kind!=Type::Tuple||fields.size()!=p.elements.size())fail(p.where,"tuple pattern arity/type mismatch");
    for(std::size_t i=0;i<fields.size();++i)child(i,fields[i]);
    return out;
   }
   switch(p.kind){
   case P::Name:put(p.name,input);break;
   case P::Wildcard:case P::Rest:break;
   case P::Literal:{auto value=patterns::literal(p);unify(input,scalar(value.type.kind(),p.where),p.where);break;}
   case P::As:if(p.elements.size()!=1)fail(p.where,"invalid as-pattern");child(0,input);put(p.name,input);break;
   case P::Or:{
    if(p.elements.size()!=2)fail(p.where,"invalid or-pattern");
    auto a=match_pattern(p.elements[0],input,depth+1),b=match_pattern(p.elements[1],input,depth+1);
    if(a.size()!=b.size())fail(p.where,"or-pattern alternatives must bind the same names");
    for(const auto& [name,t]:a){auto found=b.find(name);if(found==b.end())fail(p.where,"or-pattern alternatives must bind the same names");unify(t,found->second,p.where);}out=std::move(a);break;
   }
   case P::Record:{
    if(kind==Type::Hole&&!p.open){std::vector<Term> fields;for(std::size_t i=0;i<p.elements.size();++i)fields.push_back(fresh(p.where));unify(input,data(Type::Record,fields,p.labels,0,p.where),p.where);container=head(input,p.where);}
    if(nodes[container].kind!=Type::Record)fail(p.where,"record pattern requires a known record shape");
    auto labels=nodes[container].labels;auto fields=nodes[container].parameters;
    if(p.labels.size()!=p.elements.size())fail(p.where,"record pattern field arity mismatch");
    for(std::size_t i=0;i<p.labels.size();++i){auto f=std::find(labels.begin(),labels.end(),p.labels[i]);if(f==labels.end())fail(p.where,"unknown record pattern field: "+p.labels[i]);child(i,fields[static_cast<std::size_t>(f-labels.begin())]);}break;
   }
   case P::Array:{
    if(kind==Type::Hole){unify(input,data(Type::Array,{fresh(p.where)},{},p.elements.size(),p.where),p.where);container=head(input,p.where);}
    kind=nodes[container].kind;if(kind!=Type::Array&&kind!=Type::Slice)fail(p.where,"array pattern requires a known array or slice");
    auto element=nodes[container].parameters[0];for(std::size_t i=0;i<p.elements.size();++i)child(i,element);break;
   }
   case P::Variant:{
    if(kind!=Type::Sum)fail(p.where,"constructor pattern requires a known closed sum; annotate the parameter");
    auto labels=nodes[container].labels;auto fields=nodes[container].parameters;auto tag=std::find(labels.begin(),labels.end(),p.name);if(tag==labels.end())fail(p.where,"unknown constructor: "+p.name);
    auto payload=fields[static_cast<std::size_t>(tag-labels.begin())];
     if(p.elements.size()==1)child(0,payload);else{
     auto h=head(payload,p.where);auto parts=nodes[h].parameters;if(p.elements.empty()&&nodes[h].kind==Type::Record&&parts.empty())break;
     if(nodes[h].kind!=Type::Tuple||parts.size()!=p.elements.size())fail(p.where,"constructor payload arity mismatch");
     for(std::size_t i=0;i<parts.size();++i)child(i,parts[i]);
    }break;
   }
   case P::Tuple:fail(p.where,"invalid tuple pattern");
   }
   return out;
  }
   Term expression(Expr& e,std::optional<Term> hint={}) {
   Term result=0;const auto& at=e.where;
   tick(at);
   normalize_inherent_owner(e);
      switch(e.kind) {
       case Expr::Kind::SourceCsp:{
        auto operand=[&](std::size_t i){if(i>=e.children.size()||!e.children[i])fail(at,"missing concurrency operand");return expression(*e.children[i]);};
        auto empty=[&](const Location& where){return data(Type::Record,{},{},0,where);};
        auto receive_result=[&](Term payload,const Location& where){
         return data(Type::Sum,{empty(where),payload},{"None","Some"},0,where);
        };
        auto try_receive_result=[&](Term payload,const Location& where){
         return data(Type::Sum,{empty(where),empty(where),payload},
                     {"Closed","Empty","Value"},0,where);
        };
        if(e.text=="poll"){
         if(e.children.empty()||e.children.size()>18)fail(at,"poll offer limit/arity mismatch");
         std::vector<Term> fields;std::vector<std::string> labels;bool has_deadline=false;
          for(std::size_t i=0;i<e.children.size();++i){auto& offer=*e.children[i];
           if(offer.kind==Expr::Kind::Boolean&&offer.text=="true"&&i+1==e.children.size()){
            fields.push_back(empty(offer.where));labels.push_back("Default");break;
           }
         if(offer.kind==Expr::Kind::SourceCsp&&offer.text=="offer_after"){
          if(has_deadline||offer.children.size()!=1)fail(offer.where,"invalid or duplicate poll deadline");
          has_deadline=true;
          offer.type=Type::Void;unify(expression(*offer.children[0]),scalar(Type::I64,offer.where),offer.where);
           fields.push_back(empty(offer.where));labels.push_back("After");continue;
         }
         if(offer.kind!=Expr::Kind::SourceCsp||
            (offer.text!="offer_recv"&&offer.text!="offer_send")||offer.children.size()!=(offer.text=="offer_recv"?1u:2u))fail(offer.where,"invalid poll offer");
         offer.type=Type::Void;
           auto endpoint=expression(*offer.children[0]);auto root=head(endpoint,offer.where);
           auto kind=nodes[root].kind;
           bool receive=offer.text=="offer_recv";
           if(receive?kind!=Type::Receiver:kind!=Type::Sender)
            fail(offer.where,receive?"poll receive expects receiver<T>":"poll send expects sender<T>");
           if(nodes[root].parameters.size()!=1)fail(offer.where,"invalid poll endpoint payload");
           auto payload=nodes[root].parameters.front();
           if(!receive)unify(expression(*offer.children[1]),payload,offer.where);
           fields.push_back(receive?payload:empty(offer.where));
          labels.push_back((offer.text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
          fields.push_back(empty(offer.where));labels.push_back("Closed"+std::to_string(i));
         }
         result=data(Type::Sum,std::move(fields),std::move(labels),0,at);break;
        }
       if(e.text=="send"){
        auto left=operand(0);auto k=nodes[head(left,at)].kind;
         if(k!=Type::Sender&&k!=Type::Receiver&&k!=Type::Channel&&
            k!=Type::Fiber&&k!=Type::ExternalFiber){
         // `<-` used to tokenize as `<` then unary `-`. Keep existing numeric
         // comparisons, including source without whitespace, byte-for-byte.
        auto neg=std::make_unique<Expr>();neg->kind=Expr::Kind::Unary;neg->where=at;neg->text="-";neg->children.push_back(std::move(e.children.at(1)));e.children[1]=std::move(neg);e.kind=Expr::Kind::Binary;e.text="<";
         auto right=operand(1);unify(left,right,at);numeric_operand(left,at);result=scalar(Type::Bool,at);break;
        }
         if(k!=Type::Sender)fail(at,"send expects sender<T>");
         auto root=head(left,at);if(nodes[root].parameters.size()!=1)fail(at,"invalid sender payload type");
         unify(operand(1),nodes[root].parameters.front(),at);
         result=annotation(csp::send_result(),at);
        }else if(e.text=="launch"||e.text=="launch_async"){unify(operand(0),function({},scalar(Type::I64,at),at),at);result=scalar(Type::Fiber,at);}
       else if(e.text=="await_timer"){
        if(e.children.size()!=1)fail(at,"await timer expects one millisecond duration");
        unify(operand(0),scalar(Type::I64,at),at);result=scalar(Type::Void,at);
       }
       else if(e.text=="launch_exec"){
        if(e.children.empty()||e.children.size()>9)fail(at,"external argv limit/arity mismatch");
        for(std::size_t i=0;i<e.children.size();++i)unify(operand(i),scalar(Type::String,at),at);
        result=scalar(Type::ExternalFiber,at);
       }
       else if(e.text=="launch_capture"){
        if(e.children.empty()||e.children.size()>9)fail(at,"captured shell argv limit/arity mismatch");
        for(std::size_t i=0;i<e.children.size();++i)unify(operand(i),scalar(Type::String,at),at);
         auto byte=scalar(Type::U8,at);
         result=add(Type::Tuple,{scalar(Type::ExternalFiber,at),
                    channel(Type::Sender,byte,at),channel(Type::Receiver,byte,at),
                    channel(Type::Receiver,byte,at)},0,at);
        }
       else if(e.text=="channel"){
        if(e.children.size()!=3)fail(at,"channel operand arity mismatch");
        for(std::size_t i=0;i<3;++i)unify(operand(i),scalar(Type::I64,at),at);
        if(e.children[0]->kind!=Expr::Kind::Integer||
           e.children[1]->kind!=Expr::Kind::Integer)
         fail(at,"channel endpoint cardinalities must be decimal integer literals");
        auto senders=integer_literal_bits(e.children[0]->text,e.children[0]->where);
        auto receivers=integer_literal_bits(e.children[1]->text,e.children[1]->where);
        if(senders<=0||receivers<=0||senders>256||receivers>256||
           senders+receivers>256)
         fail(at,"channel endpoint cardinalities must be positive with total at most 256");
        auto payload=annotation(e.result,at);
        slots.push_back({&e.result,payload,at,"channel payload type"});
        result=channel(Type::Channel,payload,at,static_cast<std::size_t>(senders),
                       static_cast<std::size_t>(receivers));
       }
       else if(e.text=="split"){
        auto descriptor=head(operand(0),at);
        if(nodes[descriptor].kind!=Type::Channel||!nodes[descriptor].senders||
           !nodes[descriptor].receivers||nodes[descriptor].parameters.size()!=1)
         fail(at,"split expects a channel descriptor with known cardinalities");
        std::vector<Term> endpoints;
        endpoints.reserve(nodes[descriptor].senders+nodes[descriptor].receivers);
        for(std::size_t i=0;i<nodes[descriptor].senders;++i)
         endpoints.push_back(channel(Type::Sender,nodes[descriptor].parameters[0],at));
        for(std::size_t i=0;i<nodes[descriptor].receivers;++i)
         endpoints.push_back(channel(Type::Receiver,nodes[descriptor].parameters[0],at));
        result=add(Type::Tuple,std::move(endpoints),0,at);
       }
       else if(e.text=="join"){
        auto t=nodes[head(operand(0),at)].kind;
        if(t!=Type::Fiber&&t!=Type::ExternalFiber)fail(at,"join expects an internal or external fiber");
        result=scalar(Type::I64,at);
       }
       else if(e.text=="cancel"){
        auto t=operand(0);auto kind=nodes[head(t,at)].kind;
        if(kind!=Type::Fiber&&kind!=Type::ExternalFiber)fail(at,"cancel requires an internal or external fiber");
        result=scalar(Type::Bool,at);
       }
       else if(e.text=="recv"){
         auto endpoint=head(operand(0),at);
         if(nodes[endpoint].kind!=Type::Receiver||nodes[endpoint].parameters.size()!=1)
          fail(at,"receive expects receiver<T>");
         result=receive_result(nodes[endpoint].parameters.front(),at);
        }
        else if(e.text=="close"){
         auto endpoint=operand(0);auto kind=nodes[head(endpoint,at)].kind;
         if(kind!=Type::Sender)fail(at,"close expects sender<T>");
         result=scalar(Type::Bool,at);
        }
        else if(e.text=="try_send"){
         if(e.children.size()!=2)fail(at,"try_send requires an endpoint and payload");
         auto endpoint=head(operand(0),at);
         if(nodes[endpoint].kind!=Type::Sender||nodes[endpoint].parameters.size()!=1)
          fail(at,"try_send expects sender<T>");
         unify(operand(1),nodes[endpoint].parameters.front(),at);
         result=annotation(csp::try_send_result(),at);
        }
        else if(e.text=="try_recv"){
         if(e.children.size()!=1)fail(at,"try_recv requires one receiver");
         auto endpoint=head(operand(0),at);
         if(nodes[endpoint].kind!=Type::Receiver||nodes[endpoint].parameters.size()!=1)
          fail(at,"try_recv expects receiver<T>");
         result=try_receive_result(nodes[endpoint].parameters.front(),at);
        }
       else fail(at,"unknown source concurrency operation");
      break;
     }
     case Expr::Kind::Csp:fail(at,"private concurrency operation before inference");
    case Expr::Kind::Propagate:{
     if(e.children.size()!=1)fail(at,"propagation requires one operand");
     auto input=head(expression(*e.children[0]),at);
     if(nodes[input].kind!=Type::Sum||nodes[input].nominal.empty())fail(at,"propagation requires a known nominal Result");
     auto identity=nodes[input].nominal;catalog.result(identity,at);
     auto fields=nodes[input].parameters;auto labels=nodes[input].labels;
     auto error=fields[static_cast<std::size_t>(std::find(labels.begin(),labels.end(),"Err")-labels.begin())];
     result=fields[static_cast<std::size_t>(std::find(labels.begin(),labels.end(),"Ok")-labels.begin())];
     auto output=head(returns,at);
     if(nodes[output].kind!=Type::Sum||nodes[output].nominal!=identity)fail(at,"propagation requires the same nominal Result return declaration; annotate the return type");
     auto out_labels=nodes[output].labels;
     unify(error,nodes[output].parameters[static_cast<std::size_t>(std::find(out_labels.begin(),out_labels.end(),"Err")-out_labels.begin())],at);
     slots.push_back({&e.result,returns,at,"propagation return type"});break;
    }
   case Expr::Kind::Splice:case Expr::Kind::Lift:fail(at,"MSP stage lowering required before inference");
   case Expr::Kind::Quote:{
    auto callable=expression(*e.children.at(0));auto result_type=fresh(at);unify(callable,function({},result_type,at),at);result=add(Type::Code,{},result_type,at);break;
   }
   case Expr::Kind::Run:{
    auto code=expression(*e.children.at(0));result=fresh(at);unify(code,add(Type::Code,{},result,at),at);break;
   }
    case Expr::Kind::MacroCall:case Expr::Kind::SyntaxHole:case Expr::Kind::SyntaxBlock:case Expr::Kind::Extension:case Expr::Kind::SyntaxList:case Expr::Kind::SyntaxVector:case Expr::Kind::SyntaxKeyword:case Expr::Kind::SyntaxLet:
    fail(at,"unexpanded metaprogramming syntax before inference");
   case Expr::Kind::PolyPack:case Expr::Kind::PolyProjection:
    fail(at,"cannot reinfer an elaborated polymorphic expression");
    case Expr::Kind::Integer:case Expr::Kind::Float:result=scalar(numeric_literal_type(e.text).kind(),at);break;
  case Expr::Kind::Boolean:result=scalar(Type::Bool,at);break;
  case Expr::Kind::String:result=scalar(Type::String,at);break;
  case Expr::Kind::Character:result=scalar(Type::Char,at);break;
  case Expr::Kind::ByteBuffer:result=scalar(Type::Buffer,at);break;
  case Expr::Kind::Interpolated:
   for(auto& part:e.children)weak.push_back(expression(*part));
   result=scalar(Type::String,at);break;
  case Expr::Kind::RegexLiteral:result=scalar(Type::Regex,at);break;
   case Expr::Kind::RegexMatch:case Expr::Kind::RegexCapture:{
    if(e.children.size()!=2)fail(at,"regex operation needs a pattern and text");
    unify(expression(*e.children[0]),scalar(Type::Regex,at),e.children[0]->where);
    unify(expression(*e.children[1]),scalar(Type::String,at),e.children[1]->where);
    result=scalar(e.kind==Expr::Kind::RegexMatch?Type::Bool:Type::RegexCaptures,at);break;
   }
    case Expr::Kind::ClicheMember:{
     if(e.callee||!e.children.empty()||!e.body.empty()||e.text.empty()||
        e.cliche_member.empty())
      throw Error(at,"malformed cliche member projection","ON1X-CLICHE-OBLIGATION");
     const auto& declaration=cliches::declaration(unit,e.text,at);
     if(e.cliche_arguments.size()!=declaration.type_parameters.size()+1)
      throw Error(at,"wrong cliche argument arity for "+e.text,
                  "ON1X-CLICHE-OBLIGATION");
     const auto& member=cliches::member(unit,e.text,e.cliche_member,at);
     if(e.member_type_arguments.size()!=member.type_parameters.size())
      throw Error(at,"wrong member type-argument arity for "+e.text+"::"+
                  e.cliche_member,"ON1X-CLICHE-OBLIGATION");
     std::vector<Term> arguments,member_arguments,variables;
     for(const auto& argument:e.cliche_arguments)
      arguments.push_back(annotation(argument,at));
     for(const auto& argument:e.member_type_arguments)
      member_arguments.push_back(annotation(argument,at));
     variables=arguments;
     variables.insert(variables.end(),member_arguments.begin(),member_arguments.end());
     auto member_result=annotation(member.result,member.where,&variables);
     if(member.kind==ClicheMember::Kind::Function){
      std::vector<Term> parameters;
      for(const auto& parameter:member.params)
       parameters.push_back(annotation(parameter.type,parameter.where,&variables));
      result=function(std::move(parameters),member_result,at);
     }else result=member_result;
     const auto slot=slots.size();
     cliche_uses.push_back({&e,e.text,e.cliche_member,std::move(arguments),
                            std::move(member_arguments),result,slot,
                            active_constraints?*active_constraints:
                             std::vector<Predicate>{},
                            member.kind==ClicheMember::Kind::Value});
     for(const auto& constraint:member.constraints)
      require_later(predicate(constraint,variables),at);
     break;
    }
     case Expr::Kind::InherentMember:{
     ExprPtr receiver;
     std::optional<Term> receiver_type;
     if(!e.children.empty()){
      if(e.children.size()!=1)inherent_error(at,"malformed receiver member projection");
      receiver=std::move(e.children.front());e.children.clear();
      receiver_type=expression(*receiver);
     }
      std::optional<Term> referent;
      if(receiver_type)referent=inherent_referent(*receiver_type,at);
      auto selected=bind_inherent(e,referent,false);
      if(receiver)check_owned_receiver(*selected.member,*receiver_type,at);
     if(selected.member->kind==InherentMember::Kind::Function){
      if(receiver)
       inherent_error(at,"receiver method '"+selected.member->name+
                      "' must be called with an argument list");
      result=selected.callable;
     }else{
      auto callee=wrap(e,Expr::Kind::Call);e.callee=std::move(callee);
      if(receiver)e.children.push_back(std::move(receiver));
       result=call(e,selected.callable,referent);
     }
     break;
    }
     case Expr::Kind::Variable:return name(e);
     case Expr::Kind::Borrow:{
      if(e.children.size()!=1||!e.children[0]||
         e.children[0]->kind!=Expr::Kind::Variable)
       fail(at,"borrow requires one named local place");
      auto source=expression(*e.children[0]);
      auto root=head(source,at);
      result=nodes[root].kind==Type::Own?owned_value(source,at):source;
      break;
     }
     case Expr::Kind::Unit:result=scalar(Type::Void,at);break;
     case Expr::Kind::UnsafeBlock:
      if(e.children.size()!=1||!e.children[0])fail(at,"malformed unsafe expression");
      scopes.emplace_back();block(e.body,false);
      result=expression(*e.children[0]);scopes.pop_back();
      if(patterns::terminates(*e.children[0]))result=scalar(Type::Void,at);
      break;
    case Expr::Kind::Loop:{
     result=fresh(at);loop_values.push_back({e.control_target,result});block(e.body);
     auto exits=loop_values.back();loop_values.pop_back();if(exits.bare||!exits.valued)result=scalar(Type::Void,at);break;
    }
    case Expr::Kind::PatternMatch:{
     auto scrutinee=expression(*e.children.at(0));
     if(e.sum_match&&nodes[head(scrutinee,at)].kind!=Type::Sum)
      fail(at,"'~ { ... }' requires a known closed sum (enum) subject; annotate the subject if needed");
     result=fresh(at);bool fallthrough=false;scopes.emplace_back();
    if(!e.text.empty())define(e.text,scrutinee,at);
    for(std::size_t i=1;i<e.children.size();++i){auto& arm=*e.children[i];if(arm.kind!=Expr::Kind::Case||arm.patterns.size()!=1||arm.children.size()!=2)fail(arm.where,"malformed match arm");
     scopes.emplace_back();for(const auto& [name,t]:match_pattern(arm.patterns[0],scrutinee))define(name,t,arm.where);
     unify(expression(*arm.children[0]),scalar(Type::Bool,arm.where),arm.where);scopes.emplace_back();block(arm.body,false);auto value=expression(*arm.children[1]);
     if(!e.statement_match&&!patterns::terminates(arm.body)&&!patterns::terminates(*arm.children[1])){unify(value,result,arm.where);fallthrough=true;}scopes.pop_back();scopes.pop_back();
    }
    scopes.pop_back();if(e.statement_match||!fallthrough)result=scalar(Type::Void,at);break;
   }
   case Expr::Kind::Case:case Expr::Kind::Region:case Expr::Kind::TagTest:case Expr::Kind::Payload:case Expr::Kind::Project:fail(at,"private or misplaced expression before inference");
   case Expr::Kind::Tuple:{
    std::vector<Term> elements;for(auto& element:e.children)elements.push_back(expression(*element));
    result=add(Type::Tuple,std::move(elements),0,at);break;
   }
    case Expr::Kind::Record:{
     std::vector<Term> fields;for(auto& child:e.children)fields.push_back(expression(*child));
     auto actual=data(Type::Record,std::move(fields),e.labels,0,at);
     if(e.result!=Type::Void){
      result=annotation(e.result,at);if(nodes[result].kind!=Type::Record||nodes[result].nominal.empty())fail(at,"named record constructor requires a declared record");
      auto expected_fields=nodes[result].parameters;expected_fields.resize(nodes[result].labels.size());
      unify(actual,data(Type::Record,std::move(expected_fields),nodes[result].labels,0,at),at);
      slots.push_back({&e.result,result,at,"named record schema"});
     }else result=actual;
     break;
   }
   case Expr::Kind::Array:{
    auto element=fresh(at);for(auto& child:e.children)unify(expression(*child),element,child->where);
    result=data(Type::Array,{element},{},e.children.size(),at);break;
   }
   case Expr::Kind::Variant:{
     result=annotation(e.result,at);if(nodes[result].kind!=Type::Sum)fail(at,"variant requires a closed sum annotation");
     auto tags=nodes[result].labels;auto fields=nodes[result].parameters;
    auto tag=std::find(tags.begin(),tags.end(),e.text);if(tag==tags.end())fail(at,"unknown sum constructor: "+e.text);
    if(e.children.size()!=1)fail(at,"variant requires one payload");
    expected(*e.children[0],fields[static_cast<std::size_t>(tag-tags.begin())]);
    slots.push_back({&e.result,result,at,"variant schema"});break;
   }
   case Expr::Kind::Field:case Expr::Kind::Index:case Expr::Kind::Slice:case Expr::Kind::Length:{
    auto container=head(expression(*e.children.at(0)),at);auto kind=nodes[container].kind;
    if(e.kind==Expr::Kind::Length&&e.text==canonical::iteration_length_marker&&
       kind!=Type::Array&&kind!=Type::Slice)
     throw Error(at,canonical::iteration_type_error,"ON1X-FOR-IN");
    if(e.kind==Expr::Kind::Index&&e.text==":"&&kind!=Type::RegexCaptures){
     unify(container,scalar(Type::RegexCaptures,at),at);
     kind=Type::RegexCaptures;
    }
    if(kind==Type::RegexCaptures){
     if(e.kind==Expr::Kind::Length){result=scalar(Type::I64,at);break;}
     if(e.kind!=Expr::Kind::Index)fail(at,"captures support only [] and [index]");
     unify(expression(*e.children.at(1)),scalar(Type::I64,at),at);
     result=scalar(Type::String,at);break;
    }
    if(e.kind==Expr::Kind::Field){
     if(kind!=Type::Record)fail(at,"field projection requires a known record shape; annotate the parameter");
     auto tags=nodes[container].labels;auto tag=std::find(tags.begin(),tags.end(),e.text);
     if(tag==tags.end())fail(at,"unknown record field: "+e.text);
     result=nodes[container].parameters[static_cast<std::size_t>(tag-tags.begin())];break;
    }
    if(kind!=Type::Array&&kind!=Type::Slice)fail(at,"index/slice/length requires a known array or slice shape; annotate the parameter");
    auto element=nodes[container].parameters[0];auto extent=nodes[container].extent;
    for(std::size_t i=1;i<e.children.size();++i)unify(expression(*e.children[i]),scalar(Type::I64,at),at);
    result=e.kind==Expr::Kind::Length?scalar(Type::I64,at):e.kind==Expr::Kind::Slice?data(Type::Slice,{element},{},extent,at):element;break;
   }
   case Expr::Kind::Match:{
    auto scrutinee=head(expression(*e.children.at(0)),at);
    if(nodes[scrutinee].kind!=Type::Sum)fail(at,"match requires a known closed sum shape; annotate the parameter");
    auto tags=nodes[scrutinee].labels;auto fields=nodes[scrutinee].parameters;std::set<std::string> seen;result=fresh(at);
    if(e.children.size()!=e.labels.size()+1)fail(at,"invalid match arm shape");
    for(std::size_t i=0;i<e.labels.size();++i){
     auto tag=std::find(tags.begin(),tags.end(),e.labels[i]);
     if(tag==tags.end())fail(at,"unknown match constructor: "+e.labels[i]);
     if(!seen.insert(e.labels[i]).second)fail(at,"duplicate match constructor: "+e.labels[i]);
     if(e.children[i+1]->kind!=Expr::Kind::Lambda||e.children[i+1]->parameters.size()!=1)fail(at,"match arm requires one shared payload pattern");
     auto arm=function({fields[static_cast<std::size_t>(tag-tags.begin())]},result,at);expected(*e.children[i+1],arm);
    }
    if(seen.size()!=tags.size())fail(at,"non-exhaustive match: every constructor needs an arm");
    break;
   }
  case Expr::Kind::Lambda:{
   std::vector<Term> parameters;std::set<std::string> names;
   for(auto& parameter:e.parameters){
    if(!names.insert(parameter.name).second)fail(parameter.where,"duplicate parameter: "+parameter.name);
    auto type=annotation(parameter.type,parameter.where);parameters.push_back(type);
     slots.push_back({&parameter.type,type,parameter.where,parameter.pattern?"anonymous-function parameter pattern":"anonymous-function parameter "+parameter.name});
   }
   auto result_type=annotation(e.result,at);
   slots.push_back({&e.result,result_type,at,"anonymous-function return type"});
    result=function(parameters,result_type,at);
    if(hint)unify(result,*hint,at);
    auto previous_returns=returns;bool previous_saw_return=saw_return;auto previous_loops=std::move(loop_values);loop_values.clear();
   scopes.emplace_back();
    for(std::size_t i=0;i<e.parameters.size();++i){
     const auto& p=e.parameters[i];
     if(p.pattern)bind_pattern(*p.pattern,parameters[i]);else define(p.name,parameters[i],p.where);
    }
   returns=result_type;saw_return=false;block(e.body,false);scopes.pop_back();
    if(!saw_return&&nodes[head(result_type,at)].kind==Type::Hole)
     unify(result_type,scalar(Type::Void,at),at);
    returns=previous_returns;saw_return=previous_saw_return;loop_values=std::move(previous_loops);break;
  }
   case Expr::Kind::Unary:{
    result=expression(*e.children[0]);
    if(e.text=="!")unify(result,scalar(Type::Bool,at),at);else numeric_operand(result,at);
    break;
  }
  case Expr::Kind::Binary:{
   auto left=expression(*e.children[0]),right=expression(*e.children[1]);
   if(e.text=="=="||e.text=="!="){unify(left,right,at);weak.push_back(left);}
     else if(e.text=="&&"||e.text=="||"){
      auto operand=scalar(Type::Bool,at);
     unify(left,operand,at);unify(right,operand,at);
     }else{
      auto left_kind=nodes[head(left,at)].kind,right_kind=nodes[head(right,at)].kind;
      if(left_kind!=Type::Hole&&!numeric_kind(left_kind))
       fail(at,"numeric operand requires a numeric scalar; expected i64, got "+describe(left,at));
      if(right_kind!=Type::Hole&&!numeric_kind(right_kind))
       fail(at,"numeric operand requires a numeric scalar; expected "+describe(left,at)+", got "+describe(right,at));
      if(left_kind>=Type::U8&&left_kind<=Type::U64&&right_kind==Type::I64)unify(left,right,at);
      else unify(right,left,at);
      numeric_operand(left,at);
     }
    bool arithmetic=e.text=="+"||e.text=="-"||e.text=="*"||e.text=="/"||e.text=="%";
    result=arithmetic?left:scalar(Type::Bool,at);break;
  }
   case Expr::Kind::Call:result=call(e);break;
  }
  slots.push_back({&e.type,result,at,"expression type"});return result;
 }
 void block(std::vector<Stmt>& body,bool own_scope=true) {
  if(own_scope)scopes.emplace_back();
  for(auto& statement:body) {
   const auto& at=statement.where;
   tick(at);
     switch(statement.kind) {
     case Stmt::Kind::TaskGroup:
      if(statement.value)expected(*statement.value,scalar(Type::I64,at));
      block(statement.body);break;
     case Stmt::Kind::CspScope:fail(at,"private task scope before inference");
    case Stmt::Kind::Defer:expression(*statement.value);break;
    case Stmt::Kind::PushDefer:fail(at,"private defer registration before inference");
    case Stmt::Kind::Let:{
     auto first=slots.size();
     Term value;
    if(statement.declared!=Type::Invalid) {
     auto declared=annotation(statement.declared,at);value=expected(*statement.value,declared);
     slots.push_back({&statement.declared,declared,at,statement.pattern?"tuple binding annotation":"annotation for "+statement.name});
    }else value=expression(*statement.value);
     const bool borrowed=statement.value&&
                         statement.value->kind==Expr::Kind::Borrow;
     if(!borrowed)
      if(!generalize_pattern(statement,value,first))
       generalize(statement,value,first);
     if(statement.kind!=Stmt::Kind::Template){
     if(statement.pattern)bind_pattern(*statement.pattern,value);else define(statement.name,value,at);
    }
    break;
   }
   case Stmt::Kind::Assign:{
    auto binding=local(statement.name);
    if(!binding)fail(at,"undefined variable: "+statement.name);
     auto value=expected(*statement.value,binding->type);unify(value,binding->type,at);break;
   }
   case Stmt::Kind::Return:{
     auto value=statement.value?expected(*statement.value,returns):scalar(Type::Void,at);
    unify(value,returns,at);saw_return=true;break;
   }
   case Stmt::Kind::If:case Stmt::Kind::While:{
    auto condition=expression(*statement.value);unify(condition,scalar(Type::Bool,at),at);
    block(statement.body);block(statement.otherwise);break;
   }
   case Stmt::Kind::Block:block(statement.body);break;
   case Stmt::Kind::Expr:expression(*statement.value);break;
    case Stmt::Kind::Break:{
     auto target=std::find_if(loop_values.rbegin(),loop_values.rend(),[&](const auto& l){return l.target==statement.control_target;});
     if(target==loop_values.rend()){if(statement.value)fail(at,"value-carrying exit requires an expression loop target");break;}
     auto index=static_cast<std::size_t>(target.base()-loop_values.begin()-1);
     if(statement.value){auto value=expression(*statement.value);if(!patterns::terminates(*statement.value)){unify(value,loop_values[index].type,at);loop_values[index].valued=true;}}
     else loop_values[index].bare=true;
     break;
    }
    case Stmt::Kind::Continue:break;
    case Stmt::Kind::Range:fail(at,"range was not canonicalized");
    case Stmt::Kind::Storage:case Stmt::Kind::Initialize:case Stmt::Kind::Region:case Stmt::Kind::Leave:case Stmt::Kind::Trap:fail(at,"private region statement before inference");
   case Stmt::Kind::Template:fail(at,"cannot reinfer an elaborated template");
   }
  }
  if(own_scope)scopes.pop_back();
 }
 const Ground& resolve(Term term,const Location& at,const std::string& description,bool generic=false) {
  struct Frame { Term term; bool finish; };
  std::vector<Frame> pending{{term,false}};
  while(!pending.empty()) {
   auto frame=pending.back();pending.pop_back();auto id=head(frame.term,at);
   if(ground[id]){
    if(!generic&&ground[id]->type.has_generics())fail(at,"cannot infer concrete specialization; add a type annotation");
    continue;
   }
    auto& node=nodes[id];
    if(auto atom=atoms.find(id);atom!=atoms.end()){
     if(!generic&&atom->second.has_generics())fail(at,"rigid type variable escaped its universal proof");
     ground[id]=Ground{atom->second,next_identity++,1,1};continue;
    }
   if(node.kind==Type::Hole){
    if(!generic||!quantified.contains(id))fail(at,"cannot infer "+description+"; add a concrete type annotation or use");
    ground[id]=Ground{Type::generic(id),next_identity++,1,1};continue;
   }
   if(!compound(node.kind)) {
    ground[id]=Ground{Type(node.kind),static_cast<std::size_t>(node.kind),1,1};continue;
   }
   if(!frame.finish) {
    if(state[id])fail(at,"infinite type during inference resolution");
    state[id]=1;pending.push_back({id,true});
    if(has_result(node.kind))pending.push_back({node.result,false});
    for(auto parameter:node.parameters)pending.push_back({parameter,false});
    continue;
   }
   std::size_t depth=1,size=1;
    std::vector<Type> parameters;std::vector<std::size_t> key{static_cast<std::size_t>(node.kind)};
      if(data_kind(node.kind)){
       auto [layout,inserted]=label_identities.emplace(node.labels,label_identities.size()+1);(void)inserted;
        key.push_back(layout->second);key.push_back(node.extent);
        auto [nominal,added]=nominal_identities.emplace(node.nominal,nominal_identities.size()+1);(void)added;
        key.push_back(nominal->second);key.push_back(node.arguments);
      }else if(node.kind==Type::Own){
      auto [nominal,added]=nominal_identities.emplace(node.nominal,nominal_identities.size()+1);(void)added;
       key.push_back(nominal->second);key.push_back(node.arguments);
      }else if(node.kind==Type::Channel){
       key.push_back(node.senders);key.push_back(node.receivers);
     }
   Type result_type=Type::Invalid;
   if(has_result(node.kind)){
    const auto& result=*ground[head(node.result,at)];depth=result.depth+1;size=result.size+1;
    result_type=result.type;key.push_back(result.identity);
   }
   for(auto parameter:node.parameters) {
    const auto& child=*ground[head(parameter,at)];depth=std::max(depth,child.depth+1);
    if(child.size>limits.type_nodes||size>limits.type_nodes-child.size)
     fail(at,"inferred type exceeds expanded-size limit");
    size+=child.size;parameters.push_back(child.type);key.push_back(child.identity);
   }
   if(depth>limits.depth)fail(at,"inferred type exceeds "+std::to_string(limits.depth)+"-level limit");
   if(size>limits.type_nodes)fail(at,"inferred type exceeds expanded-size limit");
   if(auto found=canonical.find(key);found!=canonical.end())ground[id]=found->second;
   else {
       Type type;
       if(node.kind==Type::Own){
        if(node.nominal.empty()||node.arguments!=parameters.size())
         fail(at,"invalid inferred own<Named> representation");
        type=Type::own(Type::named(node.nominal,std::move(parameters)));
        }else if(channel_kind(node.kind)){
         if(parameters.size()!=1)
          fail(at,"invalid inferred channel payload representation");
         if(node.kind==Type::Channel){
          if(!node.senders||!node.receivers)
           fail(at,"cannot infer channel endpoint cardinality from channel<T> annotation");
          type=Type::channel(std::move(parameters.front()),node.senders,node.receivers);
         }else if(node.kind==Type::Sender)
          type=Type::sender(std::move(parameters.front()));
         else type=Type::receiver(std::move(parameters.front()));
        }else{
        std::vector<Type> arguments;
        if(!node.nominal.empty()){arguments.assign(parameters.end()-node.arguments,parameters.end());parameters.resize(parameters.size()-node.arguments);}
        type=data_kind(node.kind)?Type::data(node.kind,std::move(parameters),node.labels,node.extent):node.kind==Type::Tuple?Type::tuple(std::move(parameters)):node.kind==Type::Code?Type::code(std::move(result_type)):Type::function(std::move(parameters),std::move(result_type));
        if(!node.nominal.empty())type=type.with_nominal(node.nominal,std::move(arguments));
       }
    Ground value{std::move(type),next_identity++,depth,size};
    canonical.emplace(std::move(key),value);ground[id]=std::move(value);
   }
  }
  return *ground[head(term,at)];
 }
public:
   Inference(Unit& source,const InferenceLimits& budget)
    :unit(source),catalog(source),inherent_members(source),limits(budget),
     ast_budget(budget.expanded_ast) {
    for(const auto& nominal:unit.nominals)nominal_names.insert(nominal.name);
    for(const auto& cliche:unit.cliches)cliche_names.insert(cliche.name);
   }
 void run() {
  // Allocate all declaration variables first: calls, mutual recursion and
  // imported functions constrain the same monotype regardless of body order.
   for(auto& f:unit.functions) {
   if(is_reserved_intrinsic_name(f.name))fail(f.where,"intrinsic name cannot be redefined: "+f.name);
   if(globals.contains(f.name))fail(f.where,"duplicate function: "+f.name);
    if(f.name=="main"&&(!f.type_parameters.empty()||!f.constraints.empty()))
     fail(f.where,"main cannot declare generic binders or cliche constraints");
   if(f.external) {
     if(!f.type_parameters.empty()||!f.constraints.empty())
      fail(f.where,"extern signatures cannot declare generic binders or cliche constraints");
     if(has_type_holes(f.result))fail(f.where,"extern signatures require concrete types; inference is not ABI evidence");
     for(const auto& p:f.params)if(has_type_holes(p.type))
      fail(p.where,"extern signatures require concrete types; inference is not ABI evidence");
     (void)extern_parameter_types(f);
   }
    std::vector<Term> variables;
    std::set<std::string> type_names;
    for(const auto& parameter:f.type_parameters){
     if(!type_names.insert(parameter.name).second)
      fail(parameter.where,"duplicate function type parameter: "+parameter.name);
     variables.push_back(fresh(parameter.where));
    }
    std::vector<Term> parameters;std::set<std::string> names;
    for(auto& p:f.params) {
     if(!names.insert(p.name).second)fail(p.where,"duplicate parameter: "+p.name);
     auto type=annotation(p.type,p.where,&variables);parameters.push_back(type);
     slots.push_back({&p.type,type,p.where,p.pattern?"parameter pattern":"parameter "+p.name});
    }
    auto result=annotation(f.result,f.where,&variables);
    slots.push_back({&f.result,result,f.where,"return type of function "+f.name});
     Global global;
     global.signature=function(std::move(parameters),result,f.where);
     global.external=f.external;global.definition=&f;global.variables=variables;
     for(const auto& constraint:f.constraints)
      global.constraints.push_back(predicate(constraint,variables));
     auto count=f.params.size()+1;
    for(std::size_t i=slots.size()-count;i<slots.size();++i)global.declaration_slots.push_back(i);
    globals.emplace(f.name,std::move(global));
  }
    for(auto& f:unit.functions)if(!f.external)ensure_global(f.name);
     elaborate_cliche_uses();
    elaborate_foralls();
    for(const auto& obligation:obligations)
     discharge(obligation.predicate,&obligation.assumptions,obligation.where);
    for(const auto& check:receiver_checks){
     reset_ground();
     auto target=resolve(check.target,check.where,"value receiver type",true).type;
     if(!target.has_generics()&&!catalog.copy_safe(target,check.where))
      throw Error(check.where,
                  "ON1X-IMPL-RECEIVER: value receiver is not copy-safe",
                  "ON1X-IMPL-RECEIVER");
    }
    reset_ground();
  // Freeze transactionally: errors must not leave a partially grounded unit.
  std::vector<Type> resolved;resolved.reserve(slots.size());
  for(const auto& slot:slots)resolved.push_back(resolve(slot.term,slot.where,slot.description,slot.generic).type);
  for(const auto& f:unit.functions)
    resolve(globals.at(f.name).signature,f.where,"signature of function "+f.name,f.prototype);
  for(std::size_t i=0;i<slots.size();++i)*slots[i].output=std::move(resolved[i]);
  for(const auto& [expression,name]:renames)expression->text=name;
  for(const auto& instance:instances)instance.statement->prototype=slots[instance.use_slot].generic;
  finish(unit.functions);
  std::vector<Function> generated;
  for(auto& source:global_sources)for(auto& instance:source->instances){
   auto& lambda=*instance->value;finish_expression(lambda);
   Function f;f.name=instance->name;f.where=instance->where;f.prototype=instance->prototype;
    f.params=std::move(lambda.parameters);f.result=lambda.result;f.procedure=lambda.procedure;f.body=std::move(lambda.body);
   generated.push_back(std::move(f));
  }
  for(auto& f:generated)unit.functions.push_back(std::move(f));
  unit.forall_elaborated=true;
 }
 void finish_expression(Expr& e){
  if(e.callee)finish_expression(*e.callee);
  for(auto& child:e.children)finish_expression(*child);
  for(auto& proof:e.proofs)finish_expression(*proof);
  finish_body(e.body);
 }
 void finish_body(std::vector<Stmt>& body){
  for(auto& s:body){
   if(s.value)finish_expression(*s.value);
   for(auto& instance:s.instances)s.body.push_back(std::move(*instance));
   s.instances.clear();finish_body(s.body);finish_body(s.otherwise);
  }
 }
 void finish(std::vector<Function>& functions){for(auto& f:functions)finish_body(f.body);}
};
}

void infer_types(Unit& unit,const InferenceLimits& limits) {
  if(needs_inference(unit)||needs_staging(unit)||canonical::needs_control(unit)){
   auto budget=limits.expanded_ast;auto working=clone_unit(unit,budget);
    lower_stages(working);
    canonical::lower_control(working);
   Inference(working,limits).run();canonical::lower_control(working);unit=std::move(working);
 }
}
}

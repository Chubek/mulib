#include <on1x/infer.hpp>
#include <on1x/clone.hpp>
#include <on1x/primitives.hpp>
#include <on1x/staging.hpp>
#include <on1x/numeric.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/patterns.hpp>
#include <on1x/nominal.hpp>
#include <on1x/concurrency.hpp>
#include <deque>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace on1x {
namespace {
bool data_kind(Type::Kind kind){return kind==Type::Record||kind==Type::Sum||kind==Type::Array||kind==Type::Slice;}
bool compound(Type::Kind kind){return kind==Type::Function||kind==Type::Tuple||kind==Type::Code||data_kind(kind);}
bool has_result(Type::Kind kind){return kind==Type::Function||kind==Type::Code;}
bool holes_in(const Expr& expression);
bool holes_in(const std::vector<Stmt>& body) {
 for(const auto& s:body)if(s.kind==Stmt::Kind::Defer)return true;
 for(const auto& statement:body)
   if(has_type_holes(statement.declared)||statement.declared.has_foralls()||(statement.value&&holes_in(*statement.value))||
     holes_in(statement.body)||holes_in(statement.otherwise))return true;
 return false;
}
bool holes_in(const Expr& expression) {
  if(expression.kind==Expr::Kind::Propagate||expression.kind==Expr::Kind::SourceCsp)return true;
  if((expression.kind>=Expr::Kind::Record&&expression.kind<=Expr::Kind::Length)||expression.kind==Expr::Kind::PatternMatch||expression.kind==Expr::Kind::Loop)return true;
 if(expression.kind==Expr::Kind::Lambda){
   if(has_type_holes(expression.result)||expression.result.has_foralls()||holes_in(expression.body))return true;
   for(const auto& parameter:expression.parameters)if(has_type_holes(parameter.type)||parameter.type.has_foralls())return true;
 }
 if(expression.callee&&holes_in(*expression.callee))return true;
 for(const auto& child:expression.children)if(holes_in(*child))return true;
 return false;
}
bool needs_inference(const Unit& unit) {
 if(unit.forall_elaborated)return false;
 if(!unit.nominals.empty())return true;
 for(const auto& function:unit.functions) {
    if(has_type_holes(function.result)||function.result.has_foralls()||function.result.has_names()||holes_in(function.body))return true;
    for(const auto& parameter:function.params)if(has_type_holes(parameter.type)||parameter.type.has_foralls()||parameter.type.has_names())return true;
 }
 return false;
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
 };
 struct Scheme;
 struct Global {
  Term signature;bool external;Function* definition;
  Scheme* scheme=nullptr;unsigned state=0;std::size_t index=0,low=0;bool on_stack=false;
  Stmt* source=nullptr;std::size_t first_slot=0,last_slot=0;
  std::vector<std::size_t> declaration_slots;
 };
 struct Slot { Type* output; Term term; Location where; std::string description; bool generic=false; };
 struct Ground { Type type; std::size_t identity, depth, size; };
  Unit& unit;
  NominalCatalog catalog;
 const InferenceLimits& limits;
 std::size_t work = 0;
 std::vector<Node> nodes;
 std::map<Type::Kind,Term> scalars;
 std::map<Term,Type> atoms;
 std::map<Type,Term> atom_terms;
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
     std::size_t next_identity = Type::ByteReceiver + 1;
   std::map<std::vector<std::string>,std::size_t> label_identities;
   std::map<std::string,std::size_t> nominal_identities;

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
  Term annotation(const Type& type,const Location& where,const std::vector<Term>* variables=nullptr,std::size_t depth=0) {
   tick(where);
    if(depth>limits.depth)fail(where,"nominal/type annotation depth limit exceeded");
    if(type==Type::Hole)return fresh(where);
    if(type.kind()==Type::Bound){if(!variables||type.generic_identity()>=variables->size())fail(where,"quantified type variable outside forall/declaration");return (*variables)[type.generic_identity()];}
    if(type.kind()==Type::Named||!type.nominal().empty()){
     const auto& d=catalog.find(type.nominal(),where);
     if(d.parameters.size()!=type.arguments().size())fail(where,"wrong type argument arity for "+d.name);
     std::vector<Term> args,fields;
     for(const auto& a:type.arguments())args.push_back(annotation(a,where,variables,depth+1));
     for(const auto& f:d.fields)fields.push_back(annotation(f,where,&args,depth+1));
     fields.insert(fields.end(),args.begin(),args.end());
     return data(d.kind,std::move(fields),d.labels,0,where,d.name,args.size());
    }
    if(type.is_code())return add(Type::Code,{},annotation(type.code_result(),where,variables,depth+1),where);
    if(type.is_forall()||type.kind()==Type::Generic){
     auto resolved=type.is_forall()?catalog.resolve(type,where):type;
     if(auto found=atom_terms.find(resolved);found!=atom_terms.end())return found->second;
     auto id=add(type.kind(),{},0,where);atoms.emplace(id,resolved);atom_terms.emplace(std::move(resolved),id);return id;
   }
   if(type.is_tuple()){
    std::vector<Term> elements;for(const auto& element:type.elements())elements.push_back(annotation(element,where,variables,depth+1));
   return add(Type::Tuple,std::move(elements),0,where);
  }
   if(type.is_data()){
     std::vector<Term> elements;for(const auto& element:type.elements())elements.push_back(annotation(element,where,variables,depth+1));
    return data(type.kind(),std::move(elements),type.labels(),type.extent(),where);
   }
   if(!type.is_function())return scalar(type.kind(),where);
  std::vector<Term> parameters;
   for(const auto& parameter:type.parameters())parameters.push_back(annotation(parameter,where,variables,depth+1));
   auto result=annotation(type.result(),where,variables,depth+1);
  return function(std::move(parameters),result,where);
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
   if(auto atom=atoms.find(id);atom!=atoms.end())return type_name(atom->second);
  bool tuple=nodes[id].kind==Type::Tuple;
   bool aggregate=data_kind(nodes[id].kind);
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
    if(nodes[a].kind!=nodes[b].kind)
     fail(where,"expected "+describe(b,where)+", got "+describe(a,where));
    if(atoms.contains(a)||atoms.contains(b)){
     if(!atoms.contains(a)||!atoms.contains(b)||atoms.at(a)!=atoms.at(b))
      fail(where,"incompatible quantified types: expected "+describe(b,where)+", got "+describe(a,where));
     continue;
    }
     if(!compound(nodes[a].kind))continue;
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
 static bool value(const Expr& e){
  if(e.kind==Expr::Kind::Quote)return e.children.size()==1&&e.children[0]->kind==Expr::Kind::Call&&e.children[0]->children.empty();
  if(e.kind==Expr::Kind::Lambda||e.kind==Expr::Kind::Variable||e.kind==Expr::Kind::Integer||e.kind==Expr::Kind::Float||e.kind==Expr::Kind::Boolean||e.kind==Expr::Kind::String)return true;
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
    if(nodes[a].parameters.size()!=nodes[b].parameters.size()){unify(b,a,at);continue;}
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
  for(auto& child:children)child=copy_signature(child,variables,copies,at,depth+1);
  if(has_result(kind))result=copy_signature(result,variables,copies,at,depth+1);
    auto id=data_kind(kind)?data(kind,std::move(children),std::move(labels),extent,at,std::move(nominal),arguments):add(kind,std::move(children),result,at);copies.emplace(term,id);return id;
 }
 void bind_pattern(const Pattern& pattern,Term value){
  tick(pattern.where);
  if(!pattern.is_tuple()){define(pattern.name,value,pattern.where);return;}
  std::vector<Term> elements;
  for(std::size_t i=0;i<pattern.elements.size();++i)elements.push_back(fresh(pattern.where));
  auto shape=add(Type::Tuple,elements,0,pattern.where);unify(value,shape,pattern.where);
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
  lambda->parameters=f.params;lambda->result=f.result;
  for(const auto& s:f.body)lambda->body.push_back(clone_statement(s,ast_budget));
  source->value=std::move(lambda);
  global.source=source.get();global_sources.push_back(std::move(source));
   auto saved_scopes=std::move(scopes);auto saved_returns=returns;bool saved_saw=saw_return;auto saved_loops=std::move(loop_values);loop_values.clear();
  scopes.emplace_back();auto signature=global.signature;
  for(std::size_t i=0;i<f.params.size();++i){
   const auto& p=f.params[i];auto type=nodes[signature].parameters[i];
   if(p.pattern)bind_pattern(*p.pattern,type);else define(p.name,type,p.where);
  }
  returns=nodes[signature].result;saw_return=false;global.first_slot=slots.size();block(f.body,false);scopes.pop_back();global.last_slot=slots.size();
  if(!saw_return)unify(returns,scalar(Type::Void,f.where),f.where);
  if(global.low==global.index){
   std::vector<std::string> group;
   do{group.push_back(component_stack.back());component_stack.pop_back();globals.at(group.back()).on_stack=false;}while(group.back()!=name);
   std::sort(group.begin(),group.end());
   if(std::find(group.begin(),group.end(),"main")==group.end()){
    returns=scalar(Type::Void,f.where);generalizing_globals={group.begin(),group.end()};
    bool any=false;std::map<std::string,std::set<Term>> variables;
    for(const auto& member:group){variables[member]=generalizable(globals.at(member).signature,f.where);any=any||!variables[member].empty();}
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
   scopes=std::move(saved_scopes);returns=saved_returns;saw_return=saved_saw;loop_values=std::move(saved_loops);
  global.state=2;global_stack.pop_back();
  if(!global_stack.empty()){
   auto& caller=globals.at(global_stack.back());caller.low=std::min(caller.low,global.low);
  }
 }
 Term instantiate_global(Scheme& scheme,Expr& use){
  // Allocate every sibling signature before inferring any body. Recursive reads
  // resolve through these monomorphic overrides, not fresh scheme instances.
  auto saved=std::move(scopes);scopes.emplace_back();
  struct Member {Global* global;std::unique_ptr<Stmt> instance;Term signature;};
  std::vector<Member> members;
  std::set<Term> variables;std::map<Term,Term> copies;
  for(const auto& name:*scheme.group){const auto& member=*globals.at(name).scheme;variables.insert(member.variables.begin(),member.variables.end());}
  for(const auto& name:*scheme.group){
   auto& global=globals.at(name);
   if(instance_serial>=limits.specializations)fail(use.where,"global specialization limit exceeded");
   auto instance=std::make_unique<Stmt>();instance->kind=Stmt::Kind::Let;instance->where=use.where;
   instance->name="@let_"+std::to_string(++instance_serial);
   instance->value=clone_expression(*global.scheme->source,ast_budget);source_expression(*instance->value);
   auto signature=copy_signature(global.signature,variables,copies,use.where);
   define(name,signature,use.where);scopes.back().at(name).runtime_name=instance->name;
   members.push_back({&global,std::move(instance),signature});
  }
  for(auto& member:members){
   auto type=expression(*member.instance->value,member.signature);unify(type,member.signature,use.where);
   share_weak(member.global->signature,type,*member.global->scheme,use.where);
  }
  Term result=0;auto use_slot=slots.size();
  for(auto& member:members){
   if(member.global->scheme==&scheme){result=member.signature;renames.emplace_back(&use,member.instance->name);}
   instances.push_back({member.instance.get(),use_slot});
   member.global->source->instances.push_back(std::move(member.instance));
  }
  scopes=std::move(saved);return result;
 }
 Term name(Expr& expression,bool call_position=false) {
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
      type=instantiate_global(*found->scheme,expression);--specialization_depth;
      slots.push_back({&expression.type,type,expression.where,"global instantiation"});return type;
     }
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
    else if(!found->runtime_name.empty())renames.emplace_back(&expression,found->runtime_name);
  }
  else if(auto found=globals.find(expression.text);found!=globals.end()) {
   if(found->second.external&&!call_position)
    fail(expression.where,"raw extern functions cannot be used as values; write an On1x wrapper");
   type=found->second.signature;
  } else {
   if(is_intrinsic_name(expression.text))
    fail(expression.where,"intrinsic is not a first-class value; write an On1x wrapper: "+expression.text);
   fail(expression.where,(call_position?"unknown function: ":"undefined variable: ")+expression.text);
  }
  slots.push_back({&expression.type,type,expression.where,"expression type"});return type;
 }
 Term builtin(Expr& expression,const std::vector<Term>& args) {
  const auto& n=expression.callee->text;const auto& at=expression.where;
   auto arity=[&](std::size_t count){if(args.size()!=count)fail(at,"wrong arity for "+n);};
   if(auto primitive=data_primitive(n)){
    arity(primitive->parameters().size());for(std::size_t i=0;i<args.size();++i)unify(args[i],annotation(primitive->parameters()[i],at),at);
    return annotation(primitive->result(),at);
   }
  if(n=="println"){arity(1);weak.push_back(args[0]);return scalar(Type::Void,at);}
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
  for(auto& pack:packs)if(pack.expression==&e)pack.expression=old.get();
  e=Expr{};e.where=old->where;e.kind=kind;return old;
 }
 Term project(Expr& e,Term schema){
  auto type=atoms.at(head(schema,e.where));auto instance=instantiate(type,false,e.where);
  auto source=wrap(e,Expr::Kind::PolyProjection);e.children.push_back(std::move(source));
  projections.push_back({&e,schema,instance,slots.size()});
  slots.push_back({&e.type,instance,e.where,"forall instantiation"});return instance;
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
   for(std::size_t i=0;i<parameters.size();++i)expected(*e.children[i],parameters[i]);
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
  if(nodes[head(actual,e.where)].kind==Type::Forall&&nodes[expectation].kind!=Type::Forall&&nodes[expectation].kind!=Type::Hole)
   actual=project(e,actual);
  unify(actual,expectation,e.where);return expectation;
 }
 void reset_ground(){
      ground.assign(nodes.size(),{});state.assign(nodes.size(),0);canonical.clear();next_identity=Type::Named+1;
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
     switch(e.kind) {
      case Expr::Kind::SourceCsp:{
       auto operand=[&](std::size_t i){if(i>=e.children.size()||!e.children[i])fail(at,"missing concurrency operand");return expression(*e.children[i]);};
       if(e.text=="poll"){
        if(e.children.empty()||e.children.size()>17)fail(at,"poll offer limit/arity mismatch");
        std::vector<Type> fields;std::vector<std::string> labels;
        for(std::size_t i=0;i<e.children.size();++i){auto& offer=*e.children[i];
         if(offer.kind==Expr::Kind::Boolean&&offer.text=="true"&&i+1==e.children.size()){
          fields.push_back(Type::data(Type::Record,{}));labels.push_back("Default");break;
         }
         if(offer.kind!=Expr::Kind::SourceCsp||
            (offer.text!="offer_recv"&&offer.text!="offer_send")||offer.children.size()!=(offer.text=="offer_recv"?1u:2u))fail(offer.where,"invalid poll offer");
         offer.type=Type::Void;
         auto endpoint=expression(*offer.children[0]);unify(endpoint,scalar(offer.text=="offer_recv"?Type::Receiver:Type::Sender,offer.where),offer.where);
         if(offer.text=="offer_send")unify(expression(*offer.children[1]),scalar(Type::I64,offer.where),offer.where);
         fields.push_back(offer.text=="offer_recv"?Type{Type::I64}:Type::data(Type::Record,{}));
         labels.push_back((offer.text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
         fields.push_back(Type::data(Type::Record,{}));labels.push_back("Closed"+std::to_string(i));
        }
        result=annotation(Type::data(Type::Sum,std::move(fields),std::move(labels)),at);break;
       }
      if(e.text=="send"){
       auto left=operand(0);auto k=nodes[head(left,at)].kind;
        if(k!=Type::Sender&&k!=Type::ByteSender&&k!=Type::Receiver&&k!=Type::ByteReceiver&&k!=Type::Channel&&k!=Type::Fiber&&k!=Type::ExternalFiber){
        // `<-` used to tokenize as `<` then unary `-`. Keep existing numeric
        // comparisons, including source without whitespace, byte-for-byte.
        auto neg=std::make_unique<Expr>();neg->kind=Expr::Kind::Unary;neg->where=at;neg->text="-";neg->children.push_back(std::move(e.children.at(1)));e.children[1]=std::move(neg);e.kind=Expr::Kind::Binary;e.text="<";
        auto right=operand(1);unify(left,right,at);numeric_operand(left,at);result=scalar(Type::Bool,at);break;
       }
        auto endpoint=k==Type::ByteSender?Type::ByteSender:Type::Sender;
        unify(left,scalar(endpoint,at),at);unify(operand(1),scalar(endpoint==Type::ByteSender?Type::U8:Type::I64,at),at);result=annotation(csp::send_result(),at);
       }else if(e.text=="launch"){unify(operand(0),function({},scalar(Type::I64,at),at),at);result=scalar(Type::Fiber,at);}
       else if(e.text=="launch_exec"){
        if(e.children.empty()||e.children.size()>9)fail(at,"external argv limit/arity mismatch");
        for(std::size_t i=0;i<e.children.size();++i)unify(operand(i),scalar(Type::String,at),at);
        result=scalar(Type::ExternalFiber,at);
       }
       else if(e.text=="launch_capture"){
        if(e.children.empty()||e.children.size()>9)fail(at,"captured shell argv limit/arity mismatch");
        for(std::size_t i=0;i<e.children.size();++i)unify(operand(i),scalar(Type::String,at),at);
        result=add(Type::Tuple,{scalar(Type::ExternalFiber,at),scalar(Type::ByteSender,at),scalar(Type::ByteReceiver,at),scalar(Type::ByteReceiver,at)},0,at);
       }
      else if(e.text=="channel"){for(std::size_t i=0;i<e.children.size();++i)unify(operand(i),scalar(Type::I64,at),at);result=scalar(Type::Channel,at);}
      else if(e.text=="split"){unify(operand(0),scalar(Type::Channel,at),at);result=add(Type::Tuple,{scalar(Type::Sender,at),scalar(Type::Receiver,at)},0,at);}
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
        auto endpoint=operand(0);auto kind=nodes[head(endpoint,at)].kind;
        if(kind!=Type::Receiver&&kind!=Type::ByteReceiver)fail(at,"receive expects receiver<i64> or receiver<u8>");
        result=annotation(csp::receive_result(kind==Type::ByteReceiver?Type::U8:Type::I64),at);
       }
       else if(e.text=="close"){
        auto endpoint=operand(0);auto kind=nodes[head(endpoint,at)].kind;
        if(kind!=Type::Sender&&kind!=Type::ByteSender)fail(at,"close expects sender<i64> or sender<u8>");
        result=scalar(Type::Bool,at);
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
   case Expr::Kind::Variable:return name(e);
    case Expr::Kind::Unit:result=scalar(Type::Void,at);break;
    case Expr::Kind::Loop:{
     result=fresh(at);loop_values.push_back({e.control_target,result});block(e.body);
     auto exits=loop_values.back();loop_values.pop_back();if(exits.bare||!exits.valued)result=scalar(Type::Void,at);break;
    }
   case Expr::Kind::PatternMatch:{
    auto scrutinee=expression(*e.children.at(0));result=fresh(at);bool fallthrough=false;scopes.emplace_back();
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
   if(!saw_return)unify(result_type,scalar(Type::Void,at),at);
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
    }else{unify(left,right,at);numeric_operand(left,at);}
    bool arithmetic=e.text=="+"||e.text=="-"||e.text=="*"||e.text=="/"||e.text=="%";
    result=arithmetic?left:scalar(Type::Bool,at);break;
  }
  case Expr::Kind::Call:{
   auto& callee=*e.callee;
   bool intrinsic=callee.kind==Expr::Kind::Variable&&!local(callee.text)&&is_intrinsic_name(callee.text);
   Term callable=0;
    if(!intrinsic) {
     callable=callee.kind==Expr::Kind::Variable?name(callee,true):expression(callee);
     if(nodes[head(callable,at)].kind==Type::Forall)callable=project(callee,callable);
    auto kind=nodes[head(callable,at)].kind;
    if(kind!=Type::Hole&&kind!=Type::Function)
     fail(callee.where,"cannot call value of type "+describe(callable,at));
   }
    std::vector<Term> args;
    auto target=intrinsic?Term{0}:head(callable,at);
    for(std::size_t i=0;i<e.children.size();++i){
     if(!intrinsic&&nodes[target].kind==Type::Function&&i<nodes[target].parameters.size())
      args.push_back(expected(*e.children[i],nodes[target].parameters[i]));
     else args.push_back(expression(*e.children[i]));
    }
   if(intrinsic){result=builtin(e,args);break;}
   auto root=head(callable,at);
   if(nodes[root].kind==Type::Hole) {
    result=fresh(at);auto shape=function(args,result,at);unify(callable,shape,at);
   } else {
    if(nodes[root].parameters.size()!=args.size())fail(at,"wrong number of arguments to function value");
    for(std::size_t i=0;i<args.size();++i)unify(args[i],nodes[root].parameters[i],e.children[i]->where);
    result=nodes[root].result;
   }
   break;
  }
  }
  slots.push_back({&e.type,result,at,"expression type"});return result;
 }
 void block(std::vector<Stmt>& body,bool own_scope=true) {
  if(own_scope)scopes.emplace_back();
  for(auto& statement:body) {
   const auto& at=statement.where;
   tick(at);
     switch(statement.kind) {
     case Stmt::Kind::TaskGroup:block(statement.body);break;
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
    if(!generalize_pattern(statement,value,first))generalize(statement,value,first);
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
      std::vector<Type> arguments;
      if(!node.nominal.empty()){arguments.assign(parameters.end()-node.arguments,parameters.end());parameters.resize(parameters.size()-node.arguments);}
      auto type=data_kind(node.kind)?Type::data(node.kind,std::move(parameters),node.labels,node.extent):node.kind==Type::Tuple?Type::tuple(std::move(parameters)):node.kind==Type::Code?Type::code(std::move(result_type)):Type::function(std::move(parameters),std::move(result_type));
      if(!node.nominal.empty())type=type.with_nominal(node.nominal,std::move(arguments));
    Ground value{std::move(type),next_identity++,depth,size};
    canonical.emplace(std::move(key),value);ground[id]=std::move(value);
   }
  }
  return *ground[head(term,at)];
 }
public:
  Inference(Unit& source,const InferenceLimits& budget):unit(source),catalog(source),limits(budget),ast_budget(budget.expanded_ast) {}
 void run() {
  // Allocate all declaration variables first: calls, mutual recursion and
  // imported functions constrain the same monotype regardless of body order.
  for(auto& f:unit.functions) {
   if(is_intrinsic_name(f.name))fail(f.where,"intrinsic name cannot be redefined: "+f.name);
   if(globals.contains(f.name))fail(f.where,"duplicate function: "+f.name);
   if(f.external) {
    if(has_type_holes(f.result))fail(f.where,"extern signatures require concrete types; inference is not ABI evidence");
     for(const auto& p:f.params)if(has_type_holes(p.type))
      fail(p.where,"extern signatures require concrete types; inference is not ABI evidence");
     (void)extern_parameter_types(f);
   }
   std::vector<Term> parameters;std::set<std::string> names;
   for(auto& p:f.params) {
    if(!names.insert(p.name).second)fail(p.where,"duplicate parameter: "+p.name);
    auto type=annotation(p.type,p.where);parameters.push_back(type);
    slots.push_back({&p.type,type,p.where,p.pattern?"parameter pattern":"parameter "+p.name});
   }
   auto result=annotation(f.result,f.where);
   slots.push_back({&f.result,result,f.where,"return type of function "+f.name});
     Global global{function(std::move(parameters),result,f.where),f.external,&f,nullptr,0,0,0,false,nullptr,0,0,{}};
    auto count=f.params.size()+1;
    for(std::size_t i=slots.size()-count;i<slots.size();++i)global.declaration_slots.push_back(i);
    globals.emplace(f.name,std::move(global));
  }
   for(auto& f:unit.functions)if(!f.external)ensure_global(f.name);
   elaborate_foralls();reset_ground();
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
   f.params=std::move(lambda.parameters);f.result=lambda.result;f.body=std::move(lambda.body);
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

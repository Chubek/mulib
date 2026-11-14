#include <on1x/infer.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace on1x {
namespace {
bool holes_in(const Expr& expression);
bool holes_in(const std::vector<Stmt>& body) {
 for(const auto& statement:body)
  if(has_type_holes(statement.declared)||(statement.value&&holes_in(*statement.value))||
     holes_in(statement.body)||holes_in(statement.otherwise))return true;
 return false;
}
bool holes_in(const Expr& expression) {
 if(expression.kind==Expr::Kind::Lambda){
  if(has_type_holes(expression.result)||holes_in(expression.body))return true;
  for(const auto& parameter:expression.parameters)if(has_type_holes(parameter.type))return true;
 }
 if(expression.callee&&holes_in(*expression.callee))return true;
 for(const auto& child:expression.children)if(holes_in(*child))return true;
 return false;
}
bool needs_inference(const Unit& unit) {
 for(const auto& function:unit.functions) {
  if(has_type_holes(function.result)||holes_in(function.body))return true;
  for(const auto& parameter:function.params)if(has_type_holes(parameter.type))return true;
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
 };
 struct Global { Term signature; bool external; };
 struct Slot { Type* output; Term term; Location where; std::string description; };
 struct Ground { Type type; std::size_t identity, depth, size; };
 Unit& unit;
 const InferenceLimits& limits;
 std::size_t work = 0;
 std::vector<Node> nodes;
 std::map<Type::Kind,Term> scalars;
 std::map<std::string,Global> globals;
 std::vector<std::map<std::string,Term>> scopes;
 std::vector<Slot> slots;
 Term returns = 0;
 bool saw_return = false;
 std::vector<std::optional<Ground>> ground;
 std::vector<unsigned char> state;
 std::map<std::vector<std::size_t>,Ground> canonical;
 std::size_t next_identity = Type::Hole + 1;

 [[noreturn]] static void fail(const Location& where,const std::string& message) {
  throw Error(where,message);
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
 Term annotation(const Type& type,const Location& where) {
  tick(where);
  if(type==Type::Hole)return fresh(where);
  if(!type.is_function())return scalar(type.kind(),where);
  std::vector<Term> parameters;
  for(const auto& parameter:type.parameters())parameters.push_back(annotation(parameter,where));
  auto result=annotation(type.result(),where);
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
   if(nodes[id].kind==Type::Function) {
    pending.push_back(nodes[id].result);
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
  if(nodes[id].kind!=Type::Function)return type_name(Type(nodes[id].kind));
  std::string text="fn(";
  for(std::size_t i=0;i<nodes[id].parameters.size();++i) {
   if(i)text+=", ";
   if(!budget){text+="...";break;}
   text+=describe(nodes[id].parameters[i],where,budget,depth+1);
  }
  return text+") -> "+describe(nodes[id].result,where,budget,depth+1);
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
   if(nodes[a].kind!=Type::Function)continue;
   if(nodes[a].parameters.size()!=nodes[b].parameters.size())
    fail(where,"incompatible function arity: expected "+describe(b,where)+", got "+describe(a,where));
   auto pair=std::minmax(a,b);
   if(!compared.emplace(pair.first,pair.second).second)continue;
   // Only variables acquire parents. Merging function roots could introduce a
   // cycle before their children have passed the occurs check.
   pending.emplace_back(nodes[a].result,nodes[b].result);
   for(std::size_t i=nodes[a].parameters.size();i>0;--i)
    pending.emplace_back(nodes[a].parameters[i-1],nodes[b].parameters[i-1]);
  }
 }
 void define(const std::string& name,Term type,const Location& where) {
  if(!scopes.back().emplace(name,type).second)fail(where,"duplicate local: "+name);
 }
 std::optional<Term> local(const std::string& name)const {
  for(auto scope=scopes.rbegin();scope!=scopes.rend();++scope)
   if(auto found=scope->find(name);found!=scope->end())return found->second;
  return {};
 }
 Term name(Expr& expression,bool call_position=false) {
  Term type;
  if(auto found=local(expression.text))type=*found;
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
  if(n=="println"){arity(1);return scalar(Type::Void,at);}
  if(n=="assert"){arity(1);unify(args[0],scalar(Type::Bool,at),at);return scalar(Type::Void,at);}
  if(n=="json_parse_or_die") {
   arity(1);unify(args[0],scalar(Type::String,at),at);return scalar(Type::Json,at);
  }
  if(n=="json_get_or_die") {
   arity(2);unify(args[1],scalar(Type::String,at),at);return scalar(Type::JsonView,at);
  }
  if(n=="json_size"||n=="json_number"){arity(1);return scalar(Type::I64,at);}
  if(n=="json_print"){arity(1);return scalar(Type::Void,at);}
  fail(at,"unsupported intrinsic: "+n);
 }
 Term expression(Expr& e) {
  Term result=0;const auto& at=e.where;
  tick(at);
  switch(e.kind) {
  case Expr::Kind::Integer:result=scalar(Type::I64,at);break;
  case Expr::Kind::Boolean:result=scalar(Type::Bool,at);break;
  case Expr::Kind::String:result=scalar(Type::String,at);break;
  case Expr::Kind::Variable:return name(e);
  case Expr::Kind::Lambda:{
   std::vector<Term> parameters;std::set<std::string> names;
   for(auto& parameter:e.parameters){
    if(!names.insert(parameter.name).second)fail(parameter.where,"duplicate parameter: "+parameter.name);
    auto type=annotation(parameter.type,parameter.where);parameters.push_back(type);
    slots.push_back({&parameter.type,type,parameter.where,"anonymous-function parameter "+parameter.name});
   }
   auto result_type=annotation(e.result,at);
   slots.push_back({&e.result,result_type,at,"anonymous-function return type"});
   result=function(parameters,result_type,at);
   auto previous_returns=returns;bool previous_saw_return=saw_return;
   scopes.emplace_back();
   for(std::size_t i=0;i<e.parameters.size();++i)define(e.parameters[i].name,parameters[i],e.parameters[i].where);
   returns=result_type;saw_return=false;block(e.body,false);scopes.pop_back();
   if(!saw_return)unify(result_type,scalar(Type::Void,at),at);
   returns=previous_returns;saw_return=previous_saw_return;break;
  }
  case Expr::Kind::Unary:{
   auto child=expression(*e.children[0]);result=scalar(e.text=="!"?Type::Bool:Type::I64,at);
   unify(child,result,at);break;
  }
  case Expr::Kind::Binary:{
   auto left=expression(*e.children[0]),right=expression(*e.children[1]);
   if(e.text=="=="||e.text=="!=")unify(left,right,at);
   else {
    auto operand=scalar(e.text=="&&"||e.text=="||"?Type::Bool:Type::I64,at);
    unify(left,operand,at);unify(right,operand,at);
   }
   bool arithmetic=e.text=="+"||e.text=="-"||e.text=="*"||e.text=="/"||e.text=="%";
   result=scalar(arithmetic?Type::I64:Type::Bool,at);break;
  }
  case Expr::Kind::Call:{
   auto& callee=*e.callee;
   bool intrinsic=callee.kind==Expr::Kind::Variable&&!local(callee.text)&&is_intrinsic_name(callee.text);
   Term callable=0;
   if(!intrinsic) {
    callable=callee.kind==Expr::Kind::Variable?name(callee,true):expression(callee);
    auto kind=nodes[head(callable,at)].kind;
    if(kind!=Type::Hole&&kind!=Type::Function)
     fail(callee.where,"cannot call value of type "+describe(callable,at));
   }
   std::vector<Term> args;for(auto& child:e.children)args.push_back(expression(*child));
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
   case Stmt::Kind::Let:{
    auto value=expression(*statement.value);
    if(statement.declared!=Type::Invalid) {
     auto declared=annotation(statement.declared,at);unify(value,declared,at);
     slots.push_back({&statement.declared,declared,at,"annotation for "+statement.name});
    }
    define(statement.name,value,at);break;
   }
   case Stmt::Kind::Assign:{
    auto binding=local(statement.name);
    if(!binding)fail(at,"undefined variable: "+statement.name);
    auto value=expression(*statement.value);unify(value,*binding,at);break;
   }
   case Stmt::Kind::Return:{
    auto value=statement.value?expression(*statement.value):scalar(Type::Void,at);
    unify(value,returns,at);saw_return=true;break;
   }
   case Stmt::Kind::If:case Stmt::Kind::While:{
    auto condition=expression(*statement.value);unify(condition,scalar(Type::Bool,at),at);
    block(statement.body);block(statement.otherwise);break;
   }
   case Stmt::Kind::Block:block(statement.body);break;
   case Stmt::Kind::Expr:expression(*statement.value);break;
   }
  }
  if(own_scope)scopes.pop_back();
 }
 const Ground& resolve(Term term,const Location& at,const std::string& description) {
  struct Frame { Term term; bool finish; };
  std::vector<Frame> pending{{term,false}};
  while(!pending.empty()) {
   auto frame=pending.back();pending.pop_back();auto id=head(frame.term,at);
   if(ground[id])continue;
   auto& node=nodes[id];
   if(node.kind==Type::Hole)fail(at,"cannot infer "+description+"; add a concrete type annotation or use");
   if(node.kind!=Type::Function) {
    ground[id]=Ground{Type(node.kind),static_cast<std::size_t>(node.kind),1,1};continue;
   }
   if(!frame.finish) {
    if(state[id])fail(at,"infinite type during inference resolution");
    state[id]=1;pending.push_back({id,true});pending.push_back({node.result,false});
    for(auto parameter:node.parameters)pending.push_back({parameter,false});
    continue;
   }
   auto result=*ground[head(node.result,at)];
   auto depth=result.depth+1,size=result.size+1;
   std::vector<Type> parameters;std::vector<std::size_t> key{result.identity};
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
    Ground value{Type::function(std::move(parameters),result.type),next_identity++,depth,size};
    canonical.emplace(std::move(key),value);ground[id]=std::move(value);
   }
  }
  return *ground[head(term,at)];
 }
public:
 Inference(Unit& source,const InferenceLimits& budget):unit(source),limits(budget) {}
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
   }
   std::vector<Term> parameters;std::set<std::string> names;
   for(auto& p:f.params) {
    if(!names.insert(p.name).second)fail(p.where,"duplicate parameter: "+p.name);
    auto type=annotation(p.type,p.where);parameters.push_back(type);
    slots.push_back({&p.type,type,p.where,"parameter "+p.name});
   }
   auto result=annotation(f.result,f.where);
   slots.push_back({&f.result,result,f.where,"return type of function "+f.name});
   globals.emplace(f.name,Global{function(std::move(parameters),result,f.where),f.external});
  }
  for(auto& f:unit.functions) {
   if(f.external)continue;
   auto signature=globals.at(f.name).signature;scopes.emplace_back();
   for(std::size_t i=0;i<f.params.size();++i)define(f.params[i].name,nodes[signature].parameters[i],f.params[i].where);
   returns=nodes[signature].result;saw_return=false;block(f.body,false);scopes.pop_back();
   if(!saw_return)unify(returns,scalar(Type::Void,f.where),f.where);
  }
  ground.resize(nodes.size());state.resize(nodes.size());
  // Freeze transactionally: errors must not leave a partially grounded unit.
  std::vector<Type> resolved;resolved.reserve(slots.size());
  for(const auto& slot:slots)resolved.push_back(resolve(slot.term,slot.where,slot.description).type);
  for(const auto& f:unit.functions)
   resolve(globals.at(f.name).signature,f.where,"signature of function "+f.name);
  for(std::size_t i=0;i<slots.size();++i)*slots[i].output=std::move(resolved[i]);
 }
};
}

void infer_types(Unit& unit,const InferenceLimits& limits) {
 if(needs_inference(unit))Inference(unit,limits).run();
}
}

#include <on1x/partial_eval.hpp>
#include <on1x/numeric.hpp>
#include "../src/backend/checked.hpp"
#include <algorithm>
#include <map>
#include <optional>

namespace on1x::processor {
namespace {
using E=Expr::Kind;
using Op=mir::Op;
using Value=std::optional<std::int64_t>;
bool scalar(const Type& t){return t==Type::I64||t==Type::Bool;}
Value literal(const Expr& e){
 if(e.kind==E::Integer&&e.type==Type::I64)return integer_literal_bits(e.text,e.where);
 if(e.kind==E::Boolean&&e.type==Type::Bool)return e.text=="true";
 return {};
}
ExprPtr node(E kind,const Expr& origin,std::string text){
 auto e=std::make_unique<Expr>();
 e->kind=kind;e->where=origin.where;e->type=origin.type;e->text=std::move(text);
 return e;
}
ExprPtr constant(const Expr& origin,std::int64_t v){
 return origin.type==Type::Bool?node(E::Boolean,origin,v?"true":"false"):
                                node(E::Integer,origin,std::to_string(v));
}
// Only derive a constant when the corresponding checked runtime operation
// returns. A trapping result is residual code, never a compile-time error.
Value compute(const Expr& e,std::int64_t a,std::int64_t b){
 const auto& op=e.text;
 if(e.kind==E::Unary&&op=="!")return !a;
 if(e.kind==E::Binary){
  if(op=="==")return a==b;
  if(op=="!=")return a!=b;
  if(op=="<")return a<b;
  if(op=="<=")return a<=b;
  if(op==">")return a>b;
  if(op==">=")return a>=b;
  if(op=="&&")return a&&b;
  if(op=="||")return a||b;
 }
 Op check;
 if(op=="-")check=e.kind==E::Unary?Op::Neg:Op::Sub;
 else if(op=="+")check=Op::Add;
 else if(op=="*")check=Op::Mul;
 else if(op=="/")check=Op::Div;
 else if(op=="%")check=Op::Mod;
 else return {};
 const auto result=mir::detail::arithmetic(check,a,b);
 if(result.trap)return {};
 return result.value;
}

// In a speculative result, the only nonconstant leaf permitted is one hole
// for a dynamic argument. It never escapes into checked HIR as a variable.
struct Term {
 ExprPtr tree;
 unsigned holes=0,nodes=0;
};
struct Argument {std::string_view name;Value value;};
struct Evaluator {
 PartialEvalStats stats;
 const std::size_t limit;
 std::map<std::string_view,const Function*,std::less<>> functions;
 static constexpr std::size_t max_work=262144,max_depth=128,max_trial=256,max_args=16;
 bool tick(){
  if(stats.work==limit){stats.limited=true;return false;}
  ++stats.work;return true;
 }
 explicit Evaluator(const Unit& unit,std::size_t work):limit(std::min(work,max_work)){
  stats.enabled=true;
  for(const auto& f:unit.functions){
   if(!tick())break;
   if(f.external||f.prototype||!f.kernel_symbol.empty()||f.extension_generated||
      !f.attributes.empty()||!scalar(f.result)||f.body.empty()||f.body.size()>16||f.params.size()>max_args)continue;
   const auto& ret=f.body.back();
   if(ret.kind!=Stmt::Kind::Return||ret.prototype||!ret.value)continue;
   if(!std::all_of(f.body.begin(),f.body.end()-1,[](const Stmt& s){
        return s.kind==Stmt::Kind::Let&&!s.mutable_binding&&!s.prototype&&!s.pattern&&s.value&&scalar(s.value->type);
      }))continue;
   if(std::all_of(f.params.begin(),f.params.end(),[](const auto& p){
        return scalar(p.type)&&p.borrow==BorrowMode::Value&&!p.pattern;
      }))functions.emplace(f.name,&f);
  }
 }
 const Function* target(const Expr& e) const {
  if(e.kind!=E::Call||!e.callee||e.callee->kind!=E::Variable||
     e.callee->reference!=Expr::Reference::Function||e.callee->prototype)return nullptr;
  const auto it=functions.find(e.callee->text);
  return it!=functions.end()&&it->second->params.size()==e.children.size()?it->second:nullptr;
 }
 // Each attempt has bounded work independent of source-level recursion and
 // consumes the shared whole-unit budget as well. No function bodies execute.
 struct Trial {
  Evaluator& pe;
  std::size_t visits=0;
  std::vector<const Function*> stack;
  Term body(const Function& f,std::vector<Argument> env,unsigned depth){
   if(stack.size()>=8){pe.stats.limited=true;return {};}
   if(std::find(stack.begin(),stack.end(),&f)!=stack.end())return {};
   stack.push_back(&f);
   // Only immutable locals with known, successful initializers. In
   // particular, an unused local can still trap or call an effectful function.
   for(auto it=f.body.begin();it!=f.body.end()-1;++it){
    auto init=interpret(*it->value,env,depth+1);
    if(!init.tree||init.holes){stack.pop_back();return {};}
    env.push_back({it->name,literal(*init.tree)});
   }
   auto result=interpret(*f.body.back().value,env,depth);
   stack.pop_back();return result;
  }
  Term interpret(const Expr& e,const std::vector<Argument>& env,unsigned depth){
   if(++visits>max_trial||depth>=32){pe.stats.limited=true;return {};}
   if(!pe.tick()||!scalar(e.type)||e.prototype||e.noreturn||!e.proofs.empty())return {};
   if(auto n=literal(e))return {constant(e,*n),0,1};
   if(e.kind==E::Variable&&e.reference==Expr::Reference::Local){
    for(auto i=env.rbegin();i!=env.rend();++i)if(const auto& a=*i;a.name==e.text){
     if(a.value)return {constant(e,*a.value),0,1};
     return {node(E::Variable,e,{}),1,1};
    }
    return {};
   }
   if(auto f=pe.target(e)){
    // Nested calls only on fully known arguments. Never execute a dynamic
    // callee, recurse indefinitely, or synthesize a new function identity.
    std::vector<Argument> args;
    for(std::size_t i=0;i<e.children.size();++i){
     auto child=interpret(*e.children[i],env,depth+1);
     if(!child.tree||child.holes)return {};
     args.push_back({f->params[i].name,literal(*child.tree)});
    }
    return body(*f,args,depth+1);
   }
   if(e.kind!=E::Unary&&e.kind!=E::Binary)return {};
   const bool binary=e.kind==E::Binary;
   if(e.children.size()!=(binary?2u:1u))return {};
   auto a=interpret(*e.children[0],env,depth+1);
   if(!a.tree)return {};
   Term b;
   if(binary){
    const bool logical=e.text=="&&"||e.text=="||";
    const auto lhs=logical?literal(*a.tree):Value{};
    // A decisive left operand skips the RHS and any hole it may contain.
    // specialize() still requires the dynamic argument to survive exactly
    // once, since calls evaluate their arguments strictly.
    if(lhs&&(e.text=="&&"?!*lhs:bool(*lhs)))return {constant(e,*lhs),0,1};
    b=interpret(*e.children[1],env,depth+1);
    if(!b.tree||a.holes+b.holes>1)return {};
    // Never move a strict argument underneath a potentially skipped RHS.
    if(b.holes&&logical&&!lhs)return {};
   }
   if(!a.holes&&!b.holes){
    const auto n=compute(e,*literal(*a.tree),binary?*literal(*b.tree):0);
    return n?Term{constant(e,*n),0,1}:Term{};
   }
   auto tree=node(e.kind,e,e.text);
   tree->children.push_back(std::move(a.tree));
   if(binary)tree->children.push_back(std::move(b.tree));
   return {std::move(tree),a.holes+b.holes,1+a.nodes+b.nodes};
  }
 };
 static void plug(ExprPtr& term,ExprPtr& operand){
  if(term->kind==E::Variable){term=std::move(operand);return;}
  for(auto& c:term->children)if(operand)plug(c,operand);
 }
 void specialize(ExprPtr& expr,const Function& f,unsigned depth,unsigned child_height){
  const auto& e=*expr;
  std::vector<Argument> args;
  std::size_t dyn=e.children.size();
  for(std::size_t i=0;i<e.children.size();++i){
   auto n=literal(*e.children[i]);
   if(!n){
    if(dyn!=e.children.size())return;
    dyn=i;
   }
   args.push_back({f.params[i].name,n});
  }
  const bool mixed=dyn!=e.children.size();
  if(mixed&&args.size()==1)return; // No static argument at this call.
  Trial trial{*this,0,{}};
  auto replacement=trial.body(f,args,0);
  if(!replacement.tree||replacement.holes!=unsigned(mixed))return;
  if(mixed){
   // No growth in total nodes; conservatively cap the possible depth after
   // moving (not cloning) the captured dynamic operand into the residual.
   if(replacement.nodes>e.children.size()+2||
      depth+child_height+replacement.nodes>=max_depth)return;
   plug(replacement.tree,expr->children[dyn]);++stats.residual_calls;
  }else{
   replacement.tree->where=e.where;++stats.static_calls;
  }
  expr=std::move(replacement.tree);
 }
 unsigned expression(ExprPtr& expr,unsigned depth){
  if(!expr)return 0;
  if(depth>=max_depth){stats.limited=true;return max_depth;}
  if(!tick()||expr->prototype)return max_depth;
  auto& e=*expr;unsigned height=expression(e.callee,depth+1);
  for(auto& c:e.children){
   if(stats.work==limit){stats.limited=true;return max_depth;}
   height=std::max(height,expression(c,depth+1));
  }
  height=std::max(height,block(e.body,depth+1));
  if(stats.work==limit){stats.limited=true;return max_depth;}
  if(!scalar(e.type)||e.noreturn||!e.proofs.empty())return height+1;
  if(auto f=target(e)){
   specialize(expr,*f,depth,height);
   // Conservative height for nested mixed calls after plugging.
   if(expr->kind!=E::Call)return literal(*expr)?1:max_depth;
  }else if((e.kind==E::Unary&&e.children.size()==1)||(e.kind==E::Binary&&e.children.size()==2)){
   const auto a=literal(*e.children[0]);
   const auto b=e.children.size()==1?Value(0):literal(*e.children[1]);
   if(a&&b)if(auto v=compute(e,*a,*b)){
    expr=constant(e,*v);++stats.folds;return 1;
   }
  }
  return height+1;
 }
 unsigned block(std::vector<Stmt>& body,unsigned depth){
  unsigned height=0;
  for(auto& s:body){
   if(depth>=max_depth||!tick()){stats.limited=true;return max_depth;}
   if(s.prototype||s.kind==Stmt::Kind::Template)continue;
   height=std::max({height,1+expression(s.value,depth+1),1+block(s.body,depth+1),
                    1+block(s.otherwise,depth+1)});
  }
  return height;
 }
};
}
PartialEvalStats partial_evaluate(Unit& candidate,const Unit& source,std::size_t work){
 Evaluator eval(source,work);
 for(auto& f:candidate.functions){
  if(!eval.tick())break;
  if(!f.external&&!f.prototype)eval.block(f.body,0);
 }
 return eval.stats;
}
}

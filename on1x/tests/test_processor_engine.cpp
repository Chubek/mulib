#include <on1x/processor.hpp>
#include <on1x/partial_eval.hpp>
#include <on1x/clone.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
using namespace on1x;
using E=Expr::Kind;
using S=Stmt::Kind;
void require(bool value,const char* message) {
  if(!value) {std::cerr<<"processor engine: "<<message<<'\n';std::exit(1);}
}
Location at(std::size_t line=1) {return {"processor-engine.on1x",line*8,line,2};}
ExprPtr leaf(E kind,Type type,std::string text,std::size_t line=1) {
  auto e=std::make_unique<Expr>();e->kind=kind;e->type=type;
  e->text=std::move(text);e->where=at(line);return e;
}
ExprPtr local(std::string name,Type type,std::size_t line=2) {
  auto e=leaf(E::Variable,type,std::move(name),line);
  e->reference=Expr::Reference::Local;return e;
}
ExprPtr binary(std::string op,Type type,ExprPtr left,ExprPtr right) {
  auto e=leaf(E::Binary,type,std::move(op));
  e->children.push_back(std::move(left));e->children.push_back(std::move(right));
  return e;
}
ExprPtr unary(ExprPtr child) {
  auto e=leaf(E::Unary,Type::Bool,"!");
  e->children.push_back(std::move(child));return e;
}
Param param(std::string name,Type type,std::size_t line=1) {
  Param p;p.name=std::move(name);p.type=type;p.where=at(line);return p;
}
Unit unit(ExprPtr expression,std::vector<Param> params={}) {
  Unit u;Function f;f.name="compute";f.where=at();
  f.result=expression->type;f.params=std::move(params);
  Stmt s;s.kind=S::Return;s.where=at();s.value=std::move(expression);
  f.body.push_back(std::move(s));u.functions.push_back(std::move(f));return u;
}
Expr& result(Unit& u) {return *u.functions.at(0).body.at(0).value;}
void expr_dump(std::ostream&,const Expr&);
void body_dump(std::ostream& out,const std::vector<Stmt>& body) {
  for(const auto& s:body) {
    out<<'['<<int(s.kind)<<':'<<s.name<<':'<<s.context<<':'<<s.control_target;
    if(s.value)expr_dump(out,*s.value);
    body_dump(out,s.body);body_dump(out,s.otherwise);out<<']';
  }
}
void expr_dump(std::ostream& out,const Expr& e) {
  out<<'('<<int(e.kind)<<':'<<e.text<<':'<<type_name(e.type)<<':'
     <<e.where.file<<':'<<e.where.offset<<':'<<e.where.line<<':'<<e.where.column
     <<':'<<e.context<<':'<<int(e.reference)<<':'<<e.move_out;
  if(e.callee)expr_dump(out,*e.callee);
  for(const auto& child:e.children)expr_dump(out,*child);
  body_dump(out,e.body);out<<')';
}
std::string snapshot(Unit& u) {
  std::ostringstream out;for(const auto& f:u.functions)body_dump(out,f.body);
  return out.str();
}
void limited(Unit& u,processor::Limits limits) {
  const auto before=snapshot(u);
  bool failed=false;
  try {(void)processor::apply(u,processor::Phase::Canonicalize,limits);}
  catch(const Error& error) {
    failed=true;
    require(error.diagnostic_id=="ON1X-PROCESSOR-LIMIT","distinct limit diagnostic");
    require(error.where.file=="processor-engine.on1x","located limit diagnostic");
  }
  require(failed,"budget exhaustion must reject");
  require(before==snapshot(u),"failed transaction must leave unit intact");
}
}
int main() {
  using namespace on1x;
  using namespace on1x::processor;
  {
    // Exhaust the shared PE fuel at different points, including immediately
    // after the inner expression folds. Only complete residuals can survive.
    auto number=[](const char* n){return leaf(E::Integer,Type::I64,n);};
    auto source=unit(binary("+",Type::I64,
      binary("*",Type::I64,number("3"),number("4")),number("7")));
    const auto original=snapshot(source);
    bool exhausted=false,partial=false,complete=false;
    for(std::size_t fuel=0;fuel<24;++fuel){
      std::size_t budget=128;auto candidate=clone_unit(source,budget);
      const auto pe=partial_evaluate(candidate,source,fuel);
      require(pe.work<=fuel&&snapshot(source)==original,"PE respects fuel and immutable input");
      const auto& e=result(candidate);
      if(pe.folds==0){exhausted|=pe.limited;require(snapshot(candidate)==original,"PE unchanged residual");}
      else if(pe.folds==1){partial|=pe.limited;
        require(e.kind==E::Binary&&e.children[0]->text=="12"&&e.children[1]->text=="7","PE prefix residual");}
      else {complete=true;require(pe.folds==2&&e.kind==E::Integer&&e.text=="19","PE complete result");}
    }
    require(exhausted&&partial&&complete,"PE deterministic finite-fuel boundary coverage");
  }
  {
    auto x=local("x",Type::I64,7);x->context=891;
    auto u=unit(binary("+",Type::I64,
      leaf(E::Integer,Type::I64,"0"),
      binary("*",Type::I64,std::move(x),leaf(E::Integer,Type::I64,"1"))));
    auto stats=apply(u,Phase::Canonicalize);
    require(stats.rewrites==2,"nested integer identities run compiled rules");
    require(result(u).kind==E::Variable&&result(u).text=="x","capture replacement");
    require(result(u).context==891&&result(u).where.line==7&&
            result(u).reference==Expr::Reference::Local,"capture hygiene and origin preserved");
    auto before=snapshot(u);
    require(apply(u,Phase::Canonicalize).rewrites==0,"canonicalization fixed point");
    require(before==snapshot(u),"idempotence preserves tree and provenance");
  }
  {
    auto literal=[](bool b){return leaf(E::Boolean,Type::Bool,b?"true":"false");};
    auto x=local("x",Type::Bool,7);x->context=891;
    auto e=binary("&&",Type::Bool,literal(true),std::move(x));
    e=binary("&&",Type::Bool,std::move(e),literal(true));
    e=binary("||",Type::Bool,literal(false),std::move(e));
    auto u=unit(binary("||",Type::Bool,std::move(e),literal(false)));
    require(apply(u,Phase::Desugar).rewrites==4,"nested literal identities desugar directly");
    require(result(u).kind==E::Variable&&result(u).text=="x"&&
            result(u).context==891&&result(u).where.line==7,
            "desugar retains the original capture without building Select regions");
  }
  for(bool conjunction:{false,true}) {
    const char* op=conjunction?"&&":"||";
    for(bool left:{false,true})for(bool right:{false,true}) {
      auto u=unit(binary(op,Type::Bool,leaf(E::Boolean,Type::Bool,left?"true":"false"),
                        leaf(E::Boolean,Type::Bool,right?"true":"false")));
      require(apply(u,Phase::Desugar).rewrites==1,"all-literal pair desugars once");
      const bool expected=conjunction?(left&&right):(left||right);
      require(result(u).kind==E::Boolean&&result(u).text==(expected?"true":"false"),
              "all-literal short circuit returns a literal without a region");
    }
    for(bool call_first:{false,true}) {
      auto call=leaf(E::Call,Type::Bool,"",8);
      call->callee=local("mark",Type::function({},Type::Bool));
      auto literal=leaf(E::Boolean,Type::Bool,conjunction?"false":"true");
      auto u=unit(call_first?binary(op,Type::Bool,std::move(call),std::move(literal)):
                             binary(op,Type::Bool,std::move(literal),std::move(call)));
      require(apply(u,Phase::Desugar).rewrites==1&&result(u).kind==E::Region,
              "one annihilating literal cannot erase a captured call");
      auto& branch=result(u).body.at(1);
      const auto& kept=call_first?branch.value:
          (conjunction?branch.body:branch.otherwise).at(0).value;
      require(kept->kind==E::Call&&kept->where.line==8&&kept->callee->text=="mark",
              "call kept as condition or inside the unselected branch");
    }
  }
  for(const auto& op:{"&&","||"}) {
    auto u=unit(binary(op,Type::Bool,local("@processor_1",Type::Bool,4),
                       local("right",Type::Bool,9)),
                {param("@processor_1",Type::Bool,4),param("right",Type::Bool,9)});
    canonical::verify_control(u);canonical::verify_values(u);
    require(apply(u,Phase::Desugar).rewrites==1,"logical expression is desugared");
    auto& e=result(u);
    require(e.kind==E::Region&&e.body.size()==2,"Select lowers to canonical region");
    require(e.where.line==1&&e.body[0].where.file==e.where.file,"generated origin retained");
    require(e.body[0].name=="@processor_2","fresh private name avoids existing identity");
    auto& branch=e.body[1];
    require(branch.kind==S::If&&branch.value->text=="@processor_1"&&
            branch.value->where.line==4,"condition evaluated once at original origin");
    auto& selected=std::string(op)=="&&"?branch.body:branch.otherwise;
    require(selected[0].value->text=="right"&&selected[0].value->where.line==9,
            "right operand retained only in selected branch");
    canonical::verify_control(u);canonical::verify_values(u);
    auto before=snapshot(u);
    require(apply(u,Phase::Desugar).rewrites==0,"desugaring fixed point");
    require(before==snapshot(u),"desugaring idempotence");
  }
  {
    auto u=unit(unary(unary(unary(leaf(E::Boolean,Type::Bool,"false")))));
    apply(u,Phase::Canonicalize);
    require(result(u).kind==E::Boolean&&result(u).text=="true",
            "authored bool rules reach nested fixed point");
  }
  {
    auto u=unit(binary("<=",Type::Bool,local("left",Type::I64,3),
                      local("right",Type::I64,8)));
    require(apply(u,Phase::Desugar).rewrites==1,"integer order normalization");
    auto& e=result(u);
    require(e.kind==E::Unary&&e.children[0]->text==">"&&
            e.children[0]->children[0]->text=="left"&&
            e.children[0]->children[1]->text=="right","comparison preserves operand order");
  }
  {
    auto u=unit(binary("+",Type::F64,local("x",Type::F64),
                       leaf(E::Float,Type::F64,"0.0")));
    auto before=snapshot(u);
    require(apply(u,Phase::Canonicalize).rewrites==0,"float signed zero is not integer identity");
    require(before==snapshot(u),"float tree unchanged");
  }
  for(const auto& op:{"*","/"}) {
    auto u=unit(binary(op,Type::I64,local("effectful",Type::I64),
                       leaf(E::Integer,Type::I64,"0")));
    require(apply(u,Phase::Canonicalize).rewrites==0,"no effect-erasing/trap-erasing zero law");
  }
  {
    auto u=unit(binary("+",Type::I64,
      binary("*",Type::I64,local("x",Type::I64),leaf(E::Integer,Type::I64,"1")),
      leaf(E::Integer,Type::I64,"0")));
    Limits l;l.rewrites=0;limited(u,l);
    l=Limits{};l.rewrites=1;limited(u,l); // Failure after one successful rewrite.
    l=Limits{};l.steps=1;limited(u,l);
    l=Limits{};l.nodes=3;limited(u,l);
  }
  {
    auto e=local("x",Type::Bool);
    for(int i=0;i<30;++i)e=unary(std::move(e));
    auto u=unit(std::move(e));Limits l;l.depth=8;limited(u,l);
  }
  {
    auto u=unit(unary(unary(local("x",Type::Bool))));
    u.functions[0].prototype=true;
    auto before=snapshot(u);
    require(apply(u,Phase::Canonicalize).rewrites==0,"generic prototypes not rewritten");
    require(before==snapshot(u),"prototype evidence preserved");
  }
  {
    auto u=unit(leaf(E::Boolean,Type::Bool,"true"));
    Pattern p;p.where=at();
    for(int i=0;i<30;++i) {
      Pattern parent;parent.where=at();parent.kind=Pattern::Kind::Tuple;
      parent.elements.push_back(std::move(p));p=std::move(parent);
    }
    auto parameter=param("deep",Type::Bool);parameter.pattern=std::move(p);
    u.functions[0].params.push_back(std::move(parameter));
    Limits l;l.depth=8;limited(u,l);
  }
  std::cout<<"processor engine: compiled rules, provenance, hygiene, idempotence and budgets passed\n";
}

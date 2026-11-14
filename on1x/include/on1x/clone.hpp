#pragma once
#include <on1x/ast.hpp>
namespace on1x {
inline void clone_tick(std::size_t& budget,const Location& at){
 if(!budget)throw Error(at,"specialization AST limit exceeded");
 --budget;
}
ExprPtr clone_expression(const Expr&,std::size_t&);
inline Stmt clone_statement(const Stmt& s,std::size_t& budget){
 clone_tick(budget,s.where);Stmt out;
 out.kind=s.kind;out.where=s.where;out.name=s.name;out.pattern=s.pattern;
 out.declared=s.declared;out.mutable_binding=s.mutable_binding;out.prototype=s.prototype;
 if(s.value)out.value=clone_expression(*s.value,budget);
 for(const auto& child:s.body)out.body.push_back(clone_statement(child,budget));
 for(const auto& child:s.otherwise)out.otherwise.push_back(clone_statement(child,budget));
 for(const auto& child:s.instances)out.instances.push_back(std::make_unique<Stmt>(clone_statement(*child,budget)));
 return out;
}
inline ExprPtr clone_expression(const Expr& e,std::size_t& budget){
 clone_tick(budget,e.where);auto out=std::make_unique<Expr>();
 out->kind=e.kind;out->reference=e.reference;out->where=e.where;out->text=e.text;out->type=e.type;
 out->parameters=e.parameters;out->result=e.result;out->captures=e.captures;out->syntax_depth=e.syntax_depth;
 out->projection=e.projection;out->prototype=e.prototype;
 for(const auto& proof:e.proofs)out->proofs.push_back(clone_expression(*proof,budget));
 if(e.callee)out->callee=clone_expression(*e.callee,budget);
 for(const auto& child:e.children)out->children.push_back(clone_expression(*child,budget));
 for(const auto& statement:e.body)out->body.push_back(clone_statement(statement,budget));
 return out;
}
inline Unit clone_unit(const Unit& unit,std::size_t& budget){
 Unit out;out.imports=unit.imports;out.forall_layouts=unit.forall_layouts;out.forall_elaborated=unit.forall_elaborated;
 for(const auto& f:unit.functions){
  Function copy;copy.name=f.name;copy.where=f.where;copy.params=f.params;copy.result=f.result;copy.external=f.external;copy.prototype=f.prototype;
  for(const auto& statement:f.body)copy.body.push_back(clone_statement(statement,budget));
  out.functions.push_back(std::move(copy));
 }
 return out;
}
inline void source_expression(Expr& e){
 while(e.kind==Expr::Kind::PolyPack||e.kind==Expr::Kind::PolyProjection){
  if(e.kind==Expr::Kind::PolyPack){
   if(e.proofs.empty())throw std::logic_error("missing forall source expression");
   auto source=std::move(e.proofs.front());e=std::move(*source);
  }else{
   auto source=std::move(e.children.at(0));e=std::move(*source);
  }
 }
 e.proofs.clear();e.prototype=false;
 if(e.callee)source_expression(*e.callee);
 for(auto& child:e.children)source_expression(*child);
 auto reset=[&](auto&& self,std::vector<Stmt>& body)->void{
  for(auto& s:body){
   s.instances.clear();s.prototype=false;
   if(s.kind==Stmt::Kind::Template){s.kind=Stmt::Kind::Let;s.body.clear();}
   if(s.value)source_expression(*s.value);
   self(self,s.body);self(self,s.otherwise);
  }
 };
 reset(reset,e.body);
}
}

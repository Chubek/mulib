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
 out.context=s.context;
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
 out->syntax_repeat=e.syntax_repeat;
 out->context=e.context;
 if(e.syntax_tail)out->syntax_tail=clone_expression(*e.syntax_tail,budget);
 for(const auto& proof:e.proofs)out->proofs.push_back(clone_expression(*proof,budget));
 if(e.callee)out->callee=clone_expression(*e.callee,budget);
 for(const auto& child:e.children)out->children.push_back(clone_expression(*child,budget));
 for(const auto& statement:e.body)out->body.push_back(clone_statement(statement,budget));
 return out;
}
inline MacroParameter clone_macro_pattern(const MacroParameter& p,std::size_t& budget){
 clone_tick(budget,p.where);MacroParameter out;out.kind=p.kind;out.where=p.where;out.name=p.name;out.category=p.category;out.repeated=p.repeated;
 if(p.literal)out.literal=clone_expression(*p.literal,budget);
 for(const auto& c:p.elements)out.elements.push_back(clone_macro_pattern(c,budget));
 if(p.tail)out.tail=std::make_unique<MacroParameter>(clone_macro_pattern(*p.tail,budget));
 return out;
}
inline Unit clone_unit(const Unit& unit,std::size_t& budget){
 Unit out;out.imports=unit.imports;out.forall_layouts=unit.forall_layouts;out.forall_elaborated=unit.forall_elaborated;out.ppx=unit.ppx;out.stages_lowered=unit.stages_lowered;
 for(const auto& m:unit.macros){
  Macro copy;copy.name=m.name;copy.where=m.where;copy.ellipsis=m.ellipsis;copy.literals=m.literals;
  for(const auto& r:m.rules){
   MacroRule rule;rule.where=r.where;rule.block=r.block;
   for(const auto& p:r.parameters)rule.parameters.push_back(clone_macro_pattern(p,budget));
   if(r.tail)rule.tail=std::make_unique<MacroParameter>(clone_macro_pattern(*r.tail,budget));
   if(r.expression)rule.expression=clone_expression(*r.expression,budget);
   for(const auto& s:r.body)rule.body.push_back(clone_statement(s,budget));
   copy.rules.push_back(std::move(rule));
  }out.macros.push_back(std::move(copy));
 }
 for(const auto& f:unit.functions){
  Function copy;copy.name=f.name;copy.where=f.where;copy.params=f.params;copy.result=f.result;copy.external=f.external;copy.prototype=f.prototype;
  for(const auto& statement:f.body)copy.body.push_back(clone_statement(statement,budget));
  for(const auto& attribute:f.attributes)copy.attributes.push_back(clone_expression(*attribute,budget));
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

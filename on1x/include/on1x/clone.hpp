#pragma once
#include <on1x/ast.hpp>
namespace on1x {
inline void clone_tick(std::size_t& budget,const Location& at){
 if(!budget)throw Error(at,"specialization AST limit exceeded","ON1X-AST-LIMIT");
 --budget;
}
[[noreturn]] inline void clone_malformed(const Location& at,const char* message){
 throw Error(at,std::string("malformed AST during clone: ")+message,
             "ON1X-AST-MALFORMED");
}
ExprPtr clone_expression(const Expr&,std::size_t&);
Macro clone_macro(const Macro&,std::size_t&);
inline Function clone_function(const Function&,std::size_t&);
inline Pattern clone_pattern(const Pattern& p,std::size_t& budget){
 clone_tick(budget,p.where);Pattern out;
 out.kind=p.kind;out.where=p.where;out.name=p.name;out.context=p.context;
 out.labels=p.labels;out.open=p.open;out.type=p.type;
 for(const auto& c:p.elements)out.elements.push_back(clone_pattern(c,budget));
 return out;
}
inline Stmt clone_statement(const Stmt& s,std::size_t& budget){
  clone_tick(budget,s.where);Stmt out;
  out.kind=s.kind;out.where=s.where;out.name=s.name;if(s.pattern)out.pattern=clone_pattern(*s.pattern,budget);
 out.declared=s.declared;out.mutable_binding=s.mutable_binding;out.prototype=s.prototype;out.ref_store=s.ref_store;
  out.context=s.context;
   out.label=s.label;out.label_context=s.label_context;out.control_target=s.control_target;out.inclusive=s.inclusive;
   out.unsafe_block=s.unsafe_block;
  if(s.value)out.value=clone_expression(*s.value,budget);
  if(s.syntax_operator){
   const auto& d=*s.syntax_operator;
   out.syntax_operator=std::make_unique<OperatorDeclaration>(
    OperatorDeclaration{d.header,clone_function(d.function,budget),d.identity,d.block_local});
  }
 for(const auto& child:s.body)out.body.push_back(clone_statement(child,budget));
 for(const auto& child:s.otherwise)out.otherwise.push_back(clone_statement(child,budget));
 for(const auto& child:s.instances){
  if(!child)clone_malformed(s.where,"null statement instance");
  out.instances.push_back(std::make_unique<Stmt>(clone_statement(*child,budget)));
 }
 return out;
}
inline ExprPtr clone_expression(const Expr& e,std::size_t& budget){
  clone_tick(budget,e.where);auto out=std::make_unique<Expr>();
   out->kind=e.kind;out->reference=e.reference;out->where=e.where;out->text=e.text;out->type=e.type;
   out->operator_lexeme2=e.operator_lexeme2;out->operator_identity=e.operator_identity;
    out->operator_position=e.operator_position;
    out->operator_parse=e.operator_parse;out->operator_grouped=e.operator_grouped;
   out->labels=e.labels;
   for(const auto& p:e.patterns)out->patterns.push_back(clone_pattern(p,budget));
    out->statement_match=e.statement_match;out->sum_match=e.sum_match;out->move_out=e.move_out;out->borrow=e.borrow;out->noreturn=e.noreturn;out->unsafe_region=e.unsafe_region;
 out->parameters=e.parameters;out->type_parameters=e.type_parameters;out->constraints=e.constraints;
 out->cliche_arguments=e.cliche_arguments;out->member_type_arguments=e.member_type_arguments;
 out->cliche_member=e.cliche_member;
 out->result=e.result;out->procedure=e.procedure;out->captures=e.captures;out->syntax_depth=e.syntax_depth;
 out->projection=e.projection;out->prototype=e.prototype;
 out->syntax_repeat=e.syntax_repeat;
  out->context=e.context;
  out->control_target=e.control_target;
 if(e.syntax_tail)out->syntax_tail=clone_expression(*e.syntax_tail,budget);
 for(const auto& m:e.syntax_definitions)out->syntax_definitions.push_back(clone_macro(m,budget));
 for(const auto& proof:e.proofs){
  if(!proof)clone_malformed(e.where,"null universal proof");
  out->proofs.push_back(clone_expression(*proof,budget));
 }
 if(e.callee)out->callee=clone_expression(*e.callee,budget);
 for(const auto& child:e.children){
  if(!child)clone_malformed(e.where,"null expression child");
  out->children.push_back(clone_expression(*child,budget));
 }
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
inline Macro clone_macro(const Macro& m,std::size_t& budget){
  clone_tick(budget,m.where);
  Macro copy;copy.name=m.name;copy.where=m.where;copy.ellipsis=m.ellipsis;copy.literals=m.literals;
  for(const auto& r:m.rules){
   MacroRule rule;rule.where=r.where;rule.block=r.block;
   for(const auto& p:r.parameters)rule.parameters.push_back(clone_macro_pattern(p,budget));
   if(r.tail)rule.tail=std::make_unique<MacroParameter>(clone_macro_pattern(*r.tail,budget));
   if(r.expression)rule.expression=clone_expression(*r.expression,budget);
   for(const auto& s:r.body)rule.body.push_back(clone_statement(s,budget));
   copy.rules.push_back(std::move(rule));
  }return copy;
}
 inline Function clone_function(const Function& f,std::size_t& budget){
   Function copy;copy.name=f.name;copy.where=f.where;copy.params=f.params;
   copy.type_parameters=f.type_parameters;copy.constraints=f.constraints;
   copy.result=f.result;copy.procedure=f.procedure;copy.external=f.external;copy.prototype=f.prototype;
   copy.extension_generated=f.extension_generated;
   copy.meta_canonicalized=f.meta_canonicalized;
   copy.meta_folded_identity=f.meta_folded_identity;
   copy.kernel_symbol=f.kernel_symbol;
   copy.kernel_link_only=f.kernel_link_only;
   for(const auto& statement:f.body)copy.body.push_back(clone_statement(statement,budget));
   for(const auto& attribute:f.attributes){
    if(!attribute)clone_malformed(f.where,"null function attribute");
    copy.attributes.push_back(clone_expression(*attribute,budget));
   }
   return copy;
 }
 inline Unit clone_unit(const Unit& unit,std::size_t& budget){
 Unit out;out.imports=unit.imports;out.forall_layouts=unit.forall_layouts;out.forall_elaborated=unit.forall_elaborated;out.ppx=unit.ppx;out.stages_lowered=unit.stages_lowered;
   out.module_imports=unit.module_imports;
    out.kernel_imports=unit.kernel_imports;
    out.foreign_imports=unit.foreign_imports;
    out.features=unit.features;out.scrutiny=unit.scrutiny;out.extensions=unit.extensions;out.nominals=unit.nominals;
    out.docstrings=unit.docstrings;
    out.cliches=unit.cliches;out.cliches_prepared=unit.cliches_prepared;
    out.inherents_prepared=unit.inherents_prepared;
    out.extension_evidence=unit.extension_evidence;out.source_digests=unit.source_digests;
  for(const auto& implementation:unit.cliche_implementations){
   ClicheImplementation copy;copy.where=implementation.where;copy.name=implementation.name;
   copy.type_parameters=implementation.type_parameters;copy.arguments=implementation.arguments;
   copy.constraints=implementation.constraints;
   for(const auto& member:implementation.members){
    ClicheImplementationMember item;item.kind=member.kind;item.where=member.where;
    item.name=member.name;item.runtime_name=member.runtime_name;item.value_type=member.value_type;
    item.function.name=member.function.name;item.function.where=member.function.where;
     item.function.params=member.function.params;item.function.type_parameters=member.function.type_parameters;
     item.function.constraints=member.function.constraints;item.function.result=member.function.result;
     item.function.procedure=member.function.procedure;
     item.function.external=member.function.external;item.function.prototype=member.function.prototype;
     item.function.extension_generated=member.function.extension_generated;
     item.function.kernel_symbol=member.function.kernel_symbol;
     item.function.kernel_link_only=member.function.kernel_link_only;
     for(const auto& statement:member.function.body)item.function.body.push_back(clone_statement(statement,budget));
     for(const auto& attribute:member.function.attributes){
      if(!attribute)clone_malformed(member.function.where,"null cliche member attribute");
      item.function.attributes.push_back(clone_expression(*attribute,budget));
     }
     if(member.value)item.value=clone_expression(*member.value,budget);
    copy.members.push_back(std::move(item));
   }
   out.cliche_implementations.push_back(std::move(copy));
  }
  for(const auto& implementation:unit.inherent_implementations){
   InherentImplementation copy;copy.where=implementation.where;
   copy.type_parameters=implementation.type_parameters;copy.target=implementation.target;
   for(const auto& member:implementation.members){
    InherentMember item;item.kind=member.kind;item.receiver=member.receiver;
    item.where=member.where;item.name=member.name;item.runtime_name=member.runtime_name;
    item.type_parameters=member.type_parameters;item.value_type=member.value_type;
    item.function.name=member.function.name;item.function.where=member.function.where;
    item.function.params=member.function.params;
    item.function.type_parameters=member.function.type_parameters;
    item.function.constraints=member.function.constraints;
    item.function.result=member.function.result;
    item.function.procedure=member.function.procedure;
    item.function.external=member.function.external;
    item.function.prototype=member.function.prototype;
    item.function.extension_generated=member.function.extension_generated;
    item.function.kernel_symbol=member.function.kernel_symbol;
    item.function.kernel_link_only=member.function.kernel_link_only;
    for(const auto& statement:member.function.body){
     item.function.body.push_back(clone_statement(statement,budget));
    }
    for(const auto& attribute:member.function.attributes){
       if(!attribute)clone_malformed(member.function.where,"null function attribute");
       item.function.attributes.push_back(clone_expression(*attribute,budget));
    }
    if(member.value)item.value=clone_expression(*member.value,budget);
    copy.members.push_back(std::move(item));
   }
   out.inherent_implementations.push_back(std::move(copy));
  }
 for(const auto& m:unit.macros)out.macros.push_back(clone_macro(m,budget));
  for(const auto& f:unit.functions)out.functions.push_back(clone_function(f,budget));
  out.fixity_directives=unit.fixity_directives;
  out.operator_scopes=unit.operator_scopes;out.operator_owners=unit.operator_owners;
  out.operator_entry=unit.operator_entry;
  out.operator_lazy_functions=unit.operator_lazy_functions;
  for(const auto& d:unit.operators){
   clone_tick(budget,d.header.where);
   out.operators.push_back({d.header,clone_function(d.function,budget),d.identity,d.block_local});
  }
 return out;
}
inline void source_expression(Expr& e){
 while(e.kind==Expr::Kind::PolyPack||e.kind==Expr::Kind::PolyProjection){
   if(e.kind==Expr::Kind::PolyPack){
     if(e.proofs.size()!=1||!e.proofs.front())
      clone_malformed(e.where,"missing forall source expression");
    auto source=std::move(e.proofs.front());e=std::move(*source);
   }else{
    if(e.children.size()!=1||!e.children.front())
     clone_malformed(e.where,"missing forall projection source expression");
    auto source=std::move(e.children.front());e=std::move(*source);
   }
 }
 e.proofs.clear();e.prototype=false;
 if(e.callee)source_expression(*e.callee);
 for(auto& child:e.children){
  if(!child)clone_malformed(e.where,"null expression child");
  source_expression(*child);
 }
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

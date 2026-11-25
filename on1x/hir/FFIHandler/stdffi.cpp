#include "directives.hpp"
#include "tables.hpp"
#include <algorithm>

namespace on1x::stdffi {
namespace {
std::string parameter(std::size_t index){return "ffi_arg_"+std::to_string(index);}

class Materializer {
 const Location& where;

 ExprPtr make(Expr::Kind kind,Type type,std::string text={}) const {
  auto e=std::make_unique<Expr>();
  e->kind=kind;e->where=where;e->type=type;e->text=std::move(text);
  return e;
 }

 ExprPtr expression(std::size_t index) const {
  using enum tables::Code;
  const auto& node=tables::nodes.at(index);
  std::string op;
  switch(node.op){
   case Param:{
    auto e=make(Expr::Kind::Variable,Type::I64,parameter(static_cast<std::size_t>(node.immediate)));
    e->reference=Expr::Reference::Local;
    return e;
   }
   case Integer:return make(Expr::Kind::Integer,Type::I64,std::to_string(node.immediate));
   case Bool:return make(Expr::Kind::Boolean,Type::Bool,node.immediate?"true":"false");
   case Lt:op="<";break;
   case Le:op="<=";break;
   case Gt:op=">";break;
   case Ge:op=">=";break;
   case Eq:op="==";break;
   case And:op="&&";break;
   case Or:op="||";break;
   case Add:op="+";break;
   case Sub:op="-";break;
   case If:throw std::logic_error("invalid value-position conditional in compiled FFI recipe");
  }
  auto e=make(Expr::Kind::Binary,node.boolean?Type::Bool:Type::I64,std::move(op));
  // Ordinary checked HIR lowering retains left-to-right, short-circuit
  // evaluation for && and ||, including checked arithmetic in the RHS.
  e->children.push_back(expression(node.a));
  e->children.push_back(expression(node.b));
  return e;
 }

 Stmt result(std::size_t index) const {
  const auto& node=tables::nodes.at(index);
  Stmt s;s.where=where;
  if(node.op==tables::Code::If){
   s.kind=Stmt::Kind::If;s.value=expression(node.a);
   s.body.push_back(result(node.b));
   s.otherwise.push_back(result(node.c));
  }else{
   s.kind=Stmt::Kind::Return;s.value=expression(index);
  }
  return s;
 }

public:
 explicit Materializer(const Location& at):where(at){}

 Function build(const Export& info,const tables::Recipe& recipe) const {
  Function f;
  f.name=info.function;f.where=where;f.result=Type::I64;
  for(std::size_t i=0;i<info.arity;++i)
   f.params.push_back({parameter(i),Type::I64,where,0,std::nullopt,BorrowMode::Value});
  const auto& guard=tables::nodes.at(recipe.guard);
  if(guard.op!=tables::Code::Bool||!guard.immediate){
   Stmt s;s.where=where;s.kind=Stmt::Kind::Expr;
   auto call=make(Expr::Kind::Call,Type::Void);
   call->callee=make(Expr::Kind::Variable,Type::function({Type::Bool},Type::Void),"assert");
   call->callee->reference=Expr::Reference::Intrinsic;
   call->children.push_back(expression(recipe.guard));
   s.value=std::move(call);
   f.body.push_back(std::move(s));
  }
  f.body.push_back(result(recipe.body));
  return f;
 }
};
}

std::span<const Export> exports(){return tables::exports;}
bool has_kernel(std::string_view kernel){
 return std::any_of(tables::exports.begin(),tables::exports.end(),[&](const auto& info){return info.kernel==kernel;});
}
std::optional<Function> materialize(std::string_view kernel,std::string_view member,const Location& where){
 for(std::size_t i=0;i<tables::exports.size();++i){
  const auto& info=tables::exports[i];
  if(info.kernel==kernel&&info.member==member)return Materializer(where).build(info,tables::recipes[i]);
 }
 return std::nullopt;
}

const Export* SelectedLibrary::find(std::string_view member) const {
 for(const auto index:exports_)
  if(tables::exports[index].member==member)return &tables::exports[index];
 return nullptr;
}

std::optional<Function> SelectedLibrary::materialize(std::string_view member,const Location& where) const {
 for(const auto index:exports_)
  if(tables::exports[index].member==member)
   return Materializer(where).build(tables::exports[index],tables::recipes[index]);
 return std::nullopt;
}

bool is_recipe_function(std::string_view name){
 return name.starts_with("@stdffi_")&&
  std::any_of(tables::exports.begin(),tables::exports.end(),[&](const auto& info){return info.function==name;});
}

void attach_directives(mir::Module& module){
 // Map each generated request to the id assigned to the actual lowered
 // function. The materializer never uses requests to change recipe semantics.
 for(const auto& f:module.functions)
  for(std::size_t i=0;i<tables::exports.size();++i){
   if(f.name!=tables::exports[i].function)continue;
   const auto& recipe=tables::recipes[i];
   for(const auto& d:std::span(tables::directives).subspan(recipe.directives_begin,recipe.directives_count))
    module.directives.push_back({std::string(d.backend),std::string(d.target),std::string(d.kind),
                                std::string(d.action),f.id,d.speculative});
   break;
  }
 if(!module.directives.empty())module.version=std::max(module.version,mir::directive_format_version);
}
}

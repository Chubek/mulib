#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/clone.hpp>
#include <iostream>

namespace {
using namespace on1x;
Unit checked(const std::string& source){
 auto unit=parse(scan(source,"core-ref-hir"));
 check(unit);
 return unit;
}
template<class F>void reject(F operation,const std::string& text={}){
 try{operation();}catch(const Error& e){
  if(e.diagnostic_id!="ON1X-REF")throw;
  if(!text.empty()&&std::string(e.what()).find(text)==std::string::npos)throw;
  if(e.where.file.empty()||!e.where.line||!e.where.column)
   throw std::runtime_error("borrow diagnostic lost its source location");
  return;
 }
 throw std::runtime_error("invalid reference HIR admitted");
}
Expr* borrow_in(std::vector<Stmt>& body){
 for(auto& statement:body){
  if(statement.value&&statement.value->kind==Expr::Kind::Borrow)
   return statement.value.get();
  if(auto found=borrow_in(statement.body))return found;
  if(auto found=borrow_in(statement.otherwise))return found;
 }
 return nullptr;
}
Stmt* tag_in(std::vector<Stmt>& body){
 for(auto& statement:body){
  if(statement.value&&statement.value->kind==Expr::Kind::TagTest)return &statement;
  if(statement.value)if(auto found=tag_in(statement.value->body))return found;
  if(auto found=tag_in(statement.body))return found;
  if(auto found=tag_in(statement.otherwise))return found;
 }
 return nullptr;
}
constexpr auto sample=
 "fn set(x:ref i64,y:ref i64){x:=y;}"
 "fn main(){let a=1;let b=2;set(a,b);}";
}

int main(){try{
 auto unit=checked(sample);
 auto c=emit_c(unit,"test");
 check(unit);
 if(emit_c(unit,"test")!=c||c.find("static void on1x_fn_set(int64_t *")==std::string::npos)
  throw std::runtime_error("reference checking/code generation drift");
 unit.functions[1].body[2].value->children[1]->text="a";
 reject([&]{canonical::verify_values(unit);});
 reject([&]{emit_c(unit,"test");});
 auto wrong_store=checked(sample);
 wrong_store.functions[0].body[0].ref_store=false;
 reject([&]{emit_c(wrong_store,"test");});
 auto wrong_call=checked(sample);
 wrong_call.functions[1].body[2].value->callee->reference=Expr::Reference::Function;
 reject([&]{emit_c(wrong_call,"test");});
 auto hidden_borrow=checked(sample);
 hidden_borrow.functions[1].body[2].value->prototype=true;
 reject([&]{emit_c(hidden_borrow,"test");});
 auto wrong_place=checked(sample);
 auto& argument=*wrong_place.functions[1].body[2].value->children[0];
 argument.kind=Expr::Kind::Integer;argument.text="1";argument.reference=Expr::Reference::Unresolved;
 reject([&]{emit_c(wrong_place,"test");});
 auto wrong_capture=checked("fn read(x:ref i64){let y=x;let f=fn()->i64{return y;};} fn main(){}");
 auto& closure=*wrong_capture.functions[0].body[1].value;
 closure.captures[0].name="x";
 closure.body[0].value->text="x";
 reject([&]{emit_c(wrong_capture,"test");});

  constexpr auto shared=
   "fn read(x:borrow i64)->i64{return x;}"
   "fn main(){let a=1;let r=borrow a;println(read(r));}";
  auto shared_unit=checked(shared);
  auto shared_c=emit_c(shared_unit,"test");
  if(shared_c.find("static int64_t on1x_fn_read(int64_t const *")==
       std::string::npos)
   throw std::runtime_error("shared borrow did not lower to a private const pointer");

  constexpr auto owner=
   "record Cell{value:i64}"
   "fn read(x:borrow Cell)->i64{return x[\"value\"];}"
   "fn main(){let x=box(record<Cell>{value:1});println(read(x));drop(x);}";
  auto owner_unit=checked(owner);
  auto owner_c=emit_c(owner_unit,"test");
  if(owner_c.find("on1x_unique_borrow(")==std::string::npos)
   throw std::runtime_error("owner payload borrow lacks checked runtime access");

  auto invalid_parameter=checked(sample);
  invalid_parameter.functions[0].params[0].borrow=
   static_cast<BorrowMode>(99);
  reject([&]{canonical::verify_values(invalid_parameter);},
         "invalid checked borrowed parameter mode");

  auto invalid_mode=checked(shared);
  auto* mode=borrow_in(invalid_mode.functions[1].body);
  if(!mode)throw std::runtime_error("missing checked lexical borrow");
  mode->borrow=BorrowMode::Value;
  reject([&]{emit_c(invalid_mode,"test");},
         "invalid checked borrow expression mode");

  auto forged_mode=checked(shared);
  auto* forged=borrow_in(forged_mode.functions[1].body);
  forged->borrow=static_cast<BorrowMode>(99);
  reject([&]{canonical::verify_values(forged_mode);},
         "invalid checked borrow expression mode");

  auto wrong_referent=checked(shared);
  auto* referent=borrow_in(wrong_referent.functions[1].body);
  referent->type=Type::Bool;
  reject([&]{emit_c(wrong_referent,"test");},
         "referent type mismatch");

  auto escaped=checked(shared);
  auto& escaped_statement=escaped.functions[1].body[1];
  escaped_statement.kind=Stmt::Kind::Expr;
  escaped_statement.name.clear();
  escaped_statement.declared=Type::Invalid;
  reject([&]{emit_c(escaped,"test");},
         "escaped its direct lexical let binding");

  auto forged_source=checked(shared);
  auto* source=borrow_in(forged_source.functions[1].body);
  source->children[0]->reference=Expr::Reference::Function;
  reject([&]{canonical::verify_values(forged_source);},
         "named addressable local place");

  auto wrong_owner=checked(owner);
  wrong_owner.functions[0].params[0].type=
   Type::own(Type::named("Cell"));
  reject([&]{canonical::verify_values(wrong_owner);},
         "supported named referent");

  constexpr auto exchange=
   "fn swap(x:ref i64,y:i64)->i64{return ref_replace(x,y);}"
   "fn main(){let a=1;println(swap(a,2));}";
  auto bad_exchange=checked(exchange);
  auto& exchange_call=*bad_exchange.functions[0].body[0].value;
  exchange_call.children[1]->text="x";
  reject([&]{canonical::verify_values(bad_exchange);},"during its exclusive borrow");
  reject([&]{emit_c(bad_exchange,"test");},"during its exclusive borrow");

  auto shared_exchange=checked(exchange);
  shared_exchange.functions[0].params[0].borrow=BorrowMode::Shared;
  reject([&]{emit_c(shared_exchange,"test");},"named exclusive borrowed place");

  auto wrong_exchange_type=checked(exchange);
  wrong_exchange_type.functions[0].body[0].value->children[1]->type=Type::Bool;
  reject([&]{emit_c(wrong_exchange_type,"test");},"invalid checked ref_replace signature");

  // A direct exclusive call can overwrite the active arm of an immutable
  // lexical slot. A checked payload projection must re-test the new tag.
  auto stale_tag=checked(
   "enum E{A(i64),B(bool)}"
   "fn reset(x:ref E){x:=variant<E>(B,true);}"
   "fn use(v:E){match v with {A(x)=>println(x),B(y)=>println(y)};}"
   "fn main(){use(variant<E>(A,42));}");
  auto* yes=tag_in(stale_tag.functions[1].body);
  if(!yes)throw std::runtime_error("missing tag check");
  const auto& tag=*yes->value;
  auto update=std::make_unique<Expr>();
  update->kind=Expr::Kind::Call;update->type=Type::Void;update->where=tag.where;
  update->callee=std::make_unique<Expr>();
  update->callee->kind=Expr::Kind::Variable;
  update->callee->reference=Expr::Reference::BorrowFunction;
  update->callee->type=Type::function({tag.children[0]->type},Type::Void);
  update->callee->text="reset";update->callee->where=tag.where;
  std::size_t clone_budget=16;
  update->children.push_back(clone_expression(*tag.children[0],clone_budget));
  Stmt update_stmt;update_stmt.kind=Stmt::Kind::Expr;update_stmt.where=tag.where;
  update_stmt.value=std::move(update);
  yes->body.insert(yes->body.begin(),std::move(update_stmt));
  bool rejected_tag=false;
  try{(void)emit_c(stale_tag,"test");}
  catch(const Error& error){
   if(std::string(error.what()).find("payload projection lacks a dominating tag test")==
      std::string::npos)throw;
   rejected_tag=true;
  }
  if(!rejected_tag)throw std::runtime_error("stale tag after exclusive write was accepted");

  std::cout<<"borrow verifier: deterministic const/payload lowering and rejection of corrupted places/calls/stores/captures/modes PASS\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

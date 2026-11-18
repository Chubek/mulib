#include <on1x/type_safety.hpp>
#include <functional>
#include <iostream>

using namespace on1x;
Unit checked(const std::string& source){auto u=parse(scan(source,"type-check.on1x"));check(u);return u;}
void rejects(const std::function<void()>& action,const std::string& expected){
 try{action();}catch(const Error& e){if(std::string(e.what()).find(expected)!=std::string::npos)return;throw;}
 throw std::runtime_error("missing rejection: "+expected);
}
int main(){try{
  auto u=checked("fn main(){println(42);}");
  auto before=emit_c(u,"type-check.on1x"), report=type_safety::verify(u);
  if(report!=type_safety::verify(u)||before!=emit_c(u,"type-check.on1x"))throw std::runtime_error("verification mutated HIR or report");
  auto owning=checked(
   "enum N{Z}fn main(){let n=box(variant<N>(Z,record{}));drop(n);}");
  rejects([&]{type_safety::verify(owning);},
          "unique ownership is outside the independent structural adapter");
  // Mutate HIR after On1x's ordinary checker. These must reach the independent
 // Nerotypical checker, rather than passing through sema again.
 auto& literal=*u.functions[0].body[0].value->children[0];literal.type=Type::Bool;
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn f(x:i64)->i64{return x;}fn main(){println(f(1));}");
 u.functions[0].result=Type::Bool;
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn main(){let x=1;println(x);}");
 u.functions[0].body[1].value->children[0]->type=Type::String;
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn main(){if true {println(1);}}");
  u.functions[0].body[0].value->kind=Expr::Kind::Integer;u.functions[0].body[0].value->type=Type::I64;
  u.functions[0].body[0].value->text="1";
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn main(){let n=40;let f=fn(x:i64)->i64{return n+x;};println(f(2));}");
 type_safety::verify(u);
 u.functions[0].body[1].value->captures[0].type=Type::Bool;
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u.functions[0].body[1].value->captures.clear();
 rejects([&]{type_safety::verify(u);},"missing closure capture");
 u=checked("fn main(){let p=(1,true);}");
 u.functions[0].body[0].value->type=Type::tuple({Type::Bool,Type::I64});
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn take(f:fn(i64)->i64)->i64{return f(1);}fn main(){println(take(fn(x:i64)->i64{return x;}));}");
 type_safety::verify(u);
 u.functions[0].params[0].type=Type::function({Type::Bool},Type::Bool);
 rejects([&]{type_safety::verify(u);},"nerotypical");
 u=checked("fn main(){var x=1;x=2;}");u.functions[0].body[0].mutable_binding=false;
 rejects([&]{type_safety::verify(u);},"immutable local");
 u=checked("fn f()->i64{return 1;}fn main(){println(f());}");u.functions[0].body.clear();
  rejects([&]{type_safety::verify(u);},"return path");
  u=checked("fn main(){println(1.5f32);}");type_safety::verify(u);
  u.functions[0].body[0].value->children[0]->type=Type::F64;
  rejects([&]{type_safety::verify(u);},"nerotypical");
  u=checked("fn main(){println(255u8);}");type_safety::verify(u);
  u.functions[0].body[0].value->children[0]->type=Type::U64;
  rejects([&]{type_safety::verify(u);},"nerotypical");
  u=checked("fn main(){println(-1.5f32);}");
  u.functions[0].body[0].value->children[0]->children[0].reset();
  rejects([&]{type_safety::verify(u);},"missing expression child");
 u=checked("fn main(){println(42);}");
 rejects([&]{type_safety::verify(u,{0,4096,262144,64});},"term budget");
 rejects([&]{type_safety::verify(u,{1024,0,262144,64});},"equality budget");
 rejects([&]{type_safety::verify(u,{1024,4096,0,64});},"construction work");
 rejects([&]{type_safety::verify(u,{1024,4096,262144,0});},"did not decide");
 // Ground constructor equality oracle, independent of the solver. Include
 // nested arrows/tuples so mere outer-constructor comparison cannot suffice.
  std::vector<Type> types={Type::I64,Type::Bool,Type::String,Type::F32,Type::F64,Type::U8,Type::U16,Type::U32,Type::U64,
  Type::tuple({Type::I64,Type::Bool}),Type::tuple({Type::Bool,Type::I64}),
  Type::function({Type::I64},Type::Bool),Type::function({Type::Bool},Type::I64),
  Type::function({Type::tuple({Type::I64,Type::Bool})},Type::String)};
 std::size_t cases=0;
 for(const auto& a:types)for(const auto& b:types){
  Unit model;Function f;f.name="oracle";f.where={"oracle.on1x"};f.params.push_back({"x",a,f.where,0,{}});f.result=b;
  Stmt r;r.kind=Stmt::Kind::Return;r.where=f.where;r.value=std::make_unique<Expr>();r.value->kind=Expr::Kind::Variable;r.value->reference=Expr::Reference::Local;r.value->text="x";r.value->type=a;r.value->where=f.where;
  f.body.push_back(std::move(r));model.functions.push_back(std::move(f));
  if(a==b)type_safety::verify(model);else rejects([&]{type_safety::verify(model);},"nerotypical");++cases;
 }
 std::cout<<"Nerotypical HIR mutation, budgets and "<<cases<<" structural oracle cases PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

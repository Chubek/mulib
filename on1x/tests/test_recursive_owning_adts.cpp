#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <on1x/nominal.hpp>
#include <on1x_runtime.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace on1x;

void require(bool condition,std::string_view message){
 if(!condition)throw std::runtime_error(std::string(message));
}

template<class F>
void reject(F&& action,std::string_view diagnostic){
 try{action();}
 catch(const Error& error){
  if(error.diagnostic_id==diagnostic||
     std::string_view(error.what()).find(diagnostic)!=std::string_view::npos)
   return;
  throw std::runtime_error("wrong diagnostic: "+error.diagnostic_id+
                           " ("+error.what()+")");
 }
 throw std::runtime_error("invalid ownership input was admitted; expected "+
                          std::string(diagnostic));
}

void walk(Expr& expression,const std::function<void(Expr&)>& visit);

void walk(std::vector<Stmt>& body,const std::function<void(Expr&)>& visit){
 for(auto& statement:body){
  if(statement.value)walk(*statement.value,visit);
  walk(statement.body,visit);
  walk(statement.otherwise,visit);
 }
}

void walk(Expr& expression,const std::function<void(Expr&)>& visit){
 visit(expression);
 if(expression.callee)walk(*expression.callee,visit);
 for(auto& child:expression.children)if(child)walk(*child,visit);
 for(auto& proof:expression.proofs)if(proof)walk(*proof,visit);
 walk(expression.body,visit);
}

struct RuntimeNode {
 std::int64_t marker;
 on1x_unique *next;
};

std::int64_t destroyed_markers;

void destroy_runtime_node(void *raw){
 auto *node=static_cast<RuntimeNode *>(raw);
 destroyed_markers+=node->marker;
 on1x_unique_drop(node->next);
}

void destroy_owner_slot(void *raw){
 auto *slot=static_cast<on1x_unique **>(raw);
 on1x_unique_drop(*slot);
 *slot=nullptr;
}
}

int main(){
 try{
  const std::string source_text=R"(
enum List<T> { Nil, Cons(T, own<List<T>>) }
enum Result<T,E> { Ok(T), Err(E) }
fn empty<T>()->own<List<T>> {
  return box(variant<List<T>>(Nil,record{}));
}
fn prepend<T>(item:T,tail:own<List<T>>)->own<List<T>> {
  return box(variant<List<T>>(Cons,(item,move(tail))));
}
fn forward(
    outcome:Result<own<List<i64>>,own<List<i64>>>
)->Result<own<List<i64>>,own<List<i64>>> {
  let values=outcome?;
  return variant<Result<own<List<i64>>,own<List<i64>>>>(
    Ok,move(values));
}
fn main(){
  let a:own<List<i64>>=empty();
  let b=prepend(7,move(a));
  let node=take(b);
  match node with { Nil()=>0, Cons(value,tail)=>{drop(tail);value} };
}
)";
  auto raw=parse(scan(source_text,"recursive-owning-hir"));
  reject([&]{(void)meta::encode(raw);},"ON1X-OWN-PPX");

  check(raw);
  verify_nominals(raw);
  auto generated=emit_c(raw,"recursive-owning-hir");
  require(generated.find("ON1X_UNIQUE_ABI_VERSION")!=std::string::npos,
          "generated C omitted the unique ABI guard");
  require(generated.find("on1x_unique_try_box")!=std::string::npos,
          "generated C omitted owning-box construction");
  require(generated.find("on1x_unique_take")!=std::string::npos,
          "generated C omitted owning-box extraction");

  NominalCatalog catalog(raw);
  auto list=catalog.instantiate("List",{Type::I64},{"recursive-owning-hir",0,1,1});
  require(list.kind()==Type::Sum&&list.is_move_only()&&list.contains_unique(),
          "recursive nominal value lost move-only identity");
  auto owner=Type::own(Type::named("List",{Type::I64}));
  require(owner.is_move_only()&&owner.owned().nominal()=="List"&&
          check_type_form(owner),
          "finite own<Named> type form is invalid");

  std::size_t clone_budget=262144;
  auto copy=clone_unit(raw,clone_budget);
  require(emit_c(copy,"recursive-owning-hir")==generated,
           "ownership metadata was not stable across HIR cloning");
  reject([&]{(void)mir::lower(raw);},"ON1X-MIR-UNSUPPORTED");

  Expr *propagated_payload=nullptr;
  for(auto& function:copy.functions)if(!function.prototype)
   walk(function.body,[&](Expr& expression){
    if(!propagated_payload&&expression.kind==Expr::Kind::Payload&&
       expression.text=="Err"&&expression.move_out)
     propagated_payload=&expression;
   });
  require(propagated_payload,
          "Result propagation did not retain projected transfer metadata");
  propagated_payload->move_out=false;
  reject([&]{canonical::verify_values(copy);},"copy of unique sum payload");

  clone_budget=262144;
  auto moved_copy=clone_unit(raw,clone_budget);
  Expr *moved=nullptr;
  for(auto& function:moved_copy.functions)if(!function.prototype)
   walk(function.body,[&](Expr& expression){
     if(!moved&&expression.kind==Expr::Kind::Variable&&expression.move_out)
      moved=&expression;
   });
  require(moved,"checked HIR did not retain explicit transfer metadata");
  moved->move_out=false;
  reject([&]{canonical::verify_values(moved_copy);},"ON1X-VALUES-VERIFY");

  require(on1x_unique_live_allocations()==0,
          "runtime unique allocation counter was not initially zero");
  RuntimeNode taken_source{7,nullptr};
  auto *taken=on1x_unique_try_box(&taken_source,sizeof(taken_source),
                                  destroy_runtime_node);
  require(taken&&taken_source.marker==0&&taken_source.next==nullptr,
          "box did not clear its transferred source");
  RuntimeNode destination{};
  on1x_unique_take(&taken,&destination,sizeof(destination),
                   "recursive-owning-runtime",1,1);
  require(!taken&&destination.marker==7&&
          on1x_unique_live_allocations()==0,
          "take did not transfer and free exactly one box");
  destroy_runtime_node(&destination);

  constexpr std::int64_t depth=20000;
  on1x_unique *chain=nullptr;
  for(std::int64_t i=0;i<depth;++i){
   RuntimeNode source{1,chain};
   chain=on1x_unique_try_box(&source,sizeof(source),destroy_runtime_node);
   require(chain&&source.marker==0&&source.next==nullptr,
           "recursive box transfer failed");
  }
  require(on1x_unique_live_allocations()==depth,
          "runtime live-box accounting drifted");
  on1x_unique_drop(chain);
  require(on1x_unique_live_allocations()==0,
          "iterative recursive destruction leaked boxes");
  require(destroyed_markers==depth+7,
          "recursive destructor did not run exactly once per payload");

  RuntimeNode registered_source{1,nullptr};
  auto *registered=on1x_unique_try_box(&registered_source,
                                       sizeof(registered_source),
                                       destroy_runtime_node);
  on1x_cleanup cleanup{};
  on1x_cleanup_push(&cleanup,&registered,destroy_owner_slot);
  require(cleanup.active,"cleanup record was not activated");
  on1x_cleanup_pop(&cleanup);
  require(!cleanup.active,"cleanup record was not disarmed");
  on1x_unique_drop(registered);
  require(on1x_unique_live_allocations()==0&&
          destroyed_markers==depth+8,
          "disarmed cleanup changed owner destruction");

  std::cout<<"recursive own HIR, MIR rejection, C11 ABI and iterative C++ caller PASS\n";
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';
  return 1;
 }
}

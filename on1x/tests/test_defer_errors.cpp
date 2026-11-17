#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/meta.hpp>
#include <functional>
#include <iostream>

namespace {
using namespace on1x;
using E=Expr::Kind;using S=Stmt::Kind;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void reject(F f){try{f();}catch(const Error& e){require(!e.where.file.empty(),"unlocated rejection");return;}throw std::runtime_error("invalid defer/error HIR accepted");}
Unit checked(const std::string& text){auto u=parse(scan(text,"defer-error-test"));check(u);return u;}
void visit(Unit& u,const std::function<void(Expr&)>& ex,const std::function<void(Stmt&)>& st){
 std::function<void(std::vector<Stmt>&)> body;
 std::function<void(Expr&)> expr=[&](Expr& e){ex(e);if(e.callee)expr(*e.callee);for(auto& c:e.children)if(c)expr(*c);body(e.body);};
 body=[&](std::vector<Stmt>& b){for(auto& s:b){st(s);if(s.value)expr(*s.value);body(s.body);body(s.otherwise);}};
 for(auto& f:u.functions)if(!f.prototype)body(f.body);
}
void invalid(Unit& u){reject([&]{canonical::verify_values(u);});reject([&]{emit_c(u,"mutation");});reject([&]{mir::lower(u);});}
const std::string source=R"(
 enum Result<T,E>{Ok(T),Err(E)}
 fn use(x:Result<i64,u8>)->Result<bool,u8>{defer println(7);let n=x?;return variant<Result<bool,u8>>(Ok,n==42);}
 fn main(){println(match use(variant<Result<i64,u8>>(Ok,42)) with{Ok(b)=>b,Err(_)=>false});}
)";
}
int main(){try{
 auto raw=parse(scan(source,"wire"));require(canonical::needs_values(raw),"source effects not discovered");
 auto wire=meta::write_sexpr(meta::encode(raw));require(wire.find("Defer")!=std::string::npos&&wire.find("Propagate")!=std::string::npos,"source wire missing effects");
 auto copy=meta::decode(meta::read_sexpr(wire,"wire"));require(meta::write_sexpr(meta::encode(copy))==wire,"source wire round-trip mismatch");
 check(copy);require(!canonical::needs_values(copy),"source effects survived lowering");auto output=emit_c(copy,"wire");check(copy);require(emit_c(copy,"wire")==output,"rechecking effects changed code");
 auto no_defer=checked("fn main(){println(42);}");require(emit_c(no_defer,"plain").find("on1x_defer_")==std::string::npos,"defer runtime leaked into unaffected code");
 auto transactional=parse(scan("fn main(){defer println(42);}","unchecked"));auto before=meta::write_sexpr(meta::encode(transactional));
 reject([&]{canonical::lower_values(transactional);});require(meta::write_sexpr(meta::encode(transactional))==before,"failed defer lowering published partial HIR");
 auto propagation=parse(scan(source,"unchecked-result"));before=meta::write_sexpr(meta::encode(propagation));reject([&]{canonical::lower_values(propagation);});require(meta::write_sexpr(meta::encode(propagation))==before,"failed propagation lowering published partial HIR");
 for(int mutation=0;mutation<7;++mutation){
  auto u=checked("fn main(){let n=42;defer println(n);}");bool changed=false;
  visit(u,[](Expr&){},[&](Stmt& s){if(changed||s.kind!=S::PushDefer)return;changed=true;
   switch(mutation){
   case 0:s.value.reset();break;
   case 1:s.value->result=Type::I64;break;
   case 2:s.value->captures.clear();break;
   case 3:s.kind=S::Defer;break;
   case 4:s.name="forged";break;
   case 5:s.where.line=0;break;
   case 6:s.value->kind=E::Variable;s.value->text="missing";s.value->reference=Expr::Reference::Local;break;
   }
  });require(changed,"missing deferred thunk mutation fixture");invalid(u);
 }
 for(int mutation=0;mutation<6;++mutation){
  auto u=checked(source);bool changed=false;
  visit(u,[&](Expr& e){if(changed)return;
   if(mutation==0&&e.kind==E::TagTest){e.projection=1-e.projection;changed=true;}
   if(mutation==1&&e.kind==E::Payload){e.text=e.text=="Ok"?"Err":"Ok";changed=true;}
   if(mutation==2&&e.kind==E::Region){e.kind=E::Propagate;changed=true;}
   if(mutation==3&&e.kind==E::Variant&&e.text=="Err"){e.children[0]->type=Type::U16;changed=true;}
   if(mutation==4&&e.kind==E::Variant&&e.text=="Err"){e.type=e.result=Type::data(Type::Sum,{Type::U8,Type::Bool},{"Err","Ok"});changed=true;}
  },[&](Stmt& s){if(!changed&&mutation==5&&s.kind==S::Initialize){s.kind=S::Expr;changed=true;}});
  require(changed,"missing propagation mutation fixture");invalid(u);
 }
 // Error exits cannot borrow an enclosing function's return type inside a thunk.
 auto boundary=checked(source);bool changed=false;
 visit(boundary,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::PushDefer){Stmt ret;ret.kind=S::Return;ret.where=s.where;ret.value=std::make_unique<Expr>();ret.value->kind=E::Integer;ret.value->where=s.where;ret.value->type=Type::I64;ret.value->text="1";s.value->body.push_back(std::move(ret));changed=true;}});invalid(boundary);
 // Source wire can carry calls/operands, never private registration or proof nodes.
 auto bad=meta::encode(parse(scan("fn main(){defer println(1);}","bad-wire")));
 auto& deferred=bad.items[2].items[0].items[4].items[1].items[0];
 deferred.items[0].text="PushDefer";reject([&]{meta::decode(meta::read_sexpr(meta::write_sexpr(bad),"bad-wire"));});
 deferred.items[0].text="Defer";deferred.items.insert(deferred.items.end()-1,meta::symbol("123"));reject([&]{meta::decode(meta::read_sexpr(meta::write_sexpr(bad),"bad-wire"));});
 // Schema-valid source output is still checked; a PPX cannot make noncalls or
 // non-Results acquire trusted defer/propagation semantics.
 auto integer=meta::encode(parse(scan("fn main(){42;}","wire-mutation")));
 auto node=integer.items[2].items[0].items[4].items[1].items[0].items[1];
 auto not_call=meta::encode(parse(scan("fn main(){defer println(1);}","wire-mutation")));
 not_call.items[2].items[0].items[4].items[1].items[0].items[1]=node;
 auto decoded=meta::decode(meta::read_sexpr(meta::write_sexpr(not_call),"wire-mutation"));reject([&]{check(decoded);});
 auto not_result=meta::encode(parse(scan("fn main(){42?;}","wire-mutation")));
 decoded=meta::decode(meta::read_sexpr(meta::write_sexpr(not_result),"wire-mutation"));reject([&]{check(decoded);});
 std::cout<<"defer/Result source wire, transactional lowering, exact returns, capture/tag/initialization mutations and idempotence PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

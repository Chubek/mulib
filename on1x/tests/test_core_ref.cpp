#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/values.hpp>
#include <iostream>

namespace {
using namespace on1x;
Unit checked(const std::string& source){
 auto unit=parse(scan(source,"core-ref-hir"));
 check(unit);
 return unit;
}
template<class F>void reject(F operation){
 try{operation();}catch(const Error& e){if(e.diagnostic_id=="ON1X-REF")return;throw;}
 throw std::runtime_error("invalid reference HIR admitted");
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
 std::cout<<"reference verifier: deterministic lowering and rejection of corrupted places/calls/stores/captures PASS\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

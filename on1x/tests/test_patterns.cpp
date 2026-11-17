#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <on1x/patterns.hpp>
#include <functional>
#include <iostream>
namespace {
using namespace on1x;
using E=Expr::Kind;using S=Stmt::Kind;
void require(bool okay,const char* text){if(!okay)throw std::runtime_error(text);}
template<class F>void reject(F f){try{f();}catch(const Error&){return;}throw std::runtime_error("malformed canonical values accepted");}
Unit checked(const std::string& text){auto unit=parse(scan(text,"pattern-test"));check(unit);return unit;}
void visit(Unit& u,const std::function<void(Expr&)>& ex,const std::function<void(Stmt&)>& st){
 std::function<void(std::vector<Stmt>&)> body;
 std::function<void(Expr&)> expression=[&](Expr& e){ex(e);if(e.callee)expression(*e.callee);for(auto& c:e.children)if(c)expression(*c);body(e.body);};
 body=[&](std::vector<Stmt>& b){for(auto& s:b){st(s);if(s.value)expression(*s.value);body(s.body);body(s.otherwise);}};
 for(auto& f:u.functions)if(!f.prototype)body(f.body);
}
void invalid(Unit& u){reject([&]{canonical::verify_values(u);});reject([&]{emit_c(u,"mutation");});reject([&]{mir::lower(u);});}
}
int main(){try{
 auto surface=parse(scan("fn main(){println(match (true,2) with {(true,n)|(false,n)=>{let x=n+40;x}});}","wire"));
 auto wire=meta::write_sexpr(meta::encode(surface));auto copy=meta::decode(meta::read_sexpr(wire,"wire"));
 require(meta::write_sexpr(meta::encode(copy))==wire,"structural pattern source round-trip mismatch");
 check(copy);require(!canonical::needs_values(copy),"source match survived canonicalization");
 auto c=emit_c(copy,"wire");check(copy);require(emit_c(copy,"wire")==c,"rechecking changed canonical values");
 auto ir=mir::lower(copy);require(mir::evaluate(ir).output=="42\n","MIR pattern result mismatch");
 auto missing=checked("fn main(){println(match true with {true=>1,false=>2});}");
 bool changed=false;visit(missing,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Initialize){s.kind=S::Expr;changed=true;}});
 require(changed,"no canonical initialization found");invalid(missing);
 auto repeated=checked("fn main(){println(match 1 with {n=>n});}");
 changed=false;visit(repeated,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Initialize){std::size_t budget=262144;auto second=clone_statement(s,budget);s.kind=S::Block;s.body.push_back(std::move(second));s.body.push_back(clone_statement(s.body[0],budget));s.value.reset();changed=true;}});invalid(repeated);
 auto wrong=checked("fn main(){println(match (true,2) with {(_,n)=>n});}");
 changed=false;visit(wrong,[&](Expr& e){if(!changed&&e.kind==E::Project){e.projection=99;changed=true;}},[](Stmt&){});invalid(wrong);
 auto tags=checked("fn main(){println(match variant<sum{A:i64,B:bool}>(A,1) with {A(n)=>n,B(_)=>0});}");
 changed=false;visit(tags,[&](Expr& e){if(!changed&&e.kind==E::TagTest){e.kind=E::Boolean;e.text="true";e.children.clear();changed=true;}},[](Stmt&){});invalid(tags);
 auto target=checked("fn main(){println(match true with {n=>1});}");
 changed=false;visit(target,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Leave){s.control_target=0;changed=true;}});reject([&]{canonical::verify_control(target);});reject([&]{emit_c(target,"mutation");});
 auto metadata=checked("fn main(){println(1);}");metadata.functions[0].body[0].value->noreturn=true;invalid(metadata);
 auto null_value=checked("fn main(){println(match true with {n=>1});}");
 changed=false;visit(null_value,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Initialize){s.value.reset();changed=true;}});invalid(null_value);
 auto null_child=checked("fn main(){println(match (1,2) with {(a,b)=>a});}");
 changed=false;visit(null_child,[&](Expr& e){if(!changed&&e.kind==E::Project){e.children[0].reset();changed=true;}},[](Stmt&){});reject([&]{emit_c(null_child,"mutation");});reject([&]{mir::lower(null_child);});
 auto bad_surface=parse(scan("fn main(){match true with {_=>1};}","bad-surface"));
 auto before=meta::write_sexpr(meta::encode(bad_surface));reject([&]{canonical::lower_values(bad_surface);});require(before==meta::write_sexpr(meta::encode(bad_surface)),"failed value lowering published a partial tree");
 auto malformed=meta::read_sexpr("(Unit 1 ((Function \"main\" () (VoidType) (Block ((LetPattern (StructuralPattern \"wildcard\" \"\" () () false (Loc \"x\" 0 1 1)) (Inferred) (DecimalLiteral \"1\" (Loc \"x\" 0 1 1)) (Loc \"x\" 0 1 1))) (Loc \"x\" 0 1 1)) () (Loc \"x\" 0 1 1))))","wire");
 reject([&]{meta::decode(malformed);});
 // Neither syntax/PPX nor a corrupted typed tree can manufacture private nodes.
 reject([&]{meta::decode(meta::read_sexpr("(Unit 1 ((Function \"main\" () (VoidType) (Block ((Trap (Loc \"x\" 0 1 1))) (Loc \"x\" 0 1 1)) () (Loc \"x\" 0 1 1))))","wire"));});
 Pattern bad;bad.kind=Pattern::Kind::Literal;bad.name="\"a\" + \"b\"";bad.where={"pattern-test"};reject([&]{patterns::literal(bad);});
 std::cout<<"structural match wire, value initialization, projection/tag domination and mutation rejection PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

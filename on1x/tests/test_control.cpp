#include <on1x/canonical/control.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <iostream>
namespace {
void require(bool okay,const char* text){if(!okay)throw std::runtime_error(text);}
on1x::Unit checked(const std::string& text){auto u=on1x::parse(on1x::scan(text,"control-test"));on1x::check(u);return u;}
template<class F>void reject(F f){try{f();}catch(const on1x::Error&){return;}throw std::runtime_error("invalid control accepted");}
}
int main(){try{
 auto u=checked("fn main(){outer:while true{loop{continue outer;}}}");
 on1x::canonical::verify_control(u);
 auto& outer=u.functions[0].body[0];auto& inner=outer.body[0];auto& exit=inner.body[0];
 require(outer.control_target&&inner.control_target!=outer.control_target&&exit.control_target==outer.control_target,"canonical target resolution failed");
 exit.control_target=0;reject([&]{on1x::canonical::verify_control(u);});reject([&]{on1x::emit_c(u,"mutation");});reject([&]{on1x::mir::lower(u);});
 exit.control_target=outer.control_target;inner.control_target=outer.control_target;reject([&]{on1x::canonical::verify_control(u);});
 auto range=checked("fn main(){for i in 0..=2{if i==1{continue;}println(i);}}");
 auto mir=on1x::mir::lower(range);require(on1x::mir::evaluate(mir).output=="0\n2\n","range MIR reference mismatch");
 auto raw=on1x::parse(on1x::scan("fn main(){for i in 0..1{break;}}","raw"));reject([&]{on1x::canonical::verify_control(raw);});
 std::size_t budget=262144;auto original=on1x::clone_unit(raw,budget);
 raw.functions[0].body[0].body[0].label="not_found";
 reject([&]{on1x::canonical::lower_control(raw);});
 require(raw.functions[0].body[0].kind==on1x::Stmt::Kind::Range,"failed lowering partially published");
 auto tree=on1x::meta::encode(original);auto copy=on1x::meta::decode(tree);on1x::check(copy);on1x::canonical::verify_control(copy);
 original.functions[0].body[0].value->children[0].reset();reject([&]{on1x::canonical::lower_control(original);});
 auto provenance=checked("fn main(){loop{break;}}");provenance.functions[0].body[0].where.line=0;reject([&]{on1x::canonical::verify_control(provenance);});
 // IDs are not a source wire field and may not point across a lambda boundary.
 auto boundary=checked("fn main(){loop{let f=fn(){loop{break;}};break;}}");
 auto& loop=boundary.functions[0].body[0];auto& lambda=*loop.body[0].value;
 lambda.body[0].body[0].control_target=loop.control_target;reject([&]{on1x::canonical::verify_control(boundary);});
 auto latch=checked("fn main(){for(var i=0;i<2;i=i+1){continue;}}");
 auto& canonical_loop=latch.functions[0].body[0].body[1];
 auto invalid_exit=on1x::Stmt{};invalid_exit.kind=on1x::Stmt::Kind::Continue;invalid_exit.where=canonical_loop.where;invalid_exit.control_target=canonical_loop.control_target;
 canonical_loop.otherwise.push_back(std::move(invalid_exit));reject([&]{on1x::canonical::verify_control(latch);});
 auto bad=on1x::meta::read_sexpr("(Unit 1 ((Function \"main\" () (VoidType) (Block ((LoopExit \"goto\" \"\" 0 (Loc \"x\" 0 1 1))) (Loc \"x\" 0 1 1)) () (Loc \"x\" 0 1 1))))","wire");
 reject([&]{on1x::meta::decode(bad);});
 std::cout<<"canonical loop targets, mutation rejection and source-wire round-trip PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

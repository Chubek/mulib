#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <functional>
#include <iostream>
namespace {
using namespace on1x;
using E=Expr::Kind;using S=Stmt::Kind;
void require(bool okay,const char* text){if(!okay)throw std::runtime_error(text);}
template<class F>void reject(F f){try{f();}catch(const Error&){return;}throw std::runtime_error("malformed value loop accepted");}
Unit checked(const std::string& text){auto u=parse(scan(text,"value-loop-test"));check(u);return u;}
void visit(Unit& u,const std::function<void(Expr&)>& ex,const std::function<void(Stmt&)>& st){
 std::function<void(std::vector<Stmt>&)> body;
 std::function<void(Expr&)> expression=[&](Expr& e){ex(e);if(e.callee)expression(*e.callee);for(auto& c:e.children)if(c)expression(*c);body(e.body);};
 body=[&](std::vector<Stmt>& b){for(auto& s:b){st(s);if(s.value)expression(*s.value);body(s.body);body(s.otherwise);}};
 for(auto& f:u.functions)if(!f.prototype)body(f.body);
}
void invalid(Unit& u){reject([&]{canonical::verify_values(u);});reject([&]{emit_c(u,"mutation");});reject([&]{mir::lower(u);});}
}
int main(){try{
 auto raw=parse(scan("fn main(){println(answer:loop{break answer with 42;});}","wire"));
 auto wire=meta::write_sexpr(meta::encode(raw));require(wire.find("LoopValue")!=std::string::npos&&wire.find("ValueBreak")!=std::string::npos,"missing additive source constructors");
 auto copy=meta::decode(meta::read_sexpr(wire,"wire"));require(meta::write_sexpr(meta::encode(copy))==wire,"source loop round-trip mismatch");
 check(copy);require(!canonical::needs_values(copy),"surface loop survived lowering");
 auto output=emit_c(copy,"wire");check(copy);require(emit_c(copy,"wire")==output,"canonical loop rechecking changed output");
 require(mir::evaluate(mir::lower(copy)).output=="42\n","MIR value-loop mismatch");
 auto before=meta::write_sexpr(meta::encode(raw));reject([&]{canonical::lower_values(raw);});require(before==meta::write_sexpr(meta::encode(raw)),"failed lowering partially published");
 canonical::lower_control(raw);auto& loop=*raw.functions[0].body[0].value->children[0];
 auto id=loop.control_target;require(id&&loop.body[0].control_target==id,"value-loop target resolution mismatch");
 loop.body[0].control_target=id+1;reject([&]{canonical::verify_control(raw);});loop.body[0].control_target=id;
 auto retarget=parse(scan("fn main(){println(a:loop{let x=b:loop{break b with 1;};break a with x;});}","retarget"));canonical::lower_control(retarget);
 auto& active_outer=*retarget.functions[0].body[0].value->children[0];auto& active_inner=*active_outer.body[0].value;
 active_inner.body[0].control_target=active_outer.control_target;reject([&]{canonical::verify_control(retarget);});
 loop.where.line=0;reject([&]{canonical::verify_control(raw);});
 auto missing=checked("fn main(){println(loop{break with 42;});}");bool changed=false;
 visit(missing,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Initialize){s.kind=S::Expr;changed=true;}});require(changed,"missing result initialization fixture");invalid(missing);
 auto fallback=checked("fn main(){println(loop{break with 42;});}");
 changed=false;visit(fallback,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Trap){s.kind=S::Block;changed=true;}});invalid(fallback);
 auto backedge=checked("fn main(){println(loop{break with 42;});}");std::size_t target=0;
 visit(backedge,[](Expr&){},[&](Stmt& s){if(s.kind==S::While)target=s.control_target;});
 changed=false;visit(backedge,[](Expr&){},[&](Stmt& s){if(!changed&&s.kind==S::Leave){s.kind=S::Continue;s.control_target=target;changed=true;}});
 canonical::verify_control(backedge);invalid(backedge);
 // Header expressions repeat too: a condition cannot initialize an enclosing
 // single-assignment slot and then execute a backedge to the same condition.
 auto header=checked("fn main(){println(loop{if true{break with 42;}});}");Stmt* header_loop=nullptr;Stmt* header_init=nullptr;
 visit(header,[](Expr&){},[&](Stmt& s){if(s.kind==S::While)header_loop=&s;if(s.kind==S::Initialize)header_init=&s;});
 require(header_loop&&header_init,"missing header fixture");std::size_t header_budget=262144;auto header_write=clone_statement(*header_init,header_budget);header_init->kind=S::Expr;
 auto condition=std::make_unique<Expr>();condition->kind=E::Region;condition->type=Type::Bool;condition->where=header_loop->where;condition->body.push_back(std::move(header_write));condition->children.push_back(std::move(header_loop->value));header_loop->value=std::move(condition);invalid(header);
 // A conditional initialization is a possible write even when not definite.
 auto possible=checked("fn main(){println(loop{break with 42;});}");Stmt* while_loop=nullptr;Stmt* init=nullptr;
 visit(possible,[](Expr&){},[&](Stmt& s){if(s.kind==S::While)while_loop=&s;if(s.kind==S::Initialize)init=&s;});
 require(while_loop&&init,"missing loop/initialization fixture");std::size_t budget=262144;auto extra=clone_statement(*init,budget);
 Stmt branch;branch.kind=S::If;branch.where=extra.where;branch.value=std::make_unique<Expr>();branch.value->kind=E::Boolean;branch.value->where=extra.where;branch.value->type=Type::Bool;branch.value->text="true";branch.body.push_back(std::move(extra));while_loop->body.insert(while_loop->body.begin(),std::move(branch));invalid(possible);
 auto surface=checked("fn main(){println(loop{break with 42;});}");
 changed=false;visit(surface,[&](Expr& e){if(!changed&&e.kind==E::Region){e.kind=E::Loop;changed=true;}},[](Stmt&){});invalid(surface);
 auto boundary=parse(scan("fn main(){println(a:loop{let f=fn(){loop{break;}};break a with 42;});}","boundary"));canonical::lower_control(boundary);
 auto& outer=*boundary.functions[0].body[0].value->children[0];outer.body[0].value->body[0].body[0].control_target=outer.control_target;reject([&]{canonical::verify_control(boundary);});
 auto bare=checked("fn main(){(loop{if true{break;}break with 1;});}");require(mir::evaluate(mir::lower(bare)).output.empty(),"bare break did not yield void");
 // Wire data has naming contexts, never a canonical target or a result proof.
 auto malformed=meta::encode(parse(scan("fn main(){(loop{break with 1;});}","bad-wire")));
 auto& loop_wire=malformed.items[2].items[0].items[4].items[1].items[0].items[1];
 loop_wire.items.insert(loop_wire.items.end()-1,meta::symbol("777"));reject([&]{meta::decode(malformed);});
 std::cout<<"value-loop source wire, result initialization, backedge/target mutations and idempotence PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

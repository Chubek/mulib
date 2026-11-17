#include <on1x/concurrency.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
template<class F>void reject(F&& action){try{action();}catch(const on1x::Error&){return;}throw std::runtime_error("invalid concurrency accepted");}
constexpr auto source = R"(fn main(){taskgroup{
 let ch=->(1,1){payload:i64,capacity:0,mode:uni};
 let (tx,rx)=split ch;let worker=launch{let sent=tx <- 42; 7};
 match <-rx with{Some(value)=>println(value),None(_)=>println(-1)};
 println(join worker);
}})";
on1x::Unit checked(){auto unit=on1x::parse(on1x::scan(source,"fiber-test"));on1x::check(unit);return unit;}
}
int main(){try{
 auto unit=checked();on1x::csp::verify(unit,true);
 require(unit.functions[0].body[0].kind==on1x::Stmt::Kind::CspScope,"taskgroup was not canonicalized");
 require(unit.functions[0].body[0].body[0].value->kind==on1x::Expr::Kind::Csp,"channel operation was erased");
 require(on1x::emit_c(unit,"fiber-test").find("on1x_csp_run(")!=std::string::npos,"C11 scheduler not emitted");
 reject([&]{(void)on1x::mir::lower(unit);});
 auto source_wire=on1x::meta::encode(on1x::parse(on1x::scan(source,"fiber-test")));
 auto round_trip=on1x::meta::decode(source_wire);on1x::check(round_trip);on1x::csp::verify(round_trip,true);
 std::size_t budget=262144;
 auto invalid=on1x::clone_unit(unit,budget);
 invalid.functions[0].body[0].body[0].value->type=on1x::Type::I64;
 reject([&]{on1x::csp::verify(invalid,true);});reject([&]{(void)on1x::emit_c(invalid,"invalid");});
 invalid=on1x::clone_unit(unit,budget);
 invalid.functions[0].body[0].body[0].value->children[0].reset();
 reject([&]{on1x::csp::verify(invalid,true);});
 invalid=on1x::clone_unit(unit,budget);
 invalid.functions[0].body[0].where.line=0;
 reject([&]{on1x::csp::verify(invalid,true);});
 invalid=on1x::clone_unit(unit,budget);
 invalid.functions[0].body[0].body[0].value->kind=on1x::Expr::Kind::SourceCsp;
 reject([&]{on1x::csp::verify(invalid,true);});
  auto source_unit=on1x::parse(on1x::scan(source,"fiber-test"));
  reject([&]{on1x::csp::verify(source_unit,true);});
  constexpr auto poll_source=R"(fn main(){taskgroup{
   let ch=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split ch;
   let choice=poll{send tx <- 9=>1,closed tx=>-1,default=>0};
   poll{recv rx as n=>println(n),closed rx=>println(-2)};
   println(choice);
  }})";
  auto poll_wire=on1x::meta::encode(on1x::parse(on1x::scan(poll_source,"poll-test")));
  auto poll_unit=on1x::meta::decode(poll_wire);on1x::check(poll_unit);
  on1x::csp::verify(poll_unit,true);
  require(on1x::emit_c(poll_unit,"poll-test").find("on1x_csp_select(")!=std::string::npos,"atomic poll not emitted");
  reject([&]{(void)on1x::mir::lower(poll_unit);});
  constexpr auto external_source=R"(fn main(){taskgroup{
   let child:xfiber=launch exec("/bin/true");println(join child);
   let stopped:xfiber=launch exec("/bin/sleep","10");println(cancel stopped);
  }})";
  auto external_wire=on1x::meta::encode(on1x::parse(on1x::scan(external_source,"external-test")));
  auto external_unit=on1x::meta::decode(external_wire);on1x::check(external_unit);
  on1x::csp::verify(external_unit,true);
  auto external_c=on1x::emit_c(external_unit,"external-test");
  require(external_c.find("on1x_csp_launch_exec(")!=std::string::npos,"direct process spawn not emitted");
  require(external_c.find("on1x_csp_process_cancel(")!=std::string::npos,"consuming external cancellation not emitted");
  reject([&]{(void)on1x::mir::lower(external_unit);});
  auto forged_process=on1x::clone_unit(external_unit,budget);
  forged_process.functions[0].body[0].body[0].value->children[0]->text="\"relative\"";
  reject([&]{on1x::csp::verify(forged_process,true);});
  reject([&]{(void)on1x::emit_c(forged_process,"forged-process");});
  constexpr auto ready_source=R"(fn main(){taskgroup{
   let child=launch{println(999);42};cancel child;
  }})";
  auto ready_unit=on1x::meta::decode(on1x::meta::encode(on1x::parse(on1x::scan(ready_source,"ready-test"))));
  on1x::check(ready_unit);on1x::csp::verify(ready_unit,true);
  require(on1x::emit_c(ready_unit,"ready-test").find("on1x_csp_cancel_ready(")!=std::string::npos,"pre-dispatch cancellation not emitted");
 std::cout<<"canonical concurrency verification and source-wire round-trip PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

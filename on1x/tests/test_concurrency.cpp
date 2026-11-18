#include <on1x/concurrency.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <algorithm>
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
on1x::Expr *find_poll(std::vector<on1x::Stmt>&);
on1x::Expr *find_poll(on1x::Expr& e){
 if((e.kind==on1x::Expr::Kind::Csp||e.kind==on1x::Expr::Kind::SourceCsp)&&e.text=="poll")return &e;
 if(e.callee)if(auto p=find_poll(*e.callee))return p;
 for(auto& child:e.children)if(child)if(auto p=find_poll(*child))return p;
 return find_poll(e.body);
}
on1x::Expr *find_poll(std::vector<on1x::Stmt>& body){
 for(auto& s:body){
  if(s.value)if(auto p=find_poll(*s.value))return p;
  if(auto p=find_poll(s.body))return p;
  if(auto p=find_poll(s.otherwise))return p;
 }
 return nullptr;
}
}
int main(){try{
 auto unit=checked();on1x::csp::verify(unit,true);
 require(unit.functions[0].body[0].kind==on1x::Stmt::Kind::CspScope,"taskgroup was not canonicalized");
 require(unit.functions[0].body[0].body[0].value->kind==on1x::Expr::Kind::Csp,"channel operation was erased");
 require(on1x::emit_c(unit,"fiber-test").find("on1x_csp_run_v4(")!=std::string::npos,"C11 scheduler not emitted");
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
   constexpr auto timed_source=R"(fn main(){taskgroup{
    let child=launch{defer println(8);for i in 0..1000{assert(i>=0);} 42};
    poll{after 1ms=>println(1),default=>println(2)};
    let stopped=cancel child;println(stopped);
   }})";
   auto timed_wire=on1x::meta::encode(on1x::parse(on1x::scan(timed_source,"timed-test")));
   {
    auto untyped=on1x::meta::decode(timed_wire);
    auto poll=find_poll(untyped.functions[0].body);require(poll!=nullptr,"missing wire deadline");
    std::size_t wire_budget=262144;
    poll->children.insert(poll->children.begin(),on1x::clone_expression(*poll->children[0],wire_budget));
    reject([&]{on1x::check(untyped);});
   }
   auto timed=on1x::meta::decode(timed_wire);on1x::check(timed);on1x::csp::verify(timed,true);
   auto timed_c=on1x::emit_c(timed,"timed-test");
   require(timed_c.find("on1x_csp_select_after(")!=std::string::npos,"monotonic deadline not emitted");
   require(timed_c.find("on1x_csp_cancelled()")!=std::string::npos,"cancellation exit edges not emitted");
   require(timed_c.find("on1x_csp_shield_enter()")!=std::string::npos,"cleanup shielding not emitted");
   reject([&]{(void)on1x::mir::lower(timed);});
   for(int mutation=0;mutation<3;++mutation){
    std::size_t copy_budget=262144;auto forged=on1x::clone_unit(timed,copy_budget);
    auto poll=find_poll(forged.functions[0].body);require(poll!=nullptr,"missing deadline mutation target");
    if(mutation==0)poll->children[0]->children[0]->type=on1x::Type::Bool;
    else if(mutation==1)poll->children[0]->children[0].reset();
    else{
     poll->children.insert(poll->children.begin(),on1x::clone_expression(*poll->children[0],copy_budget));
    }
    reject([&]{on1x::csp::verify(forged,true);});
    reject([&]{(void)on1x::emit_c(forged,"forged-timer");});
   }
   auto within_wire=on1x::meta::encode(on1x::parse(on1x::scan(
    "fn main(){let milliseconds=8;taskgroup within milliseconds {taskgroup within 1s {}}}",
    "within-test")));
   auto within=on1x::meta::decode(within_wire);on1x::check(within);
   on1x::csp::verify(within,true);
   auto& scope=within.functions[0].body[1];
   require(scope.kind==on1x::Stmt::Kind::CspScope&&scope.value&&scope.value->type==on1x::Type::I64,
           "round-trip lost the checked taskgroup duration");
   require(on1x::emit_c(within,"within-test").find("on1x_csp_group_enter_within(")!=std::string::npos,
           "timed group not emitted");
   reject([&]{(void)on1x::mir::lower(within);});
   for(int mutation=0;mutation<3;++mutation){
    std::size_t copy_budget=262144;auto forged=on1x::clone_unit(within,copy_budget);
    auto& group=forged.functions[0].body[1];
    if(mutation==0)group.value->type=on1x::Type::Bool;
    if(mutation==1)group.value->text="not_in_scope";
    if(mutation==2)group.unsafe_block=true;
    reject([&]{on1x::csp::verify(forged,true);});
    reject([&]{(void)on1x::emit_c(forged,"forged-within");});
   }
   constexpr auto mixed_source=R"(fn main(){taskgroup{
    let (p,input,output,error)=launch capture `/bin/cat`;
    let c=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split c;
    poll{send input <- 255u8=>println(1),closed input=>println(-1),
         recv output as byte=>println(u8_to_bits(byte)),closed output=>println(0),
         recv rx as value=>println(value),closed rx=>println(-2),default=>println(2)};
    cancel p;
   }})";
   auto mixed_wire=on1x::meta::encode(on1x::parse(on1x::scan(mixed_source,"mixed-poll")));
   auto mixed_unit=on1x::meta::decode(mixed_wire);on1x::check(mixed_unit);
   on1x::csp::verify(mixed_unit,true);
   auto mixed_poll=find_poll(mixed_unit.functions[0].body);
   require(mixed_poll&&mixed_poll->children.size()==4,"mixed poll lost offers");
   auto recv=std::find(mixed_poll->type.labels().begin(),mixed_poll->type.labels().end(),"Recv1");
   require(recv!=mixed_poll->type.labels().end(),"missing byte variant");
   auto byte_index=static_cast<std::size_t>(recv-mixed_poll->type.labels().begin());
   require(mixed_poll->type.elements()[byte_index]==on1x::Type::U8,"byte binder was widened");
   require(on1x::emit_c(mixed_unit,"mixed-poll").find("on1x_csp_select(")!=std::string::npos,"mixed poll not emitted");
   reject([&]{(void)on1x::mir::lower(mixed_unit);});
   for(int mutation=0;mutation<5;++mutation){
    std::size_t copy_budget=262144;
    auto forged=on1x::clone_unit(mixed_unit,copy_budget);auto p=find_poll(forged.functions[0].body);
    require(p!=nullptr,"missing mutation target");
    if(mutation==0){
     auto fields=p->type.elements();fields[byte_index]=on1x::Type::I64;
     p->type=on1x::Type::data(on1x::Type::Sum,std::move(fields),p->type.labels());
    }else if(mutation==1)p->children[0]->children[1]->type=on1x::Type::I64;
    else if(mutation==2)p->children[1]->children[0]->type=on1x::Type::Receiver;
    else if(mutation==3)p->children[1]->text="offer_send";
    else p->children[0]->children[1].reset();
    reject([&]{on1x::csp::verify(forged,true);});
    reject([&]{(void)on1x::emit_c(forged,"forged-mixed-poll");});
   }
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
   forged_process=on1x::clone_unit(external_unit,budget);
    forged_process.functions[0].body[0].body[0].value->children[0]->text="\"/bad\\\"";
   reject([&]{on1x::csp::verify(forged_process,true);});
   reject([&]{(void)on1x::emit_c(forged_process,"forged-process-string");});
   constexpr auto capture_source=R"(fn main(){taskgroup{
    let (child,input,output,error)=launch capture `/bin/echo "two words" escaped\ word`;
    close input;
    match <-output with{Some(byte)=>println(u8_to_bits(byte)),None(_)=>println(0)};
    println(cancel child);
   }})";
   auto capture_wire=on1x::meta::encode(on1x::parse(on1x::scan(capture_source,"capture-test")));
   auto capture_unit=on1x::meta::decode(capture_wire);on1x::check(capture_unit);
   on1x::csp::verify(capture_unit,true);
   const auto& capture=*capture_unit.functions[0].body[0].body[0].value;
   require(capture.kind==on1x::Expr::Kind::Csp&&capture.text=="launch_capture","captured shell command not canonicalized");
   require(capture.children.size()==3&&capture.children[1]->text=="\"two words\""&&capture.children[2]->text=="\"escaped word\"","shell argv parser did not preserve words");
   require(capture.type==on1x::Type::tuple({on1x::Type::ExternalFiber,on1x::Type::ByteSender,on1x::Type::ByteReceiver,on1x::Type::ByteReceiver}),"captured stream tuple type mismatch");
   auto capture_c=on1x::emit_c(capture_unit,"capture-test");
   require(capture_c.find("on1x_csp_launch_capture(")!=std::string::npos,"captured process spawn not emitted");
   require(capture_c.find("on1x_csp_stream_receive(")!=std::string::npos,"captured byte receive not emitted");
   reject([&]{(void)on1x::mir::lower(capture_unit);});
   auto forged_capture=on1x::clone_unit(capture_unit,budget);
   forged_capture.functions[0].body[0].body[0].value->type=on1x::Type::ExternalFiber;
   reject([&]{on1x::csp::verify(forged_capture,true);});
   reject([&]{(void)on1x::emit_c(forged_capture,"forged-capture");});
  constexpr auto ready_source=R"(fn main(){taskgroup{
   let child=launch{println(999);42};cancel child;
  }})";
  auto ready_unit=on1x::meta::decode(on1x::meta::encode(on1x::parse(on1x::scan(ready_source,"ready-test"))));
  on1x::check(ready_unit);on1x::csp::verify(ready_unit,true);
   require(on1x::emit_c(ready_unit,"ready-test").find("on1x_csp_cancel(")!=std::string::npos,"consuming cancellation not emitted");
 std::cout<<"canonical concurrency verification and source-wire round-trip PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

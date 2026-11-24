#include <on1x/backend/mir.hpp>
#include <iostream>
#include <limits>

namespace {
namespace m=on1x::mir;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

template<class F>void rejected(F operation,std::string_view reason){
 try{operation();}catch(const on1x::Error& e){
  if(std::string_view(e.what()).find(reason)==std::string_view::npos)
   throw std::runtime_error("expected "+std::string(reason)+", got "+e.what());
  return;
 }
 throw std::runtime_error("expected rejection: "+std::string(reason));
}

m::Module lower(const std::string& source){
 auto unit=on1x::parse(on1x::scan(source,"mir-directives.on1x"));
 on1x::check(unit);
 return m::lower(unit);
}

std::string v4_wire(const std::string& legacy,std::string_view directives){
 auto wire=legacy;
 wire.replace(std::string_view("(on1x.mir ").size(),1,"4");
 wire.insert(wire.size()-2," "+std::string(directives));
 return wire;
}
}

int main(){
 try{
  const std::string old_tail=R"( 1 ((function 2 "main" void (at "test" 0 1 1) () ((block 3 4 (at "test" 0 1 1) () () (exit 5 return 0 4 () (at "test" 0 1 1))))))))";
  for(unsigned version:{1u,2u,3u}){
   auto old_wire="(on1x.mir "+std::to_string(version)+old_tail+'\n';
   auto old=m::deserialize(old_wire);
   require(old.version==version&&old.directives.empty(),"legacy wire gained a directive section");
   require(m::serialize(old)==old_wire,"legacy wire changed");
   old.directives.push_back({"","","hint","kernel-hot"});
   rejected([&]{m::serialize(old);},"directives require MIR version 4");
   old_wire.insert(old_wire.size()-2," ()");
   rejected([&]{m::deserialize(old_wire);},"fields");
  }

  auto module=lower("fn twice(x:i64)->i64{return x*2;}fn main(){println(twice(21));println(1.5f32+2f32);}");
  require(module.version==m::numeric_format_version,"numeric lowering changed the legacy version");
  auto legacy=m::serialize(module);
  require(legacy.find(" numeric ")!=std::string::npos,"fixture did not lower a numeric opcode");
  auto before=m::evaluate(module);
  require(before.status==m::Status::Returned&&before.output=="42\n0x1.c00000p+1\n","fixture execution mismatch");
  const auto function=module.functions.front().id;
  module.version=m::directive_format_version;
  auto empty_wire=v4_wire(legacy,"()");
  require(m::serialize(module)==empty_wire&&m::deserialize(empty_wire).directives.empty(),"empty v4 list lost");
  module.directives={
   {"","x86_64-linux-gnu","hint","kernel-hot",function,true},
   {"mir-c11","","rewrite","fold-identity",0,false},
   {"mir-archon","rv64-linux-gnu","hint","local-memory",0,false},
   {"","","hint","canonicalized",0,false},
   {"mir-c11","x86_64-linux-gnu","hint","private-code",function,true},
  };
  const auto field_text=std::string(R"(((directive "" "x86_64-linux-gnu" "hint" "kernel-hot" )")+
   std::to_string(function)+R"( true) (directive "mir-c11" "" "rewrite" "fold-identity" 0 false))"
   R"( (directive "mir-archon" "rv64-linux-gnu" "hint" "local-memory" 0 false))"
   R"( (directive "" "" "hint" "canonicalized" 0 false))"
   R"( (directive "mir-c11" "x86_64-linux-gnu" "hint" "private-code" )"+
   std::to_string(function)+R"( true)))";
  auto wire=m::serialize(module);
  require(wire==v4_wire(legacy,field_text),"v4 wire changed function payload or directive order");
  auto decoded=m::deserialize(wire);
  require(decoded.directives.size()==5&&decoded.directives.front().function==function&&
          decoded.directives.front().speculative&&decoded.directives.front().backend.empty()&&
          decoded.directives.front().target=="x86_64-linux-gnu","directive fields did not round-trip");
  require(m::structurally_equal(module,decoded)&&m::serialize(decoded)==wire,"noncanonical v4 round-trip");
  auto after=m::evaluate(decoded);
  require(after.status==before.status&&after.output==before.output&&after.steps==before.steps,
          "advisory metadata changed reference execution");
  auto no_directives=decoded;
  no_directives.directives.clear();
  require(!m::structurally_equal(decoded,no_directives),"structural equality ignored directives");

  auto decode=[&](std::string_view fields){return m::deserialize(v4_wire(legacy,fields),"malformed-directives.mir");};
  auto missing=legacy;
  missing[std::string_view("(on1x.mir ").size()]='4';
  rejected([&]{m::deserialize(missing);},"fields");
  rejected([&]{decode(R"("directives")");},"expected list");
  rejected([&]{decode(R"(((hint "" "" "hint" "kernel-hot" 0 false)))");},"expected directive");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 0)))");},"expected directive");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 0 true extra)))");},"expected directive");
  rejected([&]{decode(R"(((directive c11 "" "hint" "kernel-hot" 0 false)))");},"expected string");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" "0" false)))");},"expected symbol");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" -1 false)))");},"out-of-range integer");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 18446744073709551616 false)))");},"out-of-range integer");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 0 "true")))");},"expected symbol");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 0 1)))");},"directive boolean");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 0 False)))");},"directive boolean");
  rejected([&]{decode(R"(((directive "bad\x00id" "" "hint" "kernel-hot" 0 false)))");},"directive selector");
  rejected([&]{decode(R"(((directive "" "x86_64 linux" "hint" "kernel-hot" 0 false)))");},"directive selector");
  rejected([&]{decode(R"(((directive "" "" "execute" "kernel-hot" 0 false)))");},"directive kind");
  rejected([&]{decode(R"(((directive "" "" "rewrite" "kernel-hot" 0 false)))");},"rewrite action");
  rejected([&]{decode(R"(((directive "" "" "hint" "fold-identity" 0 false)))");},"hint action");
  rejected([&]{decode(R"(((directive "" "" "hint" "kernel-hot" 1 false)))");},"directive function");
  auto future=wire;
  future[std::string_view("(on1x.mir ").size()]='5';
  rejected([&]{m::deserialize(future);},"unsupported format version");

  auto malformed=[&](auto change,std::string_view reason){
   auto candidate=module;
   change(candidate);
   rejected([&]{m::verify(candidate);},reason);
  };
  malformed([](auto& x){x.directives[0].target.assign(129,'a');},"directive selector");
  malformed([](auto& x){x.directives[0].backend="*";},"directive selector");
  malformed([](auto& x){x.directives[0].function=x.functions.front().blocks.front().id;},"directive function");
  malformed([](auto& x){x.directives[0].function=std::numeric_limits<m::Id>::max();},"directive function");
  malformed([](auto& x){x.directives.push_back(x.directives.front());},"duplicate directive");
  malformed([](auto& x){
   for(auto& f:x.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)
    if(i.op==m::Op::Numeric){i.primitive="unrecognized-primitive";return;}
   throw std::runtime_error("missing numeric fixture");
  },"unknown numeric primitive");

  // This fixture has five identity nodes and no instructions. Its v4
  // directive consumes a node and work for each of its string bytes.
  auto budget=m::deserialize("(on1x.mir 4"+old_tail.substr(0,old_tail.size()-1)+" ())");
  m::verify(budget,{5,1000});
  budget.directives={{std::string(128,'a'),"","hint","kernel-hot"}};
  rejected([&]{m::verify(budget,{5,1000});},"node budget");
  rejected([&]{m::verify(budget,{6,128});},"analysis budget");
  m::verify(budget,{6,1000});
  require(m::structurally_equal(budget,m::deserialize(m::serialize(budget))),"max-size selector did not round-trip");
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

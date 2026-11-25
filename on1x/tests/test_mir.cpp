#include <on1x/backend/mir.hpp>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <limits>
#include <bit>
#if defined(__linux__) && defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace {
namespace m=on1x::mir;
using on1x::Type;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
m::Module lower(const std::string& source){auto u=on1x::parse(on1x::scan(source,"mir-test.on1x"));on1x::check(u);return m::lower(u);}
template<class F>void rejected(F operation,const std::string& reason,const on1x::Location* at=nullptr){
 try{operation();}catch(const on1x::Error& e){
  if(std::string(e.what()).find(reason)==std::string::npos)throw std::runtime_error("expected "+reason+", got "+e.what());
  if(at)require(e.where.file==at->file&&e.where.offset==at->offset&&e.where.line==at->line&&
                e.where.column==at->column,"diagnostic has wrong source location");
  return;
 }
 throw std::runtime_error("expected rejection: "+reason);
}
template<class F>void malformed(const m::Module& source,F change,const std::string& reason){
 auto candidate=source;change(candidate);rejected([&]{m::verify(candidate);},reason);
}
void rethread(m::Block& b){
 auto token=b.effect;
 for(auto& i:b.instructions)if(m::effect(i.op)!=m::Effect::Pure){i.effect_in=token;token=i.effect_out;}
 b.exit.effect=token;
}
void call_signature_budget(){
 const on1x::Location at{"m"},call_at{"m",4,2,3};
 m::Module module;
 module.functions={
  {2,"main",Type::Void,at,{},{{3,4,at,{},
    {{5,Type::Void,m::Op::Call,{},0,8,4,6,call_at}},{7,m::Exit::Return,0,6,{},at}}}},
  {8,"callee",Type::Void,at,{},{{9,10,at,{},{},{11,m::Exit::Return,0,10,{},at}}}}
 };
 // 37 work units verifies the valid no-argument call, including the result
 // and list checks. Preserve that charge and test exhaustion at 36.
 m::verify(module,{65536,37});
 rejected([&]{m::verify(module,{65536,36});},"analysis budget");
 auto& parameters=module.functions.back().blocks.front().parameters;
 for(m::Id id=12;id<12+1024;++id)parameters.push_back({id,Type::I64,at});
 // 24 covers indexing and reachability up to the forward call, but not
 // a type comparison or a walk of the callee's parameter list.
 rejected([&]{m::verify(module,{65536,24});},"opcode type/arity mismatch",&call_at);
}
}
int main(int argc,char** argv){
 try{
  auto module=lower("fn twice(x:i64)->i64{return x*2;} fn main(){var n=0;while n<4{println(twice(n));n=n+1;} assert(n==4);}");
  auto wire=m::serialize(module);auto decoded=m::deserialize(wire);
  require(m::structurally_equal(module,decoded),"round-trip changed MIR");
  require(wire==m::serialize(decoded),"serialization is not canonical");
  require(m::emit_c(module)==m::emit_c(decoded),"round-trip changed executable C");
  require(m::cfg_dot(module)==m::cfg_dot(decoded),"round-trip changed CFG");
  auto execution=m::evaluate(decoded);
  require(execution.status==m::Status::Returned&&execution.output=="0\n2\n4\n6\n","loop/call evaluation mismatch");
  require(m::evaluate(module,"twice",{21}).value==42,"parameter evaluation mismatch");
  require(m::evaluate(module,"twice",{std::numeric_limits<std::int64_t>::min()}).status==m::Status::Trapped,"multiplication overflow lost");
  rejected([&]{m::evaluate(module,"twice",{});},"arity");
  rejected([&]{m::evaluate(module,"missing");},"not found");
  auto boolean=lower("fn flip(x:bool)->bool{return !x;} fn main(){}");
  rejected([&]{m::evaluate(boolean,"flip",{2});},"bool argument");
   require(m::evaluate(module,"main",{},1).status==m::Status::BudgetExhausted,"step budget not enforced");
   auto extended=lower("fn flip((x,b):(f64,bool))->(bool,f64){return(b,-x);}fn main(){let(x,y)=flip((1.5,true));println(x);println(y);}");
   require(extended.version==2,"float/tuple MIR did not use version 2");
   require(module.version==1,"existing scalar lowering changed its version");
   require(m::structurally_equal(extended,m::deserialize(m::serialize(extended))),"extended MIR round-trip lost types/bits");
   require(m::evaluate(extended).output=="true\n-0x1.8000000000000p+0\n","float/tuple execution mismatch");
#if defined(__linux__) && defined(__x86_64__) && defined(__SSE2__)
   for(unsigned mode:{0x2000u,0x8000u,0x0040u,0x0200u}){
    auto saved=_mm_getcsr();_mm_setcsr(mode==0x0200u?saved&~mode:saved|mode);
    auto failure=m::evaluate(extended);_mm_setcsr(saved);
    require(failure.status==m::Status::Trapped&&failure.diagnostic=="unsupported f64 environment","evaluator ignored incompatible float environment");
   }
#endif
   std::vector<m::RuntimeValue> typed{{0,{{std::bit_cast<std::int64_t>(1.5),{}},{1,{}}}}};
   auto aggregate=m::evaluate_values(extended,"flip",typed);
   require(aggregate.type==Type::tuple({Type::Bool,Type::F64})&&aggregate.data.elements.size()==2&&aggregate.data.elements[0].scalar==1&&aggregate.data.elements[1].scalar==std::bit_cast<std::int64_t>(-1.5),"typed aggregate API lost values");
   auto bad_argument=typed;bad_argument[0].elements[1].scalar=2;
   rejected([&]{m::evaluate_values(extended,"flip",bad_argument);},"bool argument");
   rejected([&]{m::evaluate(extended,"flip",{0});},"tuple argument shape");
   malformed(extended,[](auto& m){m.version=1;},"unsupported");
   auto change_op=[](auto& m,m::Op op,auto change){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==op){change(i);return;}throw std::runtime_error("missing fixture opcode");};
   auto forward=extended;
   std::reverse(forward.functions.begin(),forward.functions.end());
   require(m::evaluate(m::deserialize(m::serialize(forward))).output=="true\n-0x1.8000000000000p+0\n",
           "forward tuple call failed after round-trip");
   malformed(forward,[&](auto& m){change_op(m,m::Op::Call,[](auto& i){auto arg=i.operands.front();i.operands.push_back(arg);});},"opcode type/arity mismatch");
   malformed(forward,[](auto& m){m.functions.back().blocks.front().parameters.front().type=Type::tuple({Type::F64,Type::I64});},
             "opcode type/arity mismatch");
   malformed(forward,[&](auto& m){change_op(m,m::Op::Call,[](auto& i){i.type=Type::I64;});},"opcode type/arity mismatch");
   call_signature_budget();
   malformed(extended,[&](auto& m){change_op(m,m::Op::Extract,[](auto& i){i.immediate=-1;});},"index out of bounds");
   malformed(extended,[&](auto& m){change_op(m,m::Op::Extract,[](auto& i){i.type=Type::I64;});},"extract result type");
   malformed(extended,[&](auto& m){change_op(m,m::Op::Tuple,[](auto& i){i.operands.pop_back();});},"tuple field type/arity");
   malformed(extended,[&](auto& m){change_op(m,m::Op::FloatNeg,[](auto& i){i.op=m::Op::Neg;});},"opcode type/arity");
   malformed(extended,[&](auto& m){change_op(m,m::Op::FloatNeg,[](auto& i){i.effect_in=0;});},"effect ordering");
    malformed(extended,[](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==m::Op::Constant&&i.type==Type::F64)i.immediate=INT64_C(0x7ff0000000000001);},"noncanonical f64 NaN");
   auto numeric=lower("fn small(x:u8)->u8{return x;}fn single(x:f32)->f32{return x;}fn main(){println(255u8);println(1.5f32+2f32);}");
   require(numeric.version==3,"numeric types did not select MIR v3");
   require(m::evaluate(numeric).output=="255\n0x1.c00000p+1\n","numeric v3 execution mismatch");
   require(m::structurally_equal(numeric,m::deserialize(m::serialize(numeric))),"numeric v3 round-trip mismatch");
   rejected([&]{m::evaluate(numeric,"small",{256});},"unsigned argument out of range");
   rejected([&]{m::evaluate(numeric,"single",{INT64_C(0x7fc00001)});},"binary32 argument payload");
   rejected([&]{m::evaluate(numeric,"single",{-1});},"binary32 argument payload");
   malformed(numeric,[](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==m::Op::Constant&&i.type==Type::U8)i.immediate=256;},"unsigned constant out of range");
   malformed(numeric,[](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==m::Op::Constant&&i.type==Type::F32)i.immediate=INT64_C(0x7fc00001);},"noncanonical f32 NaN");
   malformed(numeric,[&](auto& m){change_op(m,m::Op::Numeric,[](auto& i){i.primitive="unknown_native";});},"unknown numeric primitive");
   malformed(numeric,[&](auto& m){change_op(m,m::Op::Numeric,[](auto& i){i.effect_in=0;});},"effect ordering");
   malformed(numeric,[&](auto& m){change_op(m,m::Op::Numeric,[](auto& i){i.operands.pop_back();});},"type/arity mismatch");
  auto recursive=lower("fn again()->i64{return again();} fn main(){again();}");
  auto exhausted=m::evaluate(recursive,"main",{},100000,8);
  require(exhausted.status==m::Status::BudgetExhausted&&exhausted.diagnostic.find("call-depth")!=std::string::npos,"recursion budget not enforced");
  auto infinite=lower("fn main(){while true{}}");
  require(m::evaluate(infinite,"main",{},30).status==m::Status::BudgetExhausted,"loop budget not enforced");
  std::string large="fn again()->i64{";
  for(int n=0;n<1100;++n)large+="let x"+std::to_string(n)+"=1;";
  large+="return again();}fn main(){again();}";
   auto storage=m::evaluate(lower(large));
   require(storage.status==m::Status::BudgetExhausted&&storage.diagnostic.find("frame-storage")!=std::string::npos,"frame-storage budget not enforced");
   std::string tuple_type="(",tuple_value="(";
   for(int n=0;n<1100;++n){if(n){tuple_type+=",";tuple_value+=",";}tuple_type+="i64";tuple_value+="0";}
   tuple_type+=")";tuple_value+=")";
   auto aggregate_storage=lower("fn again(x:"+tuple_type+"){again(x);}fn main(){again("+tuple_value+");}");
   auto aggregate_exhausted=m::evaluate(aggregate_storage);
   require(aggregate_exhausted.status==m::Status::BudgetExhausted&&aggregate_exhausted.diagnostic.find("frame-storage")!=std::string::npos,"aggregate storage counted only root values, not fields");
   malformed(extended,[](auto& m){m.functions[0].result=Type::tuple({Type::Buffer,Type::F64});},"unsupported");
  rejected([&]{m::verify(module,{1,1000});},"node budget");
  rejected([&]{m::verify(module,{65536,1});},"analysis budget");
  auto u=on1x::parse(on1x::scan("fn main(){println(42);}","transaction.on1x"));on1x::check(u);
  auto original=on1x::emit_c(u,"");rejected([&]{m::lower(u,{1,1});},"budget");
  require(original==on1x::emit_c(u,""),"failed lowering mutated HIR");
  require(m::serialize(m::lower(u))==m::serialize(m::lower(u)),"unstable lowering IDs");

  auto simple=lower("fn main(){println(1+2);}");
  malformed(simple,[](auto& m){m.version=999;},"version");
  malformed(simple,[](auto& m){m.id=0;},"identity");
  malformed(simple,[](auto& m){m.functions[0].id=m.id;},"identity");
  malformed(simple,[](auto& m){m.functions[0].blocks.clear();},"entry");
  malformed(simple,[](auto& m){m.functions[0].result=Type::String;},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].where.line=0;},"source location");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].type=Type::code(Type::I64);},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.kind=m::Exit::Invalid;},"terminator");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.effect=0;},"effects");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].effect_in=0;},"effect ordering");
  malformed(simple,[](auto& m){auto& i=m.functions[0].blocks[0].instructions;std::swap(i[2],i[3]);},"use before definition");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].op=static_cast<m::Op>(999);},"unknown opcode");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[0].type=Type::Json;},"unsupported");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[1].type=Type::Bool;},"bool constant");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].operands[0]=99999;},"non-value");
  malformed(simple,[](auto& m){auto& i=m.functions[0].blocks[0].instructions[2];i.operands[0]=i.id;},"use before definition");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].type=Type::Bool;},"type/arity");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].immediate=1;},"unexpected immediate");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].instructions[2].target=1;},"unexpected target");
  malformed(simple,[](auto& m){auto& b=m.functions[0].blocks[0];b.instructions[0].effect_in=b.effect;},"pure operation");
  malformed(simple,[](auto& m){m.functions[0].blocks[0].exit.value=m.functions[0].blocks[0].instructions[0].id;},"unexpected terminator value");
  malformed(module,[](auto& m){m.functions[0].blocks[0].exit.value=0;},"non-value");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges[0].block=9999;},"unknown CFG");
  malformed(module,[](auto& m){auto& b=m.functions[1].blocks[0];b.exit.edges[0].block=b.id;},"entry block cannot");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges.clear();},"edge count");
  malformed(module,[](auto& m){m.functions[1].blocks[0].exit.edges[0].arguments={99};},"argument arity");
  malformed(module,[](auto& m){m.functions[1].blocks[1].exit.value=m.functions[1].blocks[0].instructions[0].id;},"requires bool");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Call)i.target=99999;},"call target");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Call)i.operands.clear();},"type/arity");
  malformed(module,[](auto& m){for(auto& b:m.functions[1].blocks)for(auto& i:b.instructions)if(i.op==m::Op::Load)i.target=99999;},"local slot");
  malformed(module,[](auto& m){auto& b=m.functions[1].blocks[0];b.instructions.pop_back();rethread(b);},"uninitialized slot");

  auto branches=lower("fn main(){if true{println(11);}else{println(22);}println(33);}");
  malformed(branches,[](auto& m){auto& b=m.functions[0].blocks;b[2].instructions[1].operands[0]=b[1].instructions[0].id;},"does not dominate");
  malformed(branches,[](auto& m){auto& b=m.functions[0].blocks;b[0].exit.edges[1]=b[0].exit.edges[0];},"unreachable");
  auto join=branches;
  auto& diamond=join.functions.front();
  diamond.slots.push_back({100,Type::I64,diamond.where});
  auto& join_block=diamond.blocks[3];
  join_block.instructions.insert(join_block.instructions.begin(),
      {101,Type::I64,m::Op::Load,{},0,100,0,102,join_block.where});
  join_block.instructions.back().operands={101};
  rethread(join_block);
  for(std::size_t arm:{1u,2u}){
   auto& b=diamond.blocks[arm];
   b.instructions.push_back({103+2*arm,Type::Void,m::Op::Store,{b.instructions.front().id},
                             0,100,0,104+2*arm,b.where});
   rethread(b);
  }
  require(m::evaluate(m::deserialize(m::serialize(join))).output=="11\n11\n","join lost definitely initialized slot");
  diamond.blocks.front().instructions.front().immediate=0;
  require(m::evaluate(join).output=="22\n22\n","join lost alternate predecessor's store");
  malformed(join,[](auto& m){
   auto& arm=m.functions.front().blocks[2];arm.instructions.pop_back();rethread(arm);
  },"load from possibly uninitialized slot");
  // Even without instructions, building the slot universe and seeding the
  // fixed point take work. Use one-byte locations to isolate the analysis:
  // indexing and reachability take 40 units, before the two eight-slot sets.
  on1x::Location slot_at{"m",0,1,1};
  m::Module unused_slots;
  m::Function unused_function{2,"main",Type::Void,slot_at,{},
      {{3,4,slot_at,{},{},{5,m::Exit::Return,0,4,{},slot_at}}}};
  for(m::Id id=100;id<108;++id)unused_function.slots.push_back({id,Type::I64,slot_at});
  unused_slots.functions.push_back(std::move(unused_function));
  m::verify(unused_slots);
  rejected([&]{m::verify(unused_slots,{65536,55});},"analysis budget");
  const auto latch=lower(R"(
   fn step(n:i64)->i64{println(n);return n+1;}
   fn main(){for(var i=0;i<3;i=step(i)){println(10+i);continue;}println(42);}
  )");
  const auto stepped=m::evaluate(m::deserialize(m::serialize(latch)));
  require(stepped.status==m::Status::Returned&&stepped.output=="10\n0\n11\n1\n12\n2\n42\n",
          "continue did not evaluate the latch once in effect order");
  malformed(latch,[&](auto& m){change_op(m,m::Op::Call,[](auto& i){i.effect_in=0;});},"effect ordering");
   auto logic=lower("fn main(){println(true&&false);}");
   require(m::evaluate(logic).output=="false\n","logical value lowering changed its result");
  auto effect_order=lower("fn main(){println(1);println(2);}");
  std::swap(effect_order.functions[0].blocks[0].instructions[1],effect_order.functions[0].blocks[0].instructions[2]);
  m::verify(effect_order);
  malformed(effect_order,[](auto& m){auto& i=m.functions[0].blocks[0].instructions;std::swap(i[2],i[3]);},"effect ordering");

  rejected([&]{m::deserialize("(on1x.mir 1 1 ()) extra");},"trailing input");
  rejected([&]{m::deserialize("(on1x.mir 999 1 ())");},"version");
  rejected([&]{m::deserialize("(on1x.mir 1 -1 ())");},"integer");
  rejected([&]{m::deserialize("(on1x.mir 1 1 () unknown)");},"fields");
  auto unknown=m::serialize(simple);unknown.replace(unknown.find("add.checked"),11,"unknown.op");
  rejected([&]{m::deserialize(unknown);},"unknown opcode");
  auto huge=std::string(4*1024*1024+1,' ');rejected([&]{m::deserialize(huge);},"byte limit");

  // Independently authored loop phis: swap must copy simultaneously. Loop
  // values do not depend on vector/block order or process-local identities.
  on1x::Location at{"phi.on1x",0,1,1};m::Module phi; m::Function f;f.id=2;f.name="main";f.where=at;
  m::Block entry;entry.id=3;entry.effect=4;entry.where=at;
  entry.instructions={{5,Type::I64,m::Op::Constant,{},10,0,0,0,at},{6,Type::I64,m::Op::Constant,{},20,0,0,0,at},{7,Type::Bool,m::Op::Constant,{},1,0,0,0,at}};
  entry.exit={8,m::Exit::Jump,0,4,{{9,{5,6,7}}},at};
  m::Block loop;loop.id=9;loop.effect=10;loop.where=at;loop.parameters={{11,Type::I64,at},{12,Type::I64,at},{13,Type::Bool,at}};
  loop.instructions={{14,Type::Bool,m::Op::Constant,{},0,0,0,0,at}};
  loop.exit={15,m::Exit::Branch,13,10,{{9,{12,11,14}},{16,{11,12}}},at};
  m::Block done;done.id=16;done.effect=17;done.where=at;done.parameters={{18,Type::I64,at},{19,Type::I64,at}};
  done.instructions={{20,Type::Void,m::Op::Print,{18},0,0,17,21,at},{22,Type::Void,m::Op::Print,{19},0,0,21,23,at}};
  done.exit={24,m::Exit::Return,0,23,{},at};f.blocks={entry,done,loop};phi.functions={f};
   m::verify(phi);require(m::evaluate(phi).output=="20\n10\n","parallel loop phi copy failed");
    auto slot_loop=phi;
    auto& slot_function=slot_loop.functions.front();
    slot_function.slots.push_back({25,Type::I64,at});
    auto& header=slot_function.blocks[2];
    // This is also the latch: the store on its self-edge cannot initialize the
    // load at the top of the first iteration.
    header.instructions.insert(header.instructions.begin(),{26,Type::I64,m::Op::Load,{},0,25,0,27,at});
    header.instructions.push_back({28,Type::Void,m::Op::Store,{11},0,25,0,29,at});
    rethread(header);
    rejected([&]{m::verify(slot_loop);},"load from possibly uninitialized slot");
    auto& preheader=slot_function.blocks.front();
    preheader.instructions.push_back({30,Type::Void,m::Op::Store,{5},0,25,0,31,at});
    rethread(preheader);
    m::verify(slot_loop);
    std::swap(slot_function.blocks[1],slot_function.blocks[2]);
    require(m::evaluate(m::deserialize(m::serialize(slot_loop))).output=="20\n10\n",
            "preheader store did not initialize the slot for the loop");
    slot_loop.functions.push_back({32,"other",Type::Void,at,{{33,Type::I64,at}},
                                  {{34,35,at,{},{},{36,m::Exit::Return,0,35,{},at}}}});
    m::verify(slot_loop);
    malformed(slot_loop,[&](auto& m){change_op(m,m::Op::Load,[](auto& i){i.target=33;});},"unknown local slot");
   // Test the edge/parameter type invariant on an explicit phi. Source logical
   // expressions may legitimately lower through canonical initialized slots.
   malformed(phi,[](auto& m){m.functions[0].blocks[1].parameters[0].type=Type::Bool;},"argument type");
  if(argc==2){std::ofstream out(argv[1]);out<<m::emit_c(phi);require(bool(out),"cannot write phi C fixture");}
  auto product=Type::tuple({Type::I64,Type::F64});phi.version=2;
  auto& pe=phi.functions[0].blocks[0];
  pe.instructions.push_back({25,Type::F64,m::Op::Constant,{},0,0,0,0,at});
  pe.instructions.push_back({26,Type::F64,m::Op::Constant,{},INT64_MIN,0,0,0,at});
  pe.instructions.push_back({27,product,m::Op::Tuple,{5,25},0,0,0,0,at});
  pe.instructions.push_back({28,product,m::Op::Tuple,{6,26},0,0,0,0,at});
  pe.exit.edges[0].arguments={27,28,7};
  auto& pl=phi.functions[0].blocks[2];pl.parameters[0].type=pl.parameters[1].type=product;
  auto& pd=phi.functions[0].blocks[1];pd.parameters[0].type=pd.parameters[1].type=product;
  pd.instructions={{29,Type::I64,m::Op::Extract,{18},0,0,0,0,at},{30,Type::F64,m::Op::Extract,{18},1,0,0,0,at},
   {31,Type::Void,m::Op::Print,{29},0,0,17,32,at},{33,Type::Void,m::Op::Print,{30},0,0,32,34,at},
   {35,Type::I64,m::Op::Extract,{19},0,0,0,0,at},{36,Type::F64,m::Op::Extract,{19},1,0,0,0,at},
   {37,Type::Void,m::Op::Print,{35},0,0,34,38,at},{39,Type::Void,m::Op::Print,{36},0,0,38,40,at}};
  pd.exit.effect=40;
  m::verify(phi);require(m::evaluate(phi).output=="20\n-0x0.0000000000000p+0\n10\n0x0.0000000000000p+0\n","aggregate parallel phi copy lost signed zero");
  require(m::structurally_equal(phi,m::deserialize(m::serialize(phi))),"aggregate phi serialization failed");
  if(argc==2){std::ofstream out(std::string(argv[1])+".v2.c");out<<m::emit_c(phi);require(bool(out),"cannot write v2 phi C fixture");}
  std::cout<<"MIR verifier, round-trip, evaluator, budgets and transactional lowering PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

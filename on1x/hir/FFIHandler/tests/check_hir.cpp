#include <on1x/c11.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/clone.hpp>
#include <on1x/ffi_hir.hpp>
#include <on1x/loader.hpp>
#include <on1x/meta.hpp>
#include <on1x/sema.hpp>
#include "../../lower_mir.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace {
using namespace on1x;
namespace fs=std::filesystem;

void expect(bool value,const char* message){if(!value)throw std::runtime_error(message);}

template<class F> void reject(std::string_view id,F action,std::string_view detail={}){
 try{action();}
 catch(const Error& error){
  if(error.diagnostic_id!=id||!error.where.line||!error.where.column||
     std::string_view(error.what()).find(detail)==std::string_view::npos)throw;
  return;
 }
 throw std::runtime_error("expected diagnostic "+std::string(id));
}

template<class F> void reject_wire(F action){
 try{action();}
 catch(const std::runtime_error&){return;}
 throw std::runtime_error("untrusted wire accepted a foreign declaration");
}

Unit duplicate(const Unit& unit){
 std::size_t budget=10000;
 return clone_unit(unit,budget);
}

Function& find_foreign(Unit& unit,std::string_view member){
 for(auto& function:unit.functions)
  if(function.foreign&&function.foreign.descriptor()->member==member)return function;
 throw std::runtime_error("expected compiler-owned descriptor for "+std::string(member));
}

bool hook_called=false;
processor::FirestoneStats forbidden_optimizer(Unit&){
 hook_called=true;
 return {};
}
}

int main(int argc,char** argv) try{
 if(argc!=2)return 2;
 const fs::path root=argv[1];
 fs::create_directories(root);
 const auto input=root/"typed-ffi.on1x";
 LoaderOptions options;options.prelude=false;options.kernel_c11=true;options.ffi_stage=true;
 auto load=[&](std::string_view source){
  std::ofstream out(input);
  out<<source;out.close();
  if(!out)throw std::runtime_error("cannot write typed FFI test fixture");
  return load_unit(input,options).unit;
 };
 const auto source=R"(
import kernel[dynlib] mulib.clamp as A;
unsafe import kernel mulib.clamp as B;
fn main(){
 let x=A.add_i64(40,2);
 let y=unsafe { B.sub_i64(43,1) };
 println(x);
 println(y);
}
)";
 auto unit=load(source);
 diagnostic::Registry registry;
 analysis::Context context{diagnostic::Catalog::embedded(),registry,{},{}};
 processor::RuleSelection selection;
 selection.partial_eval=true;
 selection.firestone_hir=forbidden_optimizer;
 processor::Report report;
 check(unit,context,selection,&report);
 expect(!registry.has_errors()&&report.verified&&!hook_called,
        "unknown foreign effects reached optional HIR optimizer");
 auto& add=find_foreign(unit,"add_i64");
 auto& sub=find_foreign(unit,"sub_i64");
 expect(add.foreign.mode()==ffi::ForeignMode::LinkOnly&&
        sub.foreign.mode()==ffi::ForeignMode::UnsafeLinkOnly,"foreign staging confused lexical and link modes");
 expect(add.foreign.descriptor()->id!=sub.foreign.descriptor()->id,"same-signature descriptors share identity");
 expect(add.kernel_symbol.empty()&&sub.kernel_symbol.empty(),"v2 call routed into legacy symbol dispatch");
 auto& main=unit.functions.front();
 auto& region=*main.body.at(1).value;
 expect(region.kind==Expr::Kind::Region&&region.unsafe_region,"unsafe lexical region missing");
 auto& unsafe_call=*region.children.at(0);
 expect(unsafe_call.callee->foreign==sub.foreign,"direct call lost its descriptor");

 auto verified_copy=duplicate(unit);
 recheck_checked_hir(verified_copy);
 expect(find_foreign(verified_copy,"sub_i64").foreign==sub.foreign,"clone lost foreign identity");
  auto no_opts=load(source);
  processor::RuleSelection off;off.default_enabled=false;
  check(no_opts,context,off);
 reject("ON1X-KERNEL-PPX",[&]{(void)meta::encode(unit);});
 reject("ON1X-MIR-UNSUPPORTED",[&]{(void)mir::lower(unit);});
 reject("ON1X-MIR-UNSUPPORTED",[&]{(void)mir::lower_for_vm(unit);});
 reject("ON1X-FFI-VERIFY",[&]{(void)ffi::verify_foreign_declaration(add,"aarch64-linux-gnu");});

 const auto original_name=add.name;
 add.name="@ffi_link_0000000000000000000000000000000000000000000000000000000000000000";
 reject("ON1X-FFI-VERIFY",[&]{canonical::verify_values(unit);});
 add.name=original_name;
 const auto original_identity=add.foreign;
 add.foreign={};
 reject("ON1X-FFI-VERIFY",[&]{recheck_checked_hir(unit);});
 add.foreign=original_identity;
 add.params.at(0).type=Type::U64;
 reject("ON1X-FFI-VERIFY",[&]{(void)emit_c(unit,input.string());});
 add.params.at(0).type=Type::I64;
 add.kernel_link_only=true;
 reject("ON1X-FFI-VERIFY",[&]{canonical::verify_values(unit);});
 add.kernel_link_only=false;

 unsafe_call.callee->foreign=add.foreign;
 reject("ON1X-FFI-VERIFY",[&]{recheck_checked_hir(unit);});
 unsafe_call.callee->foreign=sub.foreign;
 unsafe_call.callee->reference=Expr::Reference::ScalarForeign;
 reject("ON1X-VALUES-VERIFY",[&]{recheck_checked_hir(unit);});
 unsafe_call.callee->reference=Expr::Reference::UnsafeForeign;
 region.unsafe_region=false;
 reject("ON1X-VALUES-VERIFY",[&]{(void)emit_c(unit,input.string());});
 region.unsafe_region=true;

 auto argument=std::move(unsafe_call.children.back());
 unsafe_call.children.pop_back();
 reject("ON1X-VALUES-VERIFY",[&]{canonical::verify_values(unit);});
 auto bad_argument=std::make_unique<Expr>();
 bad_argument->kind=Expr::Kind::Boolean;bad_argument->type=Type::Bool;
 bad_argument->where=argument->where;bad_argument->text="true";
 unsafe_call.children.push_back(std::move(bad_argument));
  reject("ON1X-VALUES-VERIFY",[&]{canonical::verify_values(unit);});
  unsafe_call.children.back()=std::move(argument);
  auto first=std::move(unsafe_call.children.front());
  auto no_result=std::make_unique<Expr>();
  no_result->kind=Expr::Kind::Region;no_result->where=first->where;
  no_result->type=Type::Void;no_result->noreturn=true;
  Stmt trap;trap.kind=Stmt::Kind::Trap;trap.where=first->where;
  no_result->body.push_back(std::move(trap));
  auto empty=std::make_unique<Expr>();
  empty->kind=Expr::Kind::Unit;empty->type=Type::Void;empty->where=first->where;
  no_result->children.push_back(std::move(empty));
  unsafe_call.children.front()=std::move(no_result);
  reject("ON1X-VALUES-VERIFY",[&]{canonical::verify_values(unit);},
         "canonical foreign call argument type mismatch");
  unsafe_call.children.front()=std::move(first);
  canonical::verify_values(unit);

 auto wire=meta::encode(parse(scan("extern fn boundary(a:i64)->i64;fn main(){}","wire")));
 auto& external=wire.items.at(2).items.at(0);
 expect(external.items.at(0).text=="Extern","wire negative fixture must name an extern");
 const auto external_name=external.items.at(1);
 external.items.at(1)=meta::string(add.name);
 reject_wire([&]{(void)meta::decode(wire);});
 external.items.at(1)=external_name;
 external.items.push_back(meta::list({meta::symbol("ForeignId"),meta::string(std::string(add.foreign.descriptor()->id.sha256))}));
 reject_wire([&]{(void)meta::decode(wire);});

 reject("ON1X-KERNEL-DIRECT",[&]{(void)load(
  "unsafe import kernel mulib.clamp as C;fn main(){let f=C.add_i64;}");});
 reject("ON1X-KERNEL-CONTEXT",[&]{(void)load(
  "unsafe import kernel mulib.clamp as C;fn main(){println(C.add_i64(1,2));}");});
 reject("ON1X-KERNEL-CONTEXT",[&]{(void)load(
  "unsafe import kernel mulib.clamp as C;"
  "fn main(){let f=unsafe { fn()->i64{return C.add_i64(1,2);} };}");});
 reject("ON1X-KERNEL-ALIAS",[&]{(void)load(
  "import kernel mulib.clamp as C;unsafe import kernel mulib.clamp as C;fn main(){}");});
 reject("ON1X-KERNEL-MEMBER",[&]{(void)load(
  "import kernel mulib.clamp as C;fn main(){println(C.sub_i64_typo(1,2));}");});
 auto shadow=load("unsafe import kernel mulib.clamp as C;"
                  "fn main(){if true {let C=42;println(C);} println(unsafe { C.add_i64(1,2) });}");
 check(shadow);
 expect(bool(find_foreign(shadow,"add_i64").foreign),"local shadowing changed enclosing alias");
  options.foreign_target="aarch64-linux-gnu";
  reject("ON1X-FFI-TARGET",[&]{(void)load(source);});
  options.foreign_target="x86_64-linux-gnu";
  options.ffi_stage=false; // retained rich path; no executable rich descriptors
  auto counter=load("unsafe import kernel mulib.container_core as C;"
                    "fn main(){println(unsafe { C.counter_add(1u64,2u64) });}");
  check(counter,context,off);
  const auto& add_counter=find_foreign(counter,"counter_add");
  expect(add_counter.foreign.mode()==ffi::ForeignMode::Unsafe&&
         add_counter.result==Type::U64&&add_counter.params.size()==2&&
         add_counter.params[0].type==Type::U64&&add_counter.params[1].type==Type::U64,
         "real inline declaration did not reach HIR as an exact U64 call");

  const std::string ipc_prefix=R"(
unsafe import kernel mulib.ipc_framed as Framed;
fn deadline(result: Framed.Result<i64,i64>) -> i64 { return 1000; }
fn consume(session: own<Framed.Session>) -> i64 { drop(move(session)); return 1000; }
fn exercise() -> Framed.Result<i64,i64> {
 let opened=unsafe { Framed.open_session() };
 let session=opened?;
 return unsafe { )";
  const std::string ipc_end=" }; } fn main() {}";
  auto released=load(ipc_prefix+
   "let d=deadline(Framed.poll(session,1000)); Framed.poll(session,d)"+ipc_end);
  check(released,context,off);
  expect(!registry.has_errors(),"sequential foreign call did not release its loan");
  for(const auto& [argument,detail]:{
      std::pair{"deadline(Framed.poll(session,1000))","overlapping exclusive borrow"},
      std::pair{"consume(move(session))","cannot consume"}}){
   auto conflicted=load(ipc_prefix+"Framed.poll(session,"+argument+")"+ipc_end);
   reject("ON1X-REF",[&]{check(conflicted,context,off);},detail);
  }

  const auto c11=emit_c(verified_copy,input.string());
  for(const auto* function:{&add,&sub})
   expect(c11.find("on1x_ffi_v2_"+std::string(function->foreign.descriptor()->id.sha256))!=std::string::npos,
          "staged C11 did not retain an unresolved descriptor-ID link");
  expect(c11.find("#include <mulib-clamp.h>")==std::string::npos,
         "staged C11 emitted a local native wrapper");
  expect(c11==emit_c(no_opts,input.string()),"optional HIR rewrites changed staged C11");

 std::cout<<"typed foreign HIR: exact identities and signatures, staged modes, lexical scopes, wire and backend guards\n";
 return 0;
}catch(const std::exception& error){
 std::cerr<<error.what()<<'\n';
 return 1;
}

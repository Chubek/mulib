#include <on1x/ast.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <on1x/sema.hpp>
#include "../hir/lower_mir.hpp"
#include "../src/ppx_internal.hpp"
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;
constexpr std::string_view prelude=R"(
record Key { hidden:i64 }
record Peer { hidden:i64 }
enum Choice { Empty, Number(i64) }
record Wrapper<T> { item:T }
enum Outer<T> { Wrap(T) }
fn create(n:i64) -> Key { return record<Key>{hidden:n}; }
fn create_peer(n:i64) -> Peer { return record<Peer>{hidden:n}; }
fn choose(n:i64) -> Choice { return Choice::Number(n); }
fn inside<T>(k:Key,x:T) -> T { assert(k.hidden>0); return x; }
fn read_key(k:Key) -> i64 { return inside(k,k.hidden); }
fn read_choice(v:Choice) -> i64 { return match v with { Empty()=>0, Number(n)=>n }; }
fn passthrough<T>(x:T) -> T { return x; }
)";

void seal(Unit& unit){
 // These are exactly the loader's post-resolution inputs. All Locations in
 // this fixture deliberately spell the provider's file, including main.
 unit.requires_module_boundary=true;
 unit.opaque_nominal_owners={{"Choice","provider"},{"Key","provider"},{"Peer","other"}};
 unit.opaque_function_owners={{"create","provider"},{"create_peer","other"},
                             {"choose","provider"},{"inside","provider"},
                             {"read_key","provider"},{"read_choice","provider"}};
}

Unit source(std::string_view body,std::string_view before={},bool sealed=true){
 auto unit=parse(scan(std::string(prelude)+std::string(before)+"\nfn main() { "+
                      std::string(body)+" }","provider.on1x"));
 if(sealed)seal(unit);
 return unit;
}

template<class Action>
void rejected(std::string_view label,std::string_view expected,Action&& action){
 try{action();}
 catch(const Error& error){
  if(!expected.empty()&&error.diagnostic_id!=expected)
   throw std::runtime_error(std::string(label)+": wrong diagnostic: "+
                            error.diagnostic_id+": "+error.what());
  return;
 }
 throw std::runtime_error(std::string(label)+": unexpectedly accepted");
}
}

int main(){
 try{
  auto working=source(R"(
   let a = passthrough(create(19));
   let p = record<Wrapper<Key>>{item:a};
   let e = Outer<Key>::Wrap(p.item);
   let v = match e with { Wrap(wrapped) => wrapped };
   let pair = (v, choose(23));
   let (k, c) = pair;
   println(read_key(k)+read_choice(c));
  )");
  check(working);
  auto generated_c=emit_c(working,"opaque-fixture");
  rejected("MIR C11", "ON1X-MIR-UNSUPPORTED", [&]{(void)mir::lower(working);});
  rejected("MIR VM", "ON1X-MIR-UNSUPPORTED", [&]{(void)mir::lower_for_vm(working);});

  auto negative=[](std::string_view label,std::string_view body,std::string_view before={}){
   rejected(label,"ON1X-INTERFACE-OPAQUE",[&]{auto unit=source(body,before);check(unit);});
  };
  negative("projection after alias","let a=create(1); let b=a; println(b.hidden);");
  negative("nested generic projection","println(record<Wrapper<Key>>{item:create(1)}.item.hidden);");
  negative("record constructor","let forged=record<Key>{hidden:1};");
  negative("record pattern","let stolen=match create(1) with { record{hidden:x}=>x };");
  negative("nested record pattern",
           "let stolen=match (create(1),0) with { (record{hidden:x},_)=>x };");
  negative("enum constructor","let forged=Choice::Number(1);");
  negative("enum nullary constructor","let forged=Choice::Empty();");
  negative("enum match","let stolen=match choose(1) with { Empty()=>0, Number(x)=>x };");
  negative("implicit variant payload shape",
           "let stolen=match Outer<Key>::Wrap(create(1)) with { Wrap()=>0 };");
  negative("propagate","let forged=choose(1)?;");
  negative("equality","let a=create(1); assert(a==a);");
  negative("box","let owner=box(create(1)); drop(owner);");
  negative("macro projection","println(steal!(create(1)));",
           "macro steal { rule ($x:Expression) => expression { $x.hidden }; }");
  rejected("nominal mismatch","",[]{auto unit=source("println(read_key(create_peer(1)));");check(unit);});
  rejected("unsupported managed layout","ON1X-INTERFACE-OPAQUE",[]{
   auto unit=source("", "record Managed { payload:string }");
   unit.opaque_nominal_owners.emplace("Managed","provider");
   check(unit);
  });

  // A PPX processor can synthesize an ordinary nominal wire declaration, but
  // cannot replace the trusted graph and inherit its representation authority.
  auto wire_source=parse(scan("record Key { hidden:i64 } "
                              "fn main(){let k=record<Key>{hidden:1};println(k.hidden);}",
                              "forged.on1x"));
  auto wire=meta::encode(wire_source);
  auto original=source("println(read_key(create(1)));");
  meta::PpxOptions attempt;
  attempt.programs.push_back({{"forged.on1x"},"forged","not-an-authorized-transformer"});
  rejected("PPX graph rewrite","ON1X-PPX-SCOPE",[&]{meta::rewrite(original,attempt);});
  rejected("PPX encoding","ON1X-PPX-SCOPE",[&]{(void)meta::encode(original);});
  rejected("PPX forged output","",[&]{(void)meta::decode_rewrite(wire,original);});
  if(meta::decode(wire).requires_module_boundary)
   throw std::runtime_error("wire deserialization manufactured module authority");

  // As if a trusted IR pass accidentally moved a projection to a different
  // function. Type/layout proofs alone must not authorize C11 emission.
  auto forged=source("println(create(1).hidden);",{},false);
  check(forged);
  seal(forged);
  rejected("forged checked projection","ON1X-INTERFACE-OPAQUE",[&]{recheck_checked_hir(forged);});
  rejected("forged lowering","ON1X-INTERFACE-OPAQUE",[&]{(void)emit_c(forged,"forged");});
  std::cout<<generated_c;
  return 0;
 }catch(const std::exception& e){
  std::cerr<<e.what()<<'\n';return 1;
 }
}

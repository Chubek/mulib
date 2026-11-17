#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <on1x/nominal.hpp>
#include <iostream>

namespace {
using namespace on1x;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
template<class F>void reject(F f){try{f();}catch(const Error&){return;}throw std::runtime_error("invalid nominal HIR/wire admitted");}
Unit source(const std::string& text){return parse(scan(text,"nominal-test"));}
Unit checked(const std::string& text){auto unit=source(text);check(unit);return unit;}
}
int main(){try{
 using namespace on1x;
 auto raw=source("record Box<T>{x:T} enum Option<T>{None,Some(T)} fn main(){let b=record<Box<i64>>{x:42};let v=variant<Option<_>>(Some,b);}");
 auto wire=meta::write_sexpr(meta::encode(raw));require(wire.find("NominalDeclaration")!=std::string::npos&&wire.find("NamedType")!=std::string::npos,"missing source wire constructors");
 auto copy=meta::decode(meta::read_sexpr(wire,"wire"));require(meta::write_sexpr(meta::encode(copy))==wire,"nominal source round-trip differs");
 check(copy);verify_nominals(copy);auto c=emit_c(copy,"test");check(copy);require(emit_c(copy,"test")==c,"nominal checking is not idempotent");
 auto u=source("record A{x:i64} record B{x:i64} enum Tag<T>{Empty} fn main(){}");NominalCatalog catalog(u);
 auto a=catalog.instantiate("A",{},{"a"}),b=catalog.instantiate("B",{},{"b"});
 require(a!=b&&a!=Type::data(Type::Record,{Type::I64},{"x"}),"nominal identity was erased");
 auto ti=catalog.instantiate("Tag",{Type::I64},{"i"}),tb=catalog.instantiate("Tag",{Type::Bool},{"b"});require(ti!=tb&&ti.elements()==tb.elements(),"phantom argument was erased");
  auto schema=Type::for_all(1,Type::function({catalog.instantiate("Tag",{Type::bound(0)},{"forall"})},Type::bound(0)));
  reject([&]{catalog.verify(catalog.instantiate("Tag",{Type::String},{"bad"}),{"bad"});});
 require(forall_instance(schema,Type::function({ti},Type::I64)),"nominal forall instantiation rejected");
 require(!forall_instance(schema,Type::function({ti},Type::Bool)),"nominal forall argument identity lost");
 auto bad=checked("record A{x:i64} fn main(){let a=record<A>{x:42};println(a[\"x\"]);}");
 auto wrong=Type::data(Type::Record,{Type::Bool},{"x"}).with_nominal("A",{});
  bad.functions[0].body[0].value->type=wrong;
  reject([&]{verify_nominals(bad);});reject([&]{emit_c(bad,"bad");});reject([&]{mir::lower(bad);});
  auto null_child=checked("record A{x:i64} fn main(){let a=record<A>{x:42};}");null_child.functions[0].body[0].value->children[0].reset();reject([&]{verify_nominals(null_child);});
 auto changed=checked("record A{x:i64} fn main(){let a=record<A>{x:42};}");changed.nominals[0].fields[0]=Type::Bool;reject([&]{check(changed);});
 auto vanished=checked("record A{x:i64} fn main(){let a=record<A>{x:42};}");vanished.nominals.clear();reject([&]{emit_c(vanished,"bad");});
 auto malformed=meta::encode(source("record A<T>{x:T} fn main(){}"));
 // Bound parameters are source-only indices, never inferred type identities.
 auto& declaration=malformed.items[2].items[0];declaration.items[5].items[0]=meta::list({meta::symbol("BoundType"),meta::symbol("1")});reject([&]{meta::decode(malformed);});
 auto invalid=source("enum A{X} fn main(){}");invalid.nominals[0].kind=Type::Tuple;reject([&]{check(invalid);});
 auto failed=source("record A{x:A} fn main(){}");auto before=meta::write_sexpr(meta::encode(failed));reject([&]{check(failed);});require(meta::write_sexpr(meta::encode(failed))==before,"failed inference partially published nominal layout");
 std::cout<<"nominal identity, phantom/forall arguments, source wire, layout mutation and idempotence PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

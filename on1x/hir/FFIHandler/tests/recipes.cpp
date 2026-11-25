#include <on1x/backend/mir.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace {
void expect(bool predicate,const char* message){
 if(!predicate)throw std::runtime_error(message);
}
}

int main(){
 using namespace on1x;
 const Location at{"<stdffi-test>"};
 expect(!stdffi::has_kernel("mulib.nonexistent"),"registry accepts a noncanonical kernel name");
 expect(!stdffi::materialize("mulib.nonexistent","absent",at),"unknown recipe materialized");
 std::set<std::string_view> functions;
 for(const auto& descriptor:stdffi::exports()){
  expect(stdffi::has_kernel(descriptor.kernel),"generated kernel not found in its registry");
  expect(functions.insert(descriptor.function).second,"duplicate generated private function");
  auto function=stdffi::materialize(descriptor.kernel,descriptor.member,at);
  expect(function&&function->name==descriptor.function&&!function->external&&function->kernel_symbol.empty(),
         "recipe did not produce a private definition or has native authority");
  expect(function->params.size()==descriptor.arity&&function->result==Type::I64&&
         std::all_of(function->params.begin(),function->params.end(),[](const auto& p){return p.type==Type::I64;}),
         "materialized signature differs from the generated signed-i64 profile");

  auto unit=parse(scan("fn main() -> i64 { return 0; }",at.file));
  unit.functions.push_back(std::move(*function));
  check(unit);
  const auto lowered=mir::lower(unit);
  const auto target=std::find_if(lowered.functions.begin(),lowered.functions.end(),[&](const auto& f){
   return f.name==descriptor.function;
  });
  expect(target!=lowered.functions.end(),"generated recipe did not reach verified MIR");
  expect(mir::structurally_equal(lowered,mir::deserialize(mir::serialize(lowered))),
         "generated recipe did not round-trip through verified MIR");
 }
}

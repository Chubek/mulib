#include <on1x/backend/mir.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#define MULIB_IMPL_CLAMP
#include <mulib-clamp.h>

namespace {
void expect(bool predicate,const char* message){
 if(!predicate)throw std::runtime_error(message);
}
}

int main(){
 using namespace on1x;
 const Location at{"<stdffi-test>"};
 expect(stdffi::has_kernel("clamp"),"clamp is not in the standard registry");
 expect(!stdffi::has_kernel("mulib.clamp"),"registry accepts a noncanonical kernel name");
 expect(!stdffi::materialize("clamp","absent",at),"unknown recipe materialized");
 auto unit=parse(scan("fn main() -> i64 { return 0; }",at.file));
 for(auto member:{"i64","add_i64","sub_i64"}){
  auto function=stdffi::materialize("clamp",member,at);
  expect(function&&!function->external&&function->kernel_symbol.empty(),"recipe has native authority");
  unit.functions.push_back(std::move(*function));
 }
 check(unit);
 const auto module=mir::lower(unit);
 expect(module.version==mir::directive_format_version,"stdffi metadata did not promote MIR version");
 expect(mir::structurally_equal(module,mir::deserialize(mir::serialize(module))),"directives did not round trip");
 for(const auto& descriptor:stdffi::exports()){
  const auto function=std::find_if(module.functions.begin(),module.functions.end(),
                                   [&](const auto& f){return f.name==descriptor.function;});
  expect(function!=module.functions.end(),"registry function was not lowered");
  expect(std::any_of(module.directives.begin(),module.directives.end(),[&](const auto& directive){
   return directive.function==function->id&&directive.backend=="mir-c11"&&directive.target.empty()&&
          directive.kind=="hint"&&directive.action=="private-code"&&!directive.speculative;
  }),"backend-scoped private-code selector was lost");
  expect(std::any_of(module.directives.begin(),module.directives.end(),[&](const auto& directive){
   return directive.function==function->id&&directive.backend.empty()&&directive.target=="x86_64-unknown-linux-gnu"&&
          directive.kind=="hint"&&directive.action=="kernel-hot"&&directive.speculative;
  }),"target-scoped kernel-hot selector was lost");
  expect(std::any_of(module.directives.begin(),module.directives.end(),[&](const auto& directive){
   return directive.function==function->id&&directive.backend.empty()&&directive.target.empty()&&
          directive.kind=="rewrite"&&directive.action=="fold-identity"&&directive.speculative;
  }),"speculative rewrite directive was lost");
 }
 auto run=[&](std::string_view name,const std::vector<std::int64_t>& args, std::int64_t value){
  auto r=mir::evaluate(module,name,args);
  expect(r.status==mir::Status::Returned&&r.value==value,"compiled recipe disagrees with reviewed C");
 };
 constexpr auto lo=std::numeric_limits<std::int64_t>::min();
 constexpr auto hi=std::numeric_limits<std::int64_t>::max();
 const std::array<std::int64_t,9> edge{lo,lo+1,-2,-1,0,1,2,hi-1,hi};
 for(auto a:edge)for(auto b:edge){
  run("@stdffi_clamp_add_i64",{a,b},mulib_clamp_add_i64(a,b));
  run("@stdffi_clamp_sub_i64",{a,b},mulib_clamp_sub_i64(a,b));
  for(auto c:edge){
   if(b<=c)run("@stdffi_clamp_i64",{a,b,c},mulib_clamp_i64(a,b,c));
   else expect(mir::evaluate(module,"@stdffi_clamp_i64",{a,b,c}).status==mir::Status::Trapped,
               "invalid clamp interval did not trap");
  }
 }
}

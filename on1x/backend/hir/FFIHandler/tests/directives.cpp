#include <on1x/backend/mir_c11.hpp>
#include <on1x/backend/mir_directives.hpp>
#include <on1x/loader.hpp>
#include <on1x/stdffi.hpp>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#define MULIB_IMPL_FFIFIXTURE
#include "mulib-ffifixture.h"

namespace {
void expect(bool predicate,const char* message){
 if(!predicate)throw std::runtime_error(message);
}
}

int main(int argc,char** argv){
 using namespace on1x;
 expect(argc==2,"missing stdffi import fixture");
 LoaderOptions options;
 options.prelude=false;
 auto unit=load_unit(argv[1],options).unit;
 check(unit);
 const auto lowered=mir::lower(unit);
 expect(lowered.version==mir::directive_format_version,"recipe directives did not promote MIR version");
 auto module=mir::deserialize(mir::serialize(lowered));
 expect(mir::structurally_equal(module,lowered),"FFI recipe directives did not round-trip");

 const auto function=std::find_if(module.functions.begin(),module.functions.end(),[](const auto& f){
   return f.name=="@stdffi_ffifixture_bump";
 });
 expect(function!=module.functions.end()&&module.functions.size()==2,"unreferenced FFI recipes were materialized");
 expect(module.directives.size()==9,"unexpected directives for the referenced FFI recipe");
 auto directive=[&](std::string_view kind,std::string_view action,std::string_view backend,
                    std::string_view target,bool speculative=false){
  expect(std::any_of(module.directives.begin(),module.directives.end(),[&](const auto& d){
   return d.kind==kind&&d.action==action&&d.backend==backend&&d.target==target&&
          d.speculative==speculative&&d.function==function->id;
  }),"FFI directive lost its selectors, speculation flag or function scope");
 };
 constexpr std::string_view target="x86_64-unknown-linux-gnu";
 directive("hint","kernel-hot","mir-c11",target);
 directive("hint","cold-code","mir-c11",target,true);
 directive("hint","no-inline","mir-c11",target);
 directive("hint","private-code","mir-c11","");
 directive("hint","canonicalized","",target);
 directive("hint","local-memory","mir-vm","");
 directive("rewrite","fold-identity","","",true);
 directive("rewrite",mir::memgraph_local_action,"mir-c11",target);
 directive("rewrite",mir::meta_canonical_action,"mir-vm","",true);

 const auto specialized=mir::specialize(module,"mir-vm",target,true);
 const auto result=mir::evaluate(specialized);
 expect(result.status==mir::Status::Returned&&result.value==3,"specialization changed recipe result");
 constexpr auto lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();
 const std::array<std::int64_t,5> edge{lo,-1,0,hi-1,hi};
 for(auto x:edge){
  auto value=mir::evaluate(specialized,function->name,{x});
  expect(value.status==mir::Status::Returned&&value.value==mulib_ffifixture_bump(x),
         "scoped MIR rewrites changed the admitted C result");
 }
 const auto plain=mir::emit_directed_c(module,std::nullopt,"aarch64-unknown-linux-gnu",true);
 const auto hot=mir::emit_directed_c(module,std::nullopt,target,false);
 const auto cold=mir::emit_directed_c(module,std::nullopt,target,true);
 const std::string prototype="ON1X_MIR_PRIVATE int64_t on1x_mir_f"+std::to_string(function->id)+"(";
 expect(plain.find("\n"+prototype)!=std::string::npos,"target mismatch enabled a codegen hint");
 const auto signature="ON1X_MIR_NOINLINE "+prototype;
 expect(hot.find("ON1X_MIR_HOT "+signature)!=std::string::npos&&
        hot.find("ON1X_MIR_COLD")==std::string::npos,"non-speculative hint selection failed");
 expect(cold.find("ON1X_MIR_COLD "+signature)!=std::string::npos&&
        cold.find("ON1X_MIR_HOT "+signature)==std::string::npos,"speculative hint selection failed");
}

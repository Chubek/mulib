#include <on1x/meta.hpp>
#include <cassert>

using namespace on1x::meta;
template<class F> void rejects(F f){try{f();}catch(const on1x::Error&){return;}throw std::runtime_error("unexpected acceptance");}
int main(){
 auto schema=base_schema();
 auto packet=list({symbol("SourceModuleGraphMetadata"),symbol("1"),string("{\"test\":\"μ\"}"),string("{}")});
 auto copy=read_sexpr(write_sexpr(packet),"fixture");
 schema.validate(copy,"SourceModuleGraphMetadata");
 assert(copy.items[2].text==packet.items[2].text);
 rejects([&]{schema.validate(copy,"Unit");});
 copy.items[1]=symbol("2");rejects([&]{schema.validate(copy,"SourceModuleGraphMetadata");});
 rejects([&]{schema.extend(R"({"version":1,"nodes":{"evil:X":{"category":"SourceModuleGraphMetadata","fields":[]}}})","evil","fixture");});
 rejects([&]{schema.extend(R"({"version":1,"nodes":{"evil:X":{"category":"Expression","fields":[["x","SourceModuleGraphMetadata"]]}}})","evil","fixture");});
}

#include "proxy_policy.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
extern char **environ;
#endif

namespace on1x::proxy {
#if defined(__unix__) || defined(__APPLE__)
namespace {
std::string lowercase(std::string_view text){
 std::string out(text);
 for(char& c:out)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
 return out;
}
constexpr std::array keys={
 std::pair{"http_proxy","HTTP_PROXY"},
 std::pair{"https_proxy","HTTPS_PROXY"},
 std::pair{"any_proxy","ANY_PROXY"},
 std::pair{"no_proxy","NO_PROXY"},
 std::pair{"all_proxy","ALL_PROXY"},
};
enum Slot { http,https,any,no,all };
struct Setting {int rank=3;std::string key,value;};
}
#endif

void apply(const Policy& policy){
#if defined(__unix__) || defined(__APPLE__)
 std::array<Setting,keys.size()> ambient;
 std::vector<std::string> names;
 for(char** row=environ;row&&*row;++row){
  std::string_view entry=*row;
  const auto equal=entry.find('=');
  if(equal==std::string_view::npos)continue;
  const auto key=entry.substr(0,equal);
  const auto folded=lowercase(key);
  for(std::size_t i=0;i<keys.size();++i){
   const auto [canonical,uppercase]=keys[i];
   if(folded!=canonical)continue;
   names.emplace_back(key);
   const int rank=key==canonical?0:key==uppercase?1:2;
   auto& found=ambient[i];
   if(rank<found.rank||(rank==found.rank&&key<found.key))
    found={rank,std::string(key),std::string(entry.substr(equal+1))};
   break;
  }
 }
 auto fail=[](){throw std::runtime_error("cannot configure host proxy environment");};
 for(const auto& name:names)if(::unsetenv(name.c_str())!=0)fail();
 auto set=[&](Slot i,const std::string& value){
  if(!value.empty()&&(::setenv(keys[i].first,value.c_str(),1)!=0||
     ::setenv(keys[i].second,value.c_str(),1)!=0))fail();
 };
 if(policy.none){
  set(no,"*");
  return;
 }
 const auto& generic=policy.any.empty()?ambient[any].value:policy.any;
 auto route=[&](const std::string& project,Slot scheme)->const std::string&{
  if(!project.empty())return project;
  if(!policy.any.empty())return policy.any;
  if(!ambient[scheme].value.empty())return ambient[scheme].value;
  return generic;
 };
 set(http,route(policy.http,http));
 set(https,route(policy.https,https));
 set(any,generic);
 set(no,ambient[no].value);
 // Transport helpers understand ALL_PROXY, but only ANY_PROXY configures it.
 set(all,generic);
#else
 (void)policy;
#endif
}
}

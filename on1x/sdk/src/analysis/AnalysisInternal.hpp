#ifndef ON1X_SDK_ANALYSIS_INTERNAL_HPP
#define ON1X_SDK_ANALYSIS_INTERNAL_HPP

#include "On1xSDK-AnalysisManager.h"
#include "On1xSDK-PassManager.h"
#include <on1x/analysis.hpp>
#include <on1x/backend/passes.hpp>

#include "../ProgramInternal.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace on1x::sdk::analysis_detail {

inline constexpr std::uint64_t manager_magic=UINT64_C(0x4f31414e4d475232);
inline constexpr std::uint64_t analysis_result_magic=UINT64_C(0x4f31414e52534c54);
inline constexpr std::uint64_t pass_result_magic=UINT64_C(0x4f31504153535232);

struct BaseKey {
 std::uint64_t owner=0,high=0,low=0,options=0;
 std::string target;
 friend bool operator<(const BaseKey& a,const BaseKey& b){
  return std::tie(a.owner,a.high,a.low,a.target,a.options)<
         std::tie(b.owner,b.high,b.low,b.target,b.options);
 }
 friend bool operator==(const BaseKey&,const BaseKey&)=default;
};
struct FullKey {
 BaseKey base;
 std::uint64_t revision=0;
 friend bool operator<(const FullKey& a,const FullKey& b){
  return std::tie(a.base,a.revision)<std::tie(b.base,b.revision);
 }
 friend bool operator==(const FullKey&,const FullKey&)=default;
};
struct Observed {
 FullKey key;
 std::string wire;
 mir::Module module;
};
struct CacheKey {
 FullKey source;
 std::uint32_t kind=0,version=0,backend=0;
 friend bool operator<(const CacheKey& a,const CacheKey& b){
  return std::tie(a.source,a.kind,a.version,a.backend)<
         std::tie(b.source,b.kind,b.version,b.backend);
 }
 friend bool operator==(const CacheKey&,const CacheKey&)=default;
};

struct AnalysisData {
 CacheKey key;
 mir::analysis::Result value;
 bool cache_hit=false;
};

struct PassData {
 FullKey source;
 mir::Module before,after;
 std::string before_wire,after_wire;
 mir::passes::Result result;

 PassData(FullKey key,mir::Module input,std::string wire,mir::passes::Result output)
  :source(std::move(key)),before(std::move(input)),after(output.module()),
   before_wire(std::move(wire)),after_wire(output.output()),result(std::move(output)){}
};

struct ManagerImpl;

} // namespace on1x::sdk::analysis_detail

struct On1xSDK_AnalysisResult {
 std::uint64_t magic=on1x::sdk::analysis_detail::analysis_result_magic;
 std::uint64_t generation=0;
 on1x::sdk::analysis_detail::AnalysisData data;
};

struct On1xSDK_PassResult {
 std::uint64_t magic=on1x::sdk::analysis_detail::pass_result_magic;
 std::uint64_t generation=0;
 on1x::sdk::analysis_detail::PassData data;

 template<class... Args>
 On1xSDK_PassResult(std::uint64_t current,Args&&... args)
  :generation(current),data(std::forward<Args>(args)...){}
};

namespace on1x::sdk::analysis_detail {

struct ManagerImpl {
 std::uint64_t owner=0,generation=1,hits=0,misses=0;
 std::size_t max_sources=0,max_cache=0;
 bool busy=false;
 std::map<BaseKey,Observed> sources;
 std::deque<std::unique_ptr<On1xSDK_AnalysisResult>> cache;
 std::unique_ptr<On1xSDK_PassResult> pass;
};

struct ManagerOperation {
 ManagerImpl& manager;
 explicit ManagerOperation(ManagerImpl& value) noexcept:manager(value){manager.busy=true;}
 ~ManagerOperation(){manager.busy=false;}
 ManagerOperation(const ManagerOperation&)=delete;
 ManagerOperation& operator=(const ManagerOperation&)=delete;
};

} // namespace on1x::sdk::analysis_detail

struct On1xSDK_AnalysisManager {
 std::uint64_t magic=0;
 std::uint32_t active=0;
 std::uint32_t reserved=0;
 on1x::sdk::analysis_detail::ManagerImpl *impl=nullptr;
};

namespace on1x::sdk::analysis_detail {

On1xSDK_Status parse_key(const On1xSDK_AnalysisManager&,const On1xSDK_AnalysisKey&,
                         FullKey&) noexcept;
On1xSDK_Status current_source(const On1xSDK_AnalysisManager&,const FullKey&,
                              const Observed*&) noexcept;
bool valid_manager(const On1xSDK_AnalysisManager*) noexcept;
bool valid_result(const On1xSDK_AnalysisManager*,const On1xSDK_AnalysisResult*) noexcept;
bool valid_pass_result(const On1xSDK_AnalysisManager*,const On1xSDK_PassResult*) noexcept;
void mutate(ManagerImpl&) noexcept;
On1xSDK_Status check_v2_header(const void*,std::size_t) noexcept;
On1xSDK_Status check_output_header(const void*,std::size_t) noexcept;
On1xSDK_AnalysisKey public_key(const FullKey&) noexcept;
std::uint32_t callback_control(On1xSDK_AnalysisControlFn,void*,std::uint64_t) noexcept;

#ifdef ON1X_SDK_WITH_NOVOSTONE
mir::analysis::Result run_novostone(const mir::Module&,
                                    mir::analysis::Options);
#endif

} // namespace on1x::sdk::analysis_detail

#endif

#include "AnalysisInternal.hpp"

#include "On1xSDK-CFG.h"
#include "On1xSDK-Dominator.h"
#include "On1xSDK-EffectAnalysis.h"
#include "On1xSDK-Liveness.h"
#include "On1xSDK-Loop.h"

#include "../FacetInternal.hpp"

#ifdef ON1X_SDK_WITH_NOVOSTONE
#include <on1x/backend/relational.hpp>
#endif

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>

namespace on1x::sdk::analysis_detail {
namespace {

bool target_text(const On1xSDK_AnalysisStringView& input,std::string& output){
 if(!input.size||input.size>ON1X_SDK_ANALYSIS_TARGET_MAX||!input.data)return false;
 output.assign(input.data,input.size);
 return std::all_of(output.begin(),output.end(),[](unsigned char c){
  return c>=0x21&&c<=0x7e;
 });
}

bool same_identity(const BaseKey& left,const FullKey& right){
 return left.owner==right.base.owner&&left.high==right.base.high&&left.low==right.base.low;
}

} // namespace

On1xSDK_Status check_v2_header(const void* input,std::size_t required) noexcept {
 if(!input)return ON1X_SDK_INVALID_ARGUMENT;
 std::uint32_t version=0,size=0;
 std::memcpy(&version,input,sizeof version);
 std::memcpy(&size,static_cast<const unsigned char*>(input)+sizeof version,sizeof size);
 if(version!=ON1X_SDK_ANALYSIS_ABI_VERSION||size<required)return ON1X_SDK_ABI_MISMATCH;
 return ON1X_SDK_OK;
}

On1xSDK_Status check_output_header(const void* output,std::size_t required) noexcept {
 return check_v2_header(output,required);
}

bool valid_manager(const On1xSDK_AnalysisManager* manager) noexcept {
 return manager&&manager->magic==manager_magic&&manager->active==1&&manager->impl;
}

void mutate(ManagerImpl& impl) noexcept {
 ++impl.generation;
 if(!impl.generation)++impl.generation;
}

On1xSDK_Status parse_key(const On1xSDK_AnalysisManager& manager,
                         const On1xSDK_AnalysisKey& input,FullKey& output) noexcept {
 if(!valid_manager(&manager)||input.owner_token!=manager.impl->owner||
    (!input.identity.high&&!input.identity.low)||!input.revision)
  return ON1X_SDK_INVALID_ARGUMENT;
 return guard([&]{
  BaseKey base;std::string target;
  if(!target_text(input.target,target))return ON1X_SDK_INVALID_ARGUMENT;
  base.owner=input.owner_token;base.high=input.identity.high;base.low=input.identity.low;
  base.options=input.options_key;base.target=std::move(target);
  output={std::move(base),input.revision};return ON1X_SDK_OK;
 });
}

On1xSDK_Status current_source(const On1xSDK_AnalysisManager& manager,
                              const FullKey& key,const Observed*& output) noexcept {
 output=nullptr;
 const auto found=manager.impl->sources.find(key.base);
 if(found==manager.impl->sources.end()){
  for(const auto& [base,source]:manager.impl->sources){
   (void)source;
   if(same_identity(base,key))return ON1X_SDK_UNSUPPORTED;
  }
  return ON1X_SDK_ANALYSIS_NOT_FOUND;
 }
 if(found->second.key.revision!=key.revision)return ON1X_SDK_ANALYSIS_STALE_REVISION;
 output=&found->second;return ON1X_SDK_OK;
}

bool valid_result(const On1xSDK_AnalysisManager* manager,
                  const On1xSDK_AnalysisResult* result) noexcept {
 if(!valid_manager(manager)||!result)return false;
 return std::any_of(manager->impl->cache.begin(),manager->impl->cache.end(),
                    [&](const auto& entry){return entry.get()==result;})&&
        result->magic==analysis_result_magic&&result->generation==manager->impl->generation;
}

bool valid_pass_result(const On1xSDK_AnalysisManager* manager,
                       const On1xSDK_PassResult* result) noexcept {
 return valid_manager(manager)&&result&&manager->impl->pass.get()==result&&
        result->magic==pass_result_magic&&result->generation==manager->impl->generation;
}

On1xSDK_AnalysisKey public_key(const FullKey& key) noexcept {
 return {key.base.owner,{key.base.high,key.base.low},key.revision,
         {key.base.target.empty()?nullptr:key.base.target.data(),key.base.target.size()},
         key.base.options};
}

std::uint32_t callback_control(On1xSDK_AnalysisControlFn callback,void* userdata,
                               std::uint64_t work) noexcept {
 if(!callback)return ON1X_SDK_OK;
 try{return callback(userdata,work);}catch(...){return ON1X_SDK_ERROR;}
}

} // namespace on1x::sdk::analysis_detail

namespace {
using namespace on1x::sdk;
using namespace on1x::sdk::analysis_detail;

On1xSDK_Status load_module(const On1xSDK_Program* program,on1x::mir::Module& module,
                           std::string& wire) noexcept {
 if(!program)return ON1X_SDK_INVALID_ARGUMENT;
 On1xSDK_Phase phase{};
 if(on1x_sdk_program_phase(program,&phase)!=ON1X_SDK_OK||phase!=ON1X_SDK_VERIFIED_MIR)
  return ON1X_SDK_UNSUPPORTED;
 try{
  if(const auto* builtin=on1x::sdk::builtin_mir(program)){
   on1x::mir::verify(*builtin);module=*builtin;wire=on1x::mir::serialize(module);
   if(wire.size()>ON1X_SDK_MAX_PROGRAM_BYTES)return ON1X_SDK_LIMIT;
   return ON1X_SDK_OK;
  }
  const std::uint8_t* bytes=nullptr;std::size_t size=0;
  if(const auto status=on1x_sdk_program_bytes(program,&bytes,&size);status!=ON1X_SDK_OK)
   return status;
  if(size>ON1X_SDK_MAX_PROGRAM_BYTES)return ON1X_SDK_LIMIT;
  const auto* data=size?reinterpret_cast<const char*>(bytes):"";
  const std::string_view text(data,size);
  module=on1x::mir::deserialize(text,"<sdk-verified-mir>");
  wire=on1x::mir::serialize(module);
  if(wire.size()!=size||(size&&std::memcmp(wire.data(),bytes,size)!=0))
   return ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;
  return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
 catch(const on1x::Error&){return ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;}
 catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status map_analysis_status(on1x::mir::analysis::Status status) noexcept {
 using Status=on1x::mir::analysis::Status;
 switch(status){
 case Status::Complete:return ON1X_SDK_OK;
 case Status::BudgetExhausted:return ON1X_SDK_LIMIT;
 case Status::Cancelled:return ON1X_SDK_ANALYSIS_CANCELLED;
 case Status::DeadlineExceeded:return ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED;
 case Status::CallbackFailed:return ON1X_SDK_ERROR;
 }
 return ON1X_SDK_ERROR;
}

std::optional<on1x::mir::analysis::Kind> canonical_kind(std::uint32_t kind){
 using Kind=on1x::mir::analysis::Kind;
 switch(kind){
 case ON1X_SDK_ANALYSIS_CFG:return Kind::CFG;
 case ON1X_SDK_ANALYSIS_DOMINATORS:return Kind::Dominators;
 case ON1X_SDK_ANALYSIS_LOOPS:return Kind::Loops;
 case ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF:return Kind::LivenessUseDef;
 case ON1X_SDK_ANALYSIS_EFFECTS:return Kind::Effects;
 default:return {};
 }
}

template<class Input>
std::size_t count_records(const On1xSDK_AnalysisManager* manager,
                          const On1xSDK_AnalysisResult* result,
                          std::uint32_t kind,
                          const std::vector<Input> on1x::mir::analysis::Result::*member) noexcept {
 if(!valid_result(manager,result)||result->data.key.kind!=kind)return 0;
 return (result->data.value.*member).size();
}

template<class Output,class Input,class Convert>
On1xSDK_Status record_at(const On1xSDK_AnalysisManager* manager,
                         const On1xSDK_AnalysisResult* result,std::uint32_t kind,
                          const std::vector<Input> on1x::mir::analysis::Result::*member,
                          std::size_t index,
                          Output* output,Convert convert) noexcept {
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 if(result->data.key.kind!=kind)return ON1X_SDK_INVALID_ARGUMENT;
 const auto& values=result->data.value.*member;
 if(index>=values.size())return ON1X_SDK_ANALYSIS_NOT_FOUND;
 if(const auto status=check_output_header(output,sizeof(Output));status!=ON1X_SDK_OK)return status;
 Output value{};value.abi_version=output->abi_version;value.struct_size=output->struct_size;
 convert(values[index],value);
 std::memcpy(output,&value,sizeof value);return ON1X_SDK_OK;
}

} // namespace

extern "C" {

size_t on1x_sdk_analysis_manager_storage_size(void){
 return sizeof(On1xSDK_AnalysisManager);
}

size_t on1x_sdk_analysis_manager_storage_alignment(void){
 return alignof(On1xSDK_AnalysisManager);
}

On1xSDK_Status on1x_sdk_analysis_manager_initialize(
    void* storage,size_t storage_size,const On1xSDK_AnalysisManagerConfig* config,
    On1xSDK_AnalysisManager** out_manager){
 using namespace on1x::sdk;
 using namespace on1x::sdk::analysis_detail;
 if(!out_manager)return ON1X_SDK_INVALID_ARGUMENT;
 *out_manager=nullptr;
 if(!storage||storage_size<sizeof(On1xSDK_AnalysisManager)||
    reinterpret_cast<std::uintptr_t>(storage)%alignof(On1xSDK_AnalysisManager))
  return ON1X_SDK_INVALID_ARGUMENT;
 if(const auto status=check_v2_header(config,sizeof *config);status!=ON1X_SDK_OK)return status;
 if(!config->owner_token||!config->max_sources||
    config->max_sources>ON1X_SDK_ANALYSIS_MAX_SOURCES||!config->max_cache_entries||
    config->max_cache_entries>ON1X_SDK_ANALYSIS_MAX_CACHE_ENTRIES)
  return ON1X_SDK_INVALID_ARGUMENT;
 return guard([&]{
  auto impl=std::make_unique<ManagerImpl>();
  impl->owner=config->owner_token;impl->max_sources=config->max_sources;
  impl->max_cache=config->max_cache_entries;
  auto* manager=new(storage) On1xSDK_AnalysisManager{};
  manager->impl=impl.release();manager->active=1;manager->magic=manager_magic;
  *out_manager=manager;return ON1X_SDK_OK;
 });
}

On1xSDK_Status on1x_sdk_analysis_manager_dispose(On1xSDK_AnalysisManager* manager){
 using namespace on1x::sdk::analysis_detail;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(manager->impl->busy)return ON1X_SDK_BUSY;
 auto* impl=manager->impl;manager->active=0;manager->impl=nullptr;manager->magic=0;
 delete impl;return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_analysis_manager_observe(
    On1xSDK_AnalysisManager* manager,const On1xSDK_ObservedMIR* source){
 using namespace on1x::sdk;
 using namespace on1x::sdk::analysis_detail;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(manager->impl->busy)return ON1X_SDK_BUSY;
 if(const auto status=check_v2_header(source,sizeof *source);status!=ON1X_SDK_OK)return status;
 FullKey key;
 if(const auto status=parse_key(*manager,source->key,key);status!=ON1X_SDK_OK)return status;
 on1x::mir::Module module;std::string wire;
 auto status=load_module(source->program,module,wire);
 if(status!=ON1X_SDK_OK)return status;
 return guard([&]{
  auto existing=manager->impl->sources.find(key.base);
  if(existing!=manager->impl->sources.end()){
   if(key.revision<existing->second.key.revision)return ON1X_SDK_ANALYSIS_STALE_REVISION;
   if(key.revision==existing->second.key.revision)
    return wire==existing->second.wire?ON1X_SDK_OK:ON1X_SDK_ANALYSIS_REVISION_CONFLICT;
  }else if(manager->impl->sources.size()>=manager->impl->max_sources)return ON1X_SDK_LIMIT;
  Observed candidate{key,std::move(wire),std::move(module)};
  if(existing==manager->impl->sources.end())manager->impl->sources.emplace(key.base,std::move(candidate));
  else existing->second=std::move(candidate);
  std::erase_if(manager->impl->cache,[&](const auto& entry){
   return entry->data.key.source.base==key.base;
  });
  manager->impl->pass.reset();mutate(*manager->impl);
  return ON1X_SDK_OK;
 });
}

On1xSDK_Status on1x_sdk_analysis_manager_query(
    On1xSDK_AnalysisManager* manager,const On1xSDK_Query* query,
    const On1xSDK_AnalysisResult** out_result){
 using namespace on1x::sdk;
 using namespace on1x::sdk::analysis_detail;
 if(!out_result)return ON1X_SDK_INVALID_ARGUMENT;
 *out_result=nullptr;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(manager->impl->busy)return ON1X_SDK_BUSY;
 ManagerOperation operation(*manager->impl);
 if(const auto status=check_v2_header(query,sizeof *query);status!=ON1X_SDK_OK)return status;
 if(const auto status=check_v2_header(&query->control,sizeof query->control);status!=ON1X_SDK_OK)return status;
 if(query->reserved)return ON1X_SDK_INVALID_ARGUMENT;
 if(query->analysis_version!=ON1X_SDK_ANALYSIS_VERSION)return ON1X_SDK_UNSUPPORTED;
 if(query->backend!=ON1X_SDK_ANALYSIS_BACKEND_CANONICAL
#ifdef ON1X_SDK_WITH_NOVOSTONE
    &&query->backend!=ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE
#endif
   )return ON1X_SDK_UNSUPPORTED;
 auto kind=canonical_kind(query->kind);if(!kind)return ON1X_SDK_UNSUPPORTED;
 if(query->control.work_limit>ON1X_SDK_ANALYSIS_MAX_WORK||
    query->control.result_limit>ON1X_SDK_ANALYSIS_MAX_RESULTS)return ON1X_SDK_LIMIT;
 FullKey source_key;
 if(const auto status=parse_key(*manager,query->source,source_key);status!=ON1X_SDK_OK)return status;
 const Observed* source=nullptr;
 if(const auto status=current_source(*manager,source_key,source);status!=ON1X_SDK_OK)return status;
#ifdef ON1X_SDK_WITH_NOVOSTONE
 if(query->backend==ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE&&
    (source->module.version!=on1x::mir::legacy_format_version||
     (*kind!=on1x::mir::analysis::Kind::CFG&&
      *kind!=on1x::mir::analysis::Kind::LivenessUseDef&&
      *kind!=on1x::mir::analysis::Kind::Effects)))
  return ON1X_SDK_UNSUPPORTED;
#endif
 const auto initial=callback_control(query->control.poll,query->control.userdata,0);
 if(initial!=ON1X_SDK_OK)
  return initial==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_CANCELLED)?ON1X_SDK_ANALYSIS_CANCELLED:
         initial==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED)?ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED:
         ON1X_SDK_ERROR;
 try{
  CacheKey cache_key{source_key,query->kind,query->analysis_version,query->backend};
  for(auto& entry:manager->impl->cache)if(entry->data.key==cache_key){
   if(entry->data.value.work>query->control.work_limit||
      entry->data.value.record_count()>query->control.result_limit)return ON1X_SDK_LIMIT;
   entry->data.cache_hit=true;entry->generation=manager->impl->generation;
   ++manager->impl->hits;*out_result=entry.get();return ON1X_SDK_OK;
  }
  ++manager->impl->misses;
  on1x::mir::analysis::Options options;
  options.kind=*kind;options.work=static_cast<std::size_t>(query->control.work_limit);
  options.records=static_cast<std::size_t>(query->control.result_limit);
  options.control=[control=query->control](std::size_t work){
   const auto value=callback_control(control.poll,control.userdata,work);
   if(value==ON1X_SDK_OK)return on1x::mir::analysis::Control::Continue;
   if(value==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_CANCELLED))
    return on1x::mir::analysis::Control::Cancel;
   if(value==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED))
    return on1x::mir::analysis::Control::Deadline;
   return on1x::mir::analysis::Control::Failure;
  };
  on1x::mir::analysis::Result value;
#ifdef ON1X_SDK_WITH_NOVOSTONE
  if(query->backend==ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE)
   value=run_novostone(source->module,std::move(options));
  else
#endif
   value=on1x::mir::analysis::run(source->module,std::move(options));
  if(const auto status=map_analysis_status(value.status);status!=ON1X_SDK_OK)return status;
  auto result=std::make_unique<On1xSDK_AnalysisResult>();
  result->data={cache_key,std::move(value),false};
  if(manager->impl->cache.size()==manager->impl->max_cache)manager->impl->cache.pop_front();
  mutate(*manager->impl);result->generation=manager->impl->generation;
  auto* view=result.get();manager->impl->cache.push_back(std::move(result));
  *out_result=view;return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
#ifdef ON1X_SDK_WITH_NOVOSTONE
 catch(const on1x::mir::relational::BudgetExceeded&){return ON1X_SDK_LIMIT;}
#endif
 catch(const on1x::Error&){return ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;}
 catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status on1x_sdk_analysis_result_info(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_AnalysisResult* result,
    On1xSDK_AnalysisResultInfo* out_info){
 using namespace on1x::sdk::analysis_detail;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 if(const auto status=check_output_header(out_info,sizeof *out_info);status!=ON1X_SDK_OK)return status;
 On1xSDK_AnalysisResultInfo value{};
 value.abi_version=out_info->abi_version;value.struct_size=out_info->struct_size;
 value.source=public_key(result->data.key.source);
 value.kind=result->data.key.kind;value.analysis_version=result->data.key.version;
 value.deterministic_work=result->data.value.work;
 value.record_count=result->data.value.record_count();
 value.cache_generation=manager->impl->generation;value.cache_hit=result->data.cache_hit;
 value.backend=result->data.key.backend;
 std::memcpy(out_info,&value,sizeof value);return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_analysis_manager_stats(
    const On1xSDK_AnalysisManager* manager,On1xSDK_AnalysisManagerStats* out_stats){
 using namespace on1x::sdk::analysis_detail;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(const auto status=check_output_header(out_stats,sizeof *out_stats);status!=ON1X_SDK_OK)return status;
 On1xSDK_AnalysisManagerStats value{};
 value.abi_version=out_stats->abi_version;value.struct_size=out_stats->struct_size;
 value.generation=manager->impl->generation;
 value.cache_hits=manager->impl->hits;value.cache_misses=manager->impl->misses;
 value.observed_sources=static_cast<std::uint32_t>(manager->impl->sources.size());
 value.cache_entries=static_cast<std::uint32_t>(manager->impl->cache.size());
 std::memcpy(out_stats,&value,sizeof value);return ON1X_SDK_OK;
}

size_t on1x_sdk_cfg_block_count(const On1xSDK_AnalysisManager* manager,
                                const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_CFG,
                      &on1x::mir::analysis::Result::blocks);
}
size_t on1x_sdk_cfg_edge_count(const On1xSDK_AnalysisManager* manager,
                               const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_CFG,
                      &on1x::mir::analysis::Result::edges);
}
On1xSDK_Status on1x_sdk_cfg_block_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_CFGBlock* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_CFG,
   &on1x::mir::analysis::Result::blocks,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.block_id=v.block;
   out.predecessor_count=v.predecessors;out.successor_count=v.successors;
   out.is_entry=v.entry;
  });
}
On1xSDK_Status on1x_sdk_cfg_edge_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_CFGEdge* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_CFG,
   &on1x::mir::analysis::Result::edges,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.source_block_id=v.source;
   out.target_block_id=v.target;out.terminator_id=v.terminator;
   out.edge_ordinal=v.ordinal;
  });
}

size_t on1x_sdk_dominator_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_DOMINATORS,
                      &on1x::mir::analysis::Result::dominators);
}
On1xSDK_Status on1x_sdk_dominator_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_Dominator* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_DOMINATORS,
   &on1x::mir::analysis::Result::dominators,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.block_id=v.block;
   out.immediate_dominator_id=v.immediate;out.depth=v.depth;
  });
}

size_t on1x_sdk_loop_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_LOOPS,
                      &on1x::mir::analysis::Result::loops);
}
size_t on1x_sdk_loop_member_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_LOOPS,
                      &on1x::mir::analysis::Result::loop_members);
}
On1xSDK_Status on1x_sdk_loop_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_Loop* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_LOOPS,
   &on1x::mir::analysis::Result::loops,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.loop_ordinal=v.ordinal;
   out.header_block_id=v.header;out.latch_block_id=v.latch;
   out.parent_loop_ordinal=v.parent==SIZE_MAX?UINT64_MAX:v.parent;
   out.is_reducible=v.reducible;
  });
}
On1xSDK_Status on1x_sdk_loop_member_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_LoopMember* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_LOOPS,
   &on1x::mir::analysis::Result::loop_members,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.loop_ordinal=v.loop;out.block_id=v.block;
  });
}

size_t on1x_sdk_definition_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
                      &on1x::mir::analysis::Result::definitions);
}
size_t on1x_sdk_use_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
                      &on1x::mir::analysis::Result::uses);
}
size_t on1x_sdk_live_value_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
                      &on1x::mir::analysis::Result::liveness);
}
On1xSDK_Status on1x_sdk_definition_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_Definition* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
   &on1x::mir::analysis::Result::definitions,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.block_id=v.block;out.value_id=v.value;
   out.owner_id=v.owner;out.kind=
    v.kind==on1x::mir::analysis::DefinitionKind::BlockParameter?
     ON1X_SDK_DEFINITION_BLOCK_PARAMETER:ON1X_SDK_DEFINITION_INSTRUCTION;
  });
}
On1xSDK_Status on1x_sdk_use_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_Use* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
   &on1x::mir::analysis::Result::uses,index,output,[](const auto& v,auto& out){
   auto kind=v.kind==on1x::mir::analysis::UseKind::InstructionOperand?ON1X_SDK_USE_INSTRUCTION_OPERAND:
    v.kind==on1x::mir::analysis::UseKind::TerminatorValue?ON1X_SDK_USE_TERMINATOR_VALUE:
    ON1X_SDK_USE_EDGE_ARGUMENT;
   out.function_id=v.function;out.block_id=v.block;out.value_id=v.value;
   out.user_id=v.user;out.incoming_parameter_id=v.incoming_parameter;
   out.edge_ordinal=v.edge==SIZE_MAX?UINT64_MAX:v.edge;
   out.operand_ordinal=v.index;out.kind=kind;
  });
}
On1xSDK_Status on1x_sdk_live_value_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_LiveValue* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
   &on1x::mir::analysis::Result::liveness,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.block_id=v.block;out.value_id=v.value;
   out.is_live_in=v.live_in;
  });
}

size_t on1x_sdk_effect_step_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_EFFECTS,
                      &on1x::mir::analysis::Result::effects);
}
size_t on1x_sdk_function_effect_count(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result){
 return count_records(manager,result,ON1X_SDK_ANALYSIS_EFFECTS,
                      &on1x::mir::analysis::Result::function_effects);
}
On1xSDK_Status on1x_sdk_effect_step_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_EffectStep* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_EFFECTS,
   &on1x::mir::analysis::Result::effects,index,output,[](const auto& v,auto& out){
   out.function_id=v.function;out.block_id=v.block;
   out.instruction_id=v.instruction;out.effect=static_cast<std::uint32_t>(v.effect);
  });
}
On1xSDK_Status on1x_sdk_function_effect_at(const On1xSDK_AnalysisManager* manager,
 const On1xSDK_AnalysisResult* result,size_t index,On1xSDK_FunctionEffects* output){
 return record_at(manager,result,ON1X_SDK_ANALYSIS_EFFECTS,
   &on1x::mir::analysis::Result::function_effects,index,output,
   [](const auto& v,auto& out){
   out.function_id=v.function;out.transitive_effect_mask=v.mask;
  });
}

} // extern "C"

#include "../analysis/AnalysisInternal.hpp"

#include "On1xSDK-TranslationValidation.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <string_view>
#include <tuple>

namespace {
using namespace on1x::sdk;
using namespace on1x::sdk::analysis_detail;
namespace mp=on1x::mir::passes;

std::optional<mp::Pass> canonical_pass(std::uint32_t pass){
 switch(pass){
 case ON1X_SDK_PASS_CONSTANT_FOLD:return mp::Pass::ConstantFold;
 case ON1X_SDK_PASS_SIMPLIFY_CFG:return mp::Pass::SimplifyCFG;
 case ON1X_SDK_PASS_DEAD_PURE:return mp::Pass::DeadPure;
 case ON1X_SDK_PASS_LOCAL_SLOTS:return mp::Pass::LocalSlots;
 default:return {};
 }
}

std::uint32_t public_pass(mp::Pass pass){
 switch(pass){
 case mp::Pass::ConstantFold:return ON1X_SDK_PASS_CONSTANT_FOLD;
 case mp::Pass::SimplifyCFG:return ON1X_SDK_PASS_SIMPLIFY_CFG;
 case mp::Pass::DeadPure:return ON1X_SDK_PASS_DEAD_PURE;
 case mp::Pass::LocalSlots:return ON1X_SDK_PASS_LOCAL_SLOTS;
 }
 return UINT32_MAX;
}

std::uint32_t analysis_bit(mp::Analysis analysis){
 switch(analysis){
 case mp::Analysis::Signatures:return ON1X_SDK_PRESERVE_SIGNATURES;
 case mp::Analysis::CFG:return ON1X_SDK_PRESERVE_CFG;
 case mp::Analysis::Calls:return ON1X_SDK_PRESERVE_CALLS;
 case mp::Analysis::Effects:return ON1X_SDK_PRESERVE_EFFECTS;
 case mp::Analysis::Values:return ON1X_SDK_PRESERVE_VALUES;
 case mp::Analysis::Liveness:return ON1X_SDK_PRESERVE_LIVENESS;
 }
 return 0;
}

std::uint32_t mask(const std::vector<mp::Analysis>& values){
 std::uint32_t output=0;for(auto value:values)output|=analysis_bit(value);return output;
}

On1xSDK_AnalysisStringView view(std::string_view text){
 return {text.empty()?nullptr:text.data(),text.size()};
}

mp::Control control_value(std::uint32_t value){
 if(value==ON1X_SDK_OK)return mp::Control::Continue;
 if(value==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_CANCELLED))return mp::Control::Cancel;
 if(value==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED))return mp::Control::Deadline;
 return mp::Control::Failure;
}

On1xSDK_Status pass_status(mp::Status status){
 switch(status){
 case mp::Status::Complete:return ON1X_SDK_OK;
 case mp::Status::BudgetExhausted:return ON1X_SDK_LIMIT;
 case mp::Status::Cancelled:return ON1X_SDK_ANALYSIS_CANCELLED;
 case mp::Status::DeadlineExceeded:return ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED;
 case mp::Status::CallbackFailed:return ON1X_SDK_ERROR;
 }
 return ON1X_SDK_ERROR;
}

std::uint32_t public_outcome(mp::Outcome outcome){
 switch(outcome){
 case mp::Outcome::Unchanged:return ON1X_SDK_PASS_UNCHANGED;
 case mp::Outcome::Committed:return ON1X_SDK_PASS_COMMITTED;
 case mp::Outcome::BudgetExhausted:return ON1X_SDK_PASS_LIMIT;
 case mp::Outcome::Cancelled:return ON1X_SDK_PASS_CANCELLED;
 case mp::Outcome::DeadlineExceeded:return ON1X_SDK_PASS_DEADLINE;
 case mp::Outcome::CallbackFailed:return ON1X_SDK_PASS_CALLBACK_FAILED;
 }
 return ON1X_SDK_PASS_CALLBACK_FAILED;
}

On1xSDK_PassEvent event_record(const mp::Event& event,std::uint32_t stage){
 On1xSDK_PassEvent value{};
 value.abi_version=ON1X_SDK_ANALYSIS_ABI_VERSION;
 value.struct_size=sizeof value;value.pass=public_pass(event.pass);
 value.stage=stage;value.outcome=public_outcome(event.outcome);
 value.round=event.round;value.input_revision=event.input_revision;
 value.output_revision=event.output_revision;value.deterministic_work=event.work;
 value.attempted_rewrites=event.attempted_rewrites;return value;
}

std::string_view expected_target(const on1x::mir::Module& module){
 if(module.version==on1x::mir::legacy_format_version)return "portable-scalar-v1";
 if(module.version==on1x::mir::format_version)return "portable-numeric-product-v2";
 return "portable-numeric-v3";
}

bool whitespace(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}

On1xSDK_Status transition_failure(const on1x::Error& error){
 const std::string_view message=error.what();
 return message.find("budget exhausted")!=std::string_view::npos?
  ON1X_SDK_LIMIT:ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;
}

On1xSDK_Status translation_failure(const on1x::Error& error){
 const std::string_view message=error.what();
 return message.find("ON1X-MIR-EVAL:")!=std::string_view::npos&&
        message.find("argument")!=std::string_view::npos?
  ON1X_SDK_INVALID_ARGUMENT:ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;
}

} // namespace

extern "C" {

On1xSDK_Status on1x_sdk_pass_descriptor(
    On1xSDK_PassKind pass,On1xSDK_PassDescriptor* output){
 if(const auto status=check_output_header(output,sizeof *output);status!=ON1X_SDK_OK)return status;
 auto canonical=canonical_pass(pass);if(!canonical)return ON1X_SDK_UNSUPPORTED;
  try{
    const auto& descriptor=mp::descriptor(*canonical);
    On1xSDK_PassDescriptor value{};
    value.abi_version=output->abi_version;value.struct_size=output->struct_size;
    value.pass=pass;value.pass_version=descriptor.version;
   value.name=view(descriptor.name);value.preserves_mask=mask(descriptor.preserves);
   value.invalidates_mask=mask(descriptor.invalidates);
   value.target_profile=view(descriptor.target);value.verifier=view(descriptor.verifier);
   std::memcpy(output,&value,sizeof value);return ON1X_SDK_OK;
  }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
  catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status on1x_sdk_pipeline_parse(
    const char* text,size_t text_size,uint32_t round_limit,
    On1xSDK_Pipeline* output){
 if(const auto status=check_output_header(output,sizeof *output);status!=ON1X_SDK_OK)return status;
 if(text_size>ON1X_SDK_PIPELINE_MAX_TEXT)return ON1X_SDK_LIMIT;
 if((text_size&&!text)||!round_limit||round_limit>ON1X_SDK_PIPELINE_MAX_ROUNDS)
  return ON1X_SDK_INVALID_ARGUMENT;
 try{
  On1xSDK_Pipeline candidate{};
  candidate.abi_version=output->abi_version;candidate.struct_size=output->struct_size;
  candidate.pipeline_version=ON1X_SDK_PIPELINE_VERSION;
  candidate.round_limit=round_limit;
  std::size_t position=0;
  while(position<text_size){
   while(position<text_size&&whitespace(text[position]))++position;
   const auto begin=position;
   while(position<text_size&&text[position]!=',')++position;
   auto end=position;while(end>begin&&whitespace(text[end-1]))--end;
   if(begin==end)return ON1X_SDK_INVALID_ARGUMENT;
   const std::string_view token(text+begin,end-begin);
   std::optional<std::uint32_t> pass;
   for(std::uint32_t value=ON1X_SDK_PASS_CONSTANT_FOLD;value<=ON1X_SDK_PASS_LOCAL_SLOTS;++value){
    auto canonical=canonical_pass(value);if(canonical&&mp::descriptor(*canonical).name==token){pass=value;break;}
   }
   if(!pass)return ON1X_SDK_UNSUPPORTED;
   if(candidate.pass_count==ON1X_SDK_PIPELINE_MAX_PASSES)return ON1X_SDK_LIMIT;
   candidate.passes[candidate.pass_count++]=*pass;
   if(position<text_size){++position;if(position==text_size)return ON1X_SDK_INVALID_ARGUMENT;}
  }
  if(!candidate.pass_count)return ON1X_SDK_INVALID_ARGUMENT;
  std::memcpy(output,&candidate,sizeof candidate);return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
 catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status on1x_sdk_pass_manager_run(
    On1xSDK_AnalysisManager* manager,const On1xSDK_PassRun* run,
    const On1xSDK_PassResult** out_result){
 if(!out_result)return ON1X_SDK_INVALID_ARGUMENT;
 *out_result=nullptr;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(manager->impl->busy)return ON1X_SDK_BUSY;
 ManagerOperation operation(*manager->impl);
 if(const auto status=check_v2_header(run,sizeof *run);status!=ON1X_SDK_OK)return status;
 if(const auto status=check_v2_header(run->pipeline,sizeof *run->pipeline);status!=ON1X_SDK_OK)return status;
 if(const auto status=check_v2_header(&run->control,sizeof run->control);status!=ON1X_SDK_OK)return status;
 const auto& pipeline=*run->pipeline;
 if(pipeline.pipeline_version!=ON1X_SDK_PIPELINE_VERSION||!pipeline.pass_count||
     pipeline.pass_count>ON1X_SDK_PIPELINE_MAX_PASSES||!pipeline.round_limit||
     pipeline.round_limit>ON1X_SDK_PIPELINE_MAX_ROUNDS||pipeline.reserved)
  return ON1X_SDK_INVALID_ARGUMENT;
 if(run->control.result_limit)return ON1X_SDK_INVALID_ARGUMENT;
 if(run->work_limit>ON1X_SDK_ANALYSIS_MAX_WORK||
    run->control.work_limit>ON1X_SDK_ANALYSIS_MAX_WORK||
    run->rewrite_limit>UINT64_C(1048576)||run->verifier_node_limit>UINT64_C(1048576)||
    run->verifier_work_limit>ON1X_SDK_ANALYSIS_MAX_WORK)return ON1X_SDK_LIMIT;
 FullKey key;
 if(const auto status=parse_key(*manager,run->source,key);status!=ON1X_SDK_OK)return status;
 const Observed* source=nullptr;
 if(const auto status=current_source(*manager,key,source);status!=ON1X_SDK_OK)return status;
 if(key.base.target!=expected_target(source->module))return ON1X_SDK_UNSUPPORTED;
 try{
  mp::Options options;options.pipeline.clear();
  for(std::size_t index=0;index<pipeline.pass_count;++index){
   auto pass=canonical_pass(pipeline.passes[index]);if(!pass)return ON1X_SDK_UNSUPPORTED;
   options.pipeline.push_back(*pass);
  }
  const auto initial=callback_control(run->control.poll,run->control.userdata,0);
  if(initial!=ON1X_SDK_OK)
   return initial==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_CANCELLED)?ON1X_SDK_ANALYSIS_CANCELLED:
          initial==static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED)?ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED:
          ON1X_SDK_ERROR;
  options.target=key.base.target;
  options.work=static_cast<std::size_t>(std::min(run->work_limit,run->control.work_limit));
  options.rewrites=static_cast<std::size_t>(run->rewrite_limit);
  options.rounds=pipeline.round_limit;
  options.verification.nodes=static_cast<std::size_t>(run->verifier_node_limit);
  options.verification.work=static_cast<std::size_t>(run->verifier_work_limit);
  options.control=[control=run->control](std::size_t work){
   return control_value(callback_control(control.poll,control.userdata,work));
  };
  if(run->instrument)options.instrumentation=[callback=run->instrument,
   userdata=run->instrument_userdata](const mp::Event& event,bool before){
    auto record=event_record(event,before?ON1X_SDK_PASS_STAGE_BEFORE:ON1X_SDK_PASS_STAGE_AFTER);
    std::uint32_t value=ON1X_SDK_ERROR;
    try{value=callback(userdata,&record);}catch(...){value=ON1X_SDK_ERROR;}
    return control_value(value);
  };
  auto result=mp::run(source->module,std::move(options));
  if(const auto status=pass_status(result.status());status!=ON1X_SDK_OK)return status;
  if(result.output().size()>ON1X_SDK_MAX_PROGRAM_BYTES)return ON1X_SDK_LIMIT;
  auto candidate=std::make_unique<On1xSDK_PassResult>(
   manager->impl->generation,key,source->module,source->wire,std::move(result));
  manager->impl->pass.reset();mutate(*manager->impl);
  candidate->generation=manager->impl->generation;
  auto* view=candidate.get();manager->impl->pass=std::move(candidate);
  *out_result=view;return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
 catch(const on1x::Error& error){return transition_failure(error);}
 catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status on1x_sdk_pass_result_info(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_PassResult* result,
    On1xSDK_PassResultInfo* output){
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_pass_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 if(const auto status=check_output_header(output,sizeof *output);status!=ON1X_SDK_OK)return status;
 On1xSDK_PassResultInfo value{};
 value.abi_version=output->abi_version;value.struct_size=output->struct_size;
 value.source=public_key(result->data.source);
 value.deterministic_work=result->data.result.work();
 value.attempted_rewrites=result->data.result.attempted_rewrites();
 value.event_count=result->data.result.events().size();
 value.changed=result->data.result.changed();value.complete=1;
 std::memcpy(output,&value,sizeof value);return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_pass_result_bytes(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_PassResult* result,
    const uint8_t** bytes,size_t* size){
 if(!bytes||!size)return ON1X_SDK_INVALID_ARGUMENT;
 *bytes=nullptr;*size=0;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_pass_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 const auto& wire=result->data.after_wire;
 *bytes=wire.empty()?nullptr:reinterpret_cast<const std::uint8_t*>(wire.data());
 *size=wire.size();return ON1X_SDK_OK;
}

size_t on1x_sdk_pass_result_event_count(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_PassResult* result){
 return valid_pass_result(manager,result)?result->data.result.events().size():0;
}

On1xSDK_Status on1x_sdk_pass_result_event_at(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_PassResult* result,
    size_t index,On1xSDK_PassEvent* output){
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_pass_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 if(const auto status=check_output_header(output,sizeof *output);status!=ON1X_SDK_OK)return status;
 if(index>=result->data.result.events().size())return ON1X_SDK_ANALYSIS_NOT_FOUND;
 auto value=event_record(result->data.result.events()[index],ON1X_SDK_PASS_STAGE_AFTER);
 value.abi_version=output->abi_version;value.struct_size=output->struct_size;
 std::memcpy(output,&value,sizeof value);return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_pass_verify_transition(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_AnalysisKey* source_key,
    On1xSDK_PassKind pass,const void* candidate_bytes,size_t candidate_size,
    uint64_t verifier_node_limit,uint64_t verifier_work_limit){
 if(!source_key)return ON1X_SDK_INVALID_ARGUMENT;
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 auto canonical=canonical_pass(pass);if(!canonical)return ON1X_SDK_UNSUPPORTED;
 if((candidate_size&&!candidate_bytes)||candidate_size>ON1X_SDK_MAX_PROGRAM_BYTES)
  return candidate_size>ON1X_SDK_MAX_PROGRAM_BYTES?ON1X_SDK_LIMIT:ON1X_SDK_INVALID_ARGUMENT;
 if(verifier_node_limit>UINT64_C(1048576)||verifier_work_limit>ON1X_SDK_ANALYSIS_MAX_WORK)
  return ON1X_SDK_LIMIT;
 FullKey key;
 if(const auto status=parse_key(*manager,*source_key,key);status!=ON1X_SDK_OK)return status;
 const Observed* source=nullptr;
 if(const auto status=current_source(*manager,key,source);status!=ON1X_SDK_OK)return status;
 try{
  const auto* data=candidate_size?static_cast<const char*>(candidate_bytes):"";
  const std::string_view text(data,candidate_size);
  auto candidate=on1x::mir::deserialize(text,"<sdk-pass-candidate>");
  if(on1x::mir::serialize(candidate)!=text)return ON1X_SDK_ANALYSIS_VERIFICATION_FAILED;
  mp::verify_transition(source->module,candidate,*canonical,
   {static_cast<std::size_t>(verifier_node_limit),static_cast<std::size_t>(verifier_work_limit)});
  return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
 catch(const on1x::Error& error){return transition_failure(error);}
 catch(...){return ON1X_SDK_ERROR;}
}

On1xSDK_Status on1x_sdk_translation_test(
    const On1xSDK_AnalysisManager* manager,const On1xSDK_PassResult* result,
    const On1xSDK_TranslationTest* test,On1xSDK_TranslationEvidence* output){
 if(!valid_manager(manager))return ON1X_SDK_ANALYSIS_INVALID_STATE;
 if(!valid_pass_result(manager,result))return ON1X_SDK_ANALYSIS_STALE_REVISION;
 if(const auto status=check_v2_header(test,sizeof *test);status!=ON1X_SDK_OK)return status;
 if(const auto status=check_output_header(output,sizeof *output);status!=ON1X_SDK_OK)return status;
 if(!test->function_name||!test->function_name_size||test->function_name_size>ON1X_SDK_NAME_MAX||
    (test->scalar_argument_count&&!test->scalar_arguments)||test->scalar_argument_count>64||
    !test->evaluator_step_limit||test->evaluator_step_limit>UINT64_C(10000000)||
    !test->evaluator_call_depth_limit||test->evaluator_call_depth_limit>256)
  return ON1X_SDK_INVALID_ARGUMENT;
 const std::string_view name(test->function_name,test->function_name_size);
 if(name.find('\0')!=std::string_view::npos)return ON1X_SDK_INVALID_ARGUMENT;
 const on1x::mir::Function* function=nullptr;
 for(const auto& value:result->data.before.functions)if(value.name==name){function=&value;break;}
 if(!function)return ON1X_SDK_ANALYSIS_NOT_FOUND;
 if(function->blocks.front().parameters.size()!=test->scalar_argument_count)return ON1X_SDK_INVALID_ARGUMENT;
 for(const auto& parameter:function->blocks.front().parameters)
  if(parameter.type.is_tuple())return ON1X_SDK_UNSUPPORTED;
 try{
  std::vector<std::int64_t> arguments;
  if(test->scalar_argument_count)
   arguments.assign(test->scalar_arguments,
                    test->scalar_arguments+test->scalar_argument_count);
  auto before=on1x::mir::evaluate(result->data.before,name,arguments,
   static_cast<std::size_t>(test->evaluator_step_limit),
   static_cast<std::size_t>(test->evaluator_call_depth_limit));
  auto after=on1x::mir::evaluate(result->data.after,name,arguments,
   static_cast<std::size_t>(test->evaluator_step_limit),
   static_cast<std::size_t>(test->evaluator_call_depth_limit));
  if(before.status==on1x::mir::Status::BudgetExhausted||
     after.status==on1x::mir::Status::BudgetExhausted)return ON1X_SDK_LIMIT;
  const bool equal=before.status==after.status&&before.type==after.type&&
   before.value==after.value&&before.output==after.output&&
   before.diagnostic==after.diagnostic&&
   std::tie(before.where.file,before.where.offset,before.where.line,before.where.column)==
   std::tie(after.where.file,after.where.offset,after.where.line,after.where.column);
  static constexpr std::string_view subset=
   "canonical-mir-v1-v3/reference-evaluator/one-scalar-invocation";
   On1xSDK_TranslationEvidence value{};
   value.abi_version=output->abi_version;value.struct_size=output->struct_size;
   value.evidence=equal?ON1X_SDK_TRANSLATION_EVIDENCED_EQUAL:
   ON1X_SDK_TRANSLATION_EVIDENCED_DIFFERENT;
  value.is_equivalence_certificate=0;value.tests_executed=1;value.subset=view(subset);
  std::memcpy(output,&value,sizeof value);return ON1X_SDK_OK;
 }catch(const std::bad_alloc&){return ON1X_SDK_LIMIT;}
  catch(const on1x::Error& error){return translation_failure(error);}
 catch(...){return ON1X_SDK_ERROR;}
}

} // extern "C"

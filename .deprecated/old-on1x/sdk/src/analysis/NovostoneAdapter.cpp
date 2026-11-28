#include "AnalysisInternal.hpp"

#include <on1x/backend/relational.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace on1x::sdk::analysis_detail {
namespace {
namespace relational=on1x::mir::relational;
namespace datalog=novostone::datalog;

std::uint64_t id(const datalog::Value& value){
 const auto* text=std::get_if<std::string>(&value);
 if(!text)throw std::runtime_error("Novostone MIR identity is not a symbol");
 std::uint64_t output=0;
 const auto [end,error]=std::from_chars(text->data(),text->data()+text->size(),output);
 if(error!=std::errc{}||end!=text->data()+text->size())
  throw std::runtime_error("Novostone MIR identity is not canonical uint64");
 return output;
}

const std::string& symbol(const datalog::Value& value){
 const auto* text=std::get_if<std::string>(&value);
 if(!text)throw std::runtime_error("Novostone effect is not a symbol");
 return *text;
}

mir::analysis::Control poll(const mir::analysis::Options& options,std::size_t work){
 if(!options.control)return mir::analysis::Control::Continue;
 try{return options.control(work);}catch(...){return mir::analysis::Control::Failure;}
}

void clear(mir::analysis::Result& result){
 result.blocks.clear();result.edges.clear();result.dominators.clear();
 result.loops.clear();result.loop_members.clear();result.definitions.clear();
 result.uses.clear();result.liveness.clear();result.effects.clear();
 result.function_effects.clear();
}

void stopped(mir::analysis::Result& result,mir::analysis::Control control){
 switch(control){
 case mir::analysis::Control::Continue:return;
 case mir::analysis::Control::Cancel:
  result.status=mir::analysis::Status::Cancelled;result.diagnostic="analysis cancelled";break;
 case mir::analysis::Control::Deadline:
  result.status=mir::analysis::Status::DeadlineExceeded;result.diagnostic="analysis deadline exceeded";break;
 case mir::analysis::Control::Failure:
  result.status=mir::analysis::Status::CallbackFailed;result.diagnostic="analysis control callback failed";break;
 }
 clear(result);
}

std::uint32_t bit(std::string_view name){
 if(name=="trap")return 1u<<0;
 if(name=="memory")return 1u<<1;
 if(name=="call")return 1u<<2;
 if(name=="io")return 1u<<3;
 throw std::runtime_error("Novostone returned an unknown MIR effect");
}

std::optional<std::size_t> projection_work(const mir::Module& module,
                                           std::size_t limit){
 std::size_t work=0;
 auto add=[&](std::size_t amount){
  if(amount>limit-work)return false;
  work+=amount;return true;
 };
 if(!add(1))return {};
 for(const auto& function:module.functions){
  if(!add(3)||!add(2*function.slots.size()))return {};
  for(const auto& block:function.blocks){
   if(!add(3)||!add(2*block.parameters.size()))return {};
   for(const auto& instruction:block.instructions){
    std::size_t amount=2+instruction.operands.size();
    if(instruction.op==mir::Op::Constant)++amount;
    if(instruction.op==mir::Op::Call||instruction.op==mir::Op::Load||
       instruction.op==mir::Op::Store)++amount;
    if(mir::effect(instruction.op)!=mir::Effect::Pure)++amount;
    if(!add(amount))return {};
   }
   std::size_t amount=3+(block.exit.value?1:0);
   for(const auto& edge:block.exit.edges){
    if(edge.arguments.size()>std::numeric_limits<std::size_t>::max()-2)return {};
    if(!add(2+edge.arguments.size()))return {};
   }
   if(!add(amount))return {};
  }
 }
 return work;
}

} // namespace

mir::analysis::Result run_novostone(const mir::Module& module,
                                    mir::analysis::Options options){
 if(module.version!=mir::legacy_format_version)
  throw std::invalid_argument("Novostone mapping v1 accepts MIR v1 only");
 auto result=mir::analysis::run(module,options);
 if(result.status!=mir::analysis::Status::Complete)return result;
 if(auto decision=poll(options,result.work);decision!=mir::analysis::Control::Continue){
  stopped(result,decision);return result;
 }
 const auto remaining=options.work>result.work?options.work-result.work:0;
 const auto import_work=projection_work(module,remaining);
 if(!import_work){
  result.status=mir::analysis::Status::BudgetExhausted;
  result.diagnostic="Novostone projection work budget exhausted";
  clear(result);
  return result;
 }
 relational::ImportLimits import;
 import.mir=options.verification;
 import.facts=std::min<std::size_t>(ON1X_SDK_ANALYSIS_MAX_RESULTS,*import_work);
 relational::Store store(module,import);
 result.work+=*import_work;
 auto snapshot=store.snapshot();
 const auto query_work=options.work-result.work;
 const datalog::Limits limits{options.records,query_work};
 if(options.kind==mir::analysis::Kind::CFG){
  auto reachable=snapshot.query("Reachable",limits);
  std::set<std::pair<mir::Id,mir::Id>> expected,actual;
  for(const auto& function:module.functions)for(const auto& block:function.blocks)
   expected.emplace(function.id,block.id);
  for(const auto& row:reachable.result().rows){
   if(row.size()!=2)throw std::runtime_error("invalid Novostone Reachable row");
   actual.emplace(id(row[0]),id(row[1]));
  }
  if(actual!=expected)throw std::runtime_error("Novostone reachability disagrees with verified MIR");
  result.work+=reachable.result().work;
 }else if(options.kind==mir::analysis::Kind::LivenessUseDef){
  auto live_in=snapshot.query("LiveIn",limits);
  auto left=query_work>live_in.result().work?query_work-live_in.result().work:0;
  auto live_out=snapshot.query("LiveOut",{options.records,left});
  std::map<mir::Id,mir::Id> functions;
  for(const auto& function:module.functions)for(const auto& block:function.blocks)
   functions.emplace(block.id,function.id);
  std::vector<mir::analysis::LiveValue> values;
  auto import=[&](const auto& rows,bool incoming){
   for(const auto& row:rows){
    if(row.size()!=2)throw std::runtime_error("invalid Novostone liveness row");
    const auto block=id(row[0]),value=id(row[1]);
    if(!functions.contains(block))throw std::runtime_error("Novostone liveness names an unknown block");
    values.push_back({functions.at(block),block,value,incoming});
   }
  };
  import(live_in.result().rows,true);import(live_out.result().rows,false);
  std::sort(values.begin(),values.end());
  values.erase(std::unique(values.begin(),values.end()),values.end());
  const auto fixed=result.definitions.size()+result.uses.size();
  if(fixed>options.records||values.size()>options.records-fixed){
   result.status=mir::analysis::Status::BudgetExhausted;
   result.diagnostic="Novostone liveness result budget exhausted";
   result.definitions.clear();result.uses.clear();result.liveness.clear();
   return result;
  }
  result.liveness=std::move(values);
  result.work+=live_in.result().work+live_out.result().work;
 }else if(options.kind==mir::analysis::Kind::Effects){
  auto effects=snapshot.query("MayEffect",limits);
  std::map<mir::Id,std::uint32_t> masks;
  for(const auto& function:module.functions)masks.emplace(function.id,0);
  for(const auto& row:effects.result().rows){
   if(row.size()!=2)throw std::runtime_error("invalid Novostone MayEffect row");
   const auto function=id(row[0]);
   if(!masks.contains(function))throw std::runtime_error("Novostone effect names an unknown function");
   masks[function]|=bit(symbol(row[1]));
  }
  std::map<mir::Id,std::uint32_t> canonical;
  for(const auto& summary:result.function_effects)canonical.emplace(summary.function,summary.mask);
  if(masks!=canonical)throw std::runtime_error("Novostone effects disagree with canonical MIR analysis");
  result.function_effects.clear();
  for(const auto& [function,mask]:masks)result.function_effects.push_back({function,mask});
  result.work+=effects.result().work;
 }else{
  throw std::invalid_argument("analysis is outside Novostone mapping v1");
 }
 if(result.work>options.work){
  result.status=mir::analysis::Status::BudgetExhausted;
  result.diagnostic="Novostone cumulative work budget exhausted";
  result.blocks.clear();result.edges.clear();result.definitions.clear();
  result.uses.clear();result.liveness.clear();result.effects.clear();
  result.function_effects.clear();return result;
 }
 if(auto decision=poll(options,result.work);decision!=mir::analysis::Control::Continue)
  stopped(result,decision);
 return result;
}

} // namespace on1x::sdk::analysis_detail

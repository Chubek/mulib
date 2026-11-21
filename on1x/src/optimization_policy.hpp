#pragma once
#include <on1x/backend/passes.hpp>
#include <on1x/processor.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace on1x::driver {
enum class Stage { Processor, Lowering, MIR };

// Names come from the linked compiler, including alternate authored tables.
// 'all' is a selector, never a pass. Keep the descriptor pipeline's order.
struct OptimizationPolicy {
 std::string profile;
 processor::RuleSelection rules;
 bool decision_ssa=false;
 std::vector<mir::passes::Pass> pipeline;
 struct Entry {std::string_view name;Stage stage;bool enabled;};
 std::vector<Entry> entries;

 std::string listing() const {
  std::ostringstream out;out<<"{\"profile\":\""<<profile<<"\",\"optimizations\":[";
  bool first=true;
  for(const auto& entry:entries){
   if(!first)out<<',';
   first=false;
   const char* stage=entry.stage==Stage::Processor?"processor":entry.stage==Stage::Lowering?"lowering":"mir";
   // Authored identifiers, descriptor names and profile values are closed alphabets.
   out<<"{\"name\":\""<<entry.name<<"\",\"stage\":\""<<stage<<"\",\"enabled\":"
      <<(entry.enabled?"true":"false")<<"}";
  }
  out<<"]}\n";return out.str();
 }
};

struct OptimizationRequest {
 std::optional<std::string> profile;
 std::map<std::string,bool,std::less<>> selectors;
 void set_profile(std::string name) {
  if(profile)throw std::runtime_error("specify only one optimization profile");
  if(name!="debug"&&name!="release"&&name!="default")
   throw std::runtime_error("unknown optimization profile: "+name);
  profile=std::move(name);
 }
 void select(std::string name,bool enabled) {
  auto [it,added]=selectors.emplace(std::move(name),enabled);
  if(!added&&it->second!=enabled)
   throw std::runtime_error("conflicting optimization selections: "+it->first);
 }
 OptimizationPolicy resolve(bool default_mir) const {
  OptimizationPolicy policy;
  policy.profile=profile.value_or("default");
  const auto all=selectors.find("all");
  auto enabled=[&](std::string_view name,bool base){
   if(const auto it=selectors.find(name);it!=selectors.end())return it->second;
   return all!=selectors.end()?all->second:base;
  };
  const bool source_base=policy.profile!="debug";
  const bool mir_base=policy.profile=="release"||(policy.profile=="default"&&default_mir);
  policy.rules.default_enabled=all!=selectors.end()?all->second:source_base;
  for(auto name:processor::rule_names()){
   const bool on=enabled(name,source_base);
   policy.entries.push_back({name,Stage::Processor,on});
   if(on!=policy.rules.default_enabled)policy.rules.overrides.emplace(name,on);
  }
  policy.rules.partial_eval=enabled("partial-eval",policy.profile=="release");
  policy.entries.push_back({"partial-eval",Stage::Processor,policy.rules.partial_eval});
  policy.decision_ssa=enabled("decision-ssa",source_base);
  policy.entries.push_back({"decision-ssa",Stage::Lowering,policy.decision_ssa});
  for(auto pass:mir::passes::Options{}.pipeline){
   auto name=mir::passes::descriptor(pass).name;
   const bool on=enabled(name,mir_base);
   if(on)policy.pipeline.push_back(pass);
   policy.entries.push_back({name,Stage::MIR,on});
  }
  for(const auto& [name,on]:selectors){
   if(name!="all"&&std::none_of(policy.entries.begin(),policy.entries.end(),
      [&](const auto& e){return e.name==name;}))
    throw std::runtime_error("unknown optimization: "+name+" (see --list-optimizations)");
  }
  return policy;
 }
 void check_stages(const OptimizationPolicy& policy,bool processor,bool lowering,bool mir) const {
  for(const auto& e:policy.entries){
   if(selectors.contains(e.name)&&!(e.stage==Stage::Processor?processor:e.stage==Stage::Lowering?lowering:mir))
    throw std::runtime_error("optimization "+std::string(e.name)+" is not applicable in this mode");
  }
 }
};
}

#pragma once
#include <on1x/ast.hpp>
#include <map>
#include <string_view>
#include <utility>

namespace on1x::processor {
enum class Phase { Desugar, Canonicalize, Semantic };
struct RuleSelection {
  bool default_enabled=true;
  std::map<std::string,bool,std::less<>> overrides;
  // Separate from authored rules; default API behavior stays unchanged.
  bool partial_eval=false;
  bool enabled(std::string_view name) const {
    auto it=overrides.find(name);
    return it==overrides.end()?default_enabled:it->second;
  }
};
std::vector<std::string_view> rule_names();
struct Limits {
  std::size_t nodes=262144, rewrites=65536, steps=8388608;
  std::size_t depth=2*max_syntax_depth+8;
};
struct Stats {
  std::size_t visited=0, rewrites=0;
  // Authored table order, including zero-hit rules; never hash iteration order.
  std::vector<std::pair<std::string,std::size_t>> rules;
};
// Requires authoritative type, control and ownership verification beforehand.
// Transforms a private clone and publishes only a complete, bounded result.
// The caller must re-run the authoritative verifiers before using that result.
Stats apply(Unit&, Phase, const Limits& = {});
Stats apply(Unit&, Phase, const Limits&, const RuleSelection&);
// During desugaring, finish canonicalizing each rewritten operand before its
// parent chooses between a neutral rule and a control-flow expansion.
// Both rule families share one transaction and work/rewrite budget.
struct NormalizationStats { Stats canonicalize, desugar; };
NormalizationStats desugar_normalized(Unit&, const Limits& = {}, const RuleSelection& = {});
}

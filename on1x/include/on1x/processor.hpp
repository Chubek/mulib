#pragma once
#include <on1x/ast.hpp>
#include <utility>

namespace on1x::processor {
enum class Phase { Desugar, Canonicalize, Semantic };
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
}

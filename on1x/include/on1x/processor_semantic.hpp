#pragma once
#include <on1x/ast.hpp>
#include <on1x/processor.hpp>
#include <on1x/partial_eval.hpp>
#include <string>
#include <utility>
#include <vector>

namespace on1x::processor {
enum class SemanticStage { Structural, Typed };
struct SemanticStats {
 std::size_t visited=0,checks=0;
 std::vector<std::pair<std::string,std::size_t>> rules;
};
// Read-only checks driven by compiled semantic.o1m constraints. Typed checks
// inspect existing annotations; they never infer or repair them.
SemanticStats validate(const Unit&,SemanticStage,const Limits& = {});
struct Report {
 bool verified=false;
 Stats desugar,canonicalize;
 SemanticStats structural,typed_before,typed_after;
  PartialEvalStats partial_eval;
  NovostoneStats novostone_hir;
  FirestoneStats firestone_hir;
};
// Deterministic compiler-side report, with rule-source provenance.
std::string report(const Report&);
}

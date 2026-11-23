#pragma once
#include <on1x/processor_semantic.hpp>
#include <functional>

namespace on1x::hir {
// Admission has already checked the original unit. Only the semantic checker
// supplies recheck; it validates existing annotations, never re-elaborates.
void optimize_checked(Unit&, const processor::RuleSelection&,
                      const std::function<processor::SemanticStats(Unit&)>& recheck,
                      processor::Report&);
}

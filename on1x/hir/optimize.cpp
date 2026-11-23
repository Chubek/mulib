#include "optimize.hpp"
#include <on1x/clone.hpp>

namespace on1x::hir {
void optimize_checked(Unit& unit, const processor::RuleSelection& selection,
                      const std::function<processor::SemanticStats(Unit&)>& recheck,
                      processor::Report& evidence) {
    if (!selection.firestone_hir && !selection.novostone_hir && !selection.partial_eval &&
        !selection.default_enabled && selection.overrides.empty()) {
        // The original unit is unchanged, so its read-only typed stats are exact.
        evidence.typed_after = evidence.typed_before;
        evidence.verified = true;
        return;
    }

    std::size_t budget = 262144;
    auto candidate = clone_unit(unit, budget);
    auto verify = [&] { evidence.typed_after = recheck(candidate); };

    // Full-width Firestone first; Novostone can still fold its Boolean subset.
    if (selection.firestone_hir) {
        evidence.firestone_hir = selection.firestone_hir(candidate);
        if (evidence.firestone_hir.folds || evidence.firestone_hir.divide_one) verify();
    }
    if (selection.novostone_hir) {
        evidence.novostone_hir = selection.novostone_hir(candidate);
        if (evidence.novostone_hir.applied) verify();
    }
    // Expose neutral operands before desugaring can turn them into decisions
    // (e.g. `x && !false`). Desugaring may introduce new canonical redexes.
    using processor::Phase;
    evidence.canonicalize = processor::apply(candidate, Phase::Canonicalize, {}, selection);
    if (evidence.canonicalize.rewrites) verify();
    if (selection.partial_eval) {
        evidence.partial_eval = processor::partial_evaluate(candidate, unit);
        const auto& pe = evidence.partial_eval;
        if (pe.folds || pe.static_calls || pe.residual_calls) verify();
    }
    auto normalized = processor::desugar_normalized(candidate, {}, selection);
    evidence.desugar = std::move(normalized.desugar);
    auto& total = evidence.canonicalize;
    total.visited += normalized.canonicalize.visited;
    total.rewrites += normalized.canonicalize.rewrites;
    // Both traversals enumerate the same immutable canonical rule table.
    for (std::size_t i = 0; i < total.rules.size(); ++i)
        total.rules[i].second += normalized.canonicalize.rules[i].second;
    verify();
    unit = std::move(candidate);
    evidence.verified = true;
}
}

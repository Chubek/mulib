#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::mir {
// Resolve exact backend/target selectors (empty directive selectors match any
// product), apply proven policy patterns, and return verified executable MIR.
// The input is unchanged. All directives, including inactive ones, are consumed.
// Speculation enables conditional patterns; it does not bypass their proofs.
Module specialize(const Module& module, std::string_view backend,
                  std::string_view target, bool speculate = false);
}

#include "module_elevation_fault.hpp"

#ifdef ON1X_AUROCKS_CONTEXTUAL_FAULTS
#include <aurocks/Fault.h>
#include <DiagnosticsBytePEG.hpp>
#include <string_view>
#include "on1x_contextual_fault_bank.h"
#endif

namespace on1x {
const char* zero_elevation_fault() {
#ifdef ON1X_AUROCKS_CONTEXTUAL_FAULTS
    // Match the generated rule with the same byte PEG as Aurocks' runtime.
    // Unlike lexical recovery, report through the parser's structured Error
    // rather than the runtime's stderr callback. Never propose a replacement.
    const auto& fault = on1x_contextual_fault_bank[0];
    if (fault.disposition != AUROCKS_ERROR) return nullptr;
    constexpr std::string_view clause = "elevate(0)";
    const firestone::lr::LRDiagBytePEG peg(fault.pattern);
    return peg.matchPrefixAt(clause, 0) == clause.size() ? fault.message : nullptr;
#else
    return "shared elevation cannot be zero; use a positive decimal integer";
#endif
}
}

#pragma once
#include <string_view>

namespace on1x::ffi {

// An observation is not a descriptor or authorization. An empty member asks
// whether any discovery-only declaration in the module has a finding.
struct TaintObservation {
    std::string_view module, member, code;
};

const TaintObservation* find_taint(std::string_view module, std::string_view member = {}) noexcept;

} // namespace on1x::ffi

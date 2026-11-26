#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::mir {
// The trace is host-side qualification instrumentation, not guest output, an
// ownership instruction, or an ABI for observing owner addresses.
struct NominalOwnerEvent {
 Op op;
 Id nominal, from, to; // Zero denotes no place on create/drop.
 std::int64_t payload;
};
using NominalOwnerObserver = std::function<void(const NominalOwnerEvent&)>;

// Explicit opt-in for verified, in-memory nominal-owner MIR only. The
// synchronous observer, if supplied, is called once per completed owner
// instruction. Unexecuted instructions (including on budget exhaustion) do
// not report events. The observer must not mutate the input module; its
// exceptions propagate as host errors.
Evaluation evaluate_nominal_owner(const Module&, std::string_view function="main",
                                  const std::vector<std::int64_t>& arguments={},
                                  std::size_t steps=1000000,
                                  const NominalOwnerObserver& observe={});

// C11 source with private, fixed local owner storage. Qualification mode
// writes owner events to stderr; default emission adds no trace effects.
std::string emit_nominal_owner_c(const Module&, bool trace=false);
}

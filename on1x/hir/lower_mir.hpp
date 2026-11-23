#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::hir {
// Private source driver entry for the VM product layout. Serialized MIR never
// runs HIR optimizations; mir::lower_for_vm keeps its existing API and defaults.
mir::Module lower_vm(const Unit&, mir::Limits, bool memgraph_bounds);
}

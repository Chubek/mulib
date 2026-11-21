#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::mir {
// Converts finite first-order closure values in a checked unit into typed
// products and direct functions. The caller must revalidate the resulting HIR.
std::optional<Unit> vm_closures(const Unit&, Limits);
}

#pragma once
#include <on1x/processor.hpp>

namespace on1x::hir {
// Edits only a private, checked candidate; the processor rechecks it.
processor::FirestoneStats fold_with_firestone(Unit&);
}

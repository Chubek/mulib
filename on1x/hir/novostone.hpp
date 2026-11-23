#pragma once
#include <on1x/processor.hpp>

namespace on1x::hir {
// Edits a private, checked candidate; the processor must recheck before publication.
processor::NovostoneStats fold_with_novostone(Unit&);
}

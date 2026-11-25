#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::hir {
// The module is the fresh lowering of this checked unit. Requests are advisory:
// neither producer participates in ownership checking or foreign admission.
void append_analysis_directives(const Unit&,mir::Module&);
}

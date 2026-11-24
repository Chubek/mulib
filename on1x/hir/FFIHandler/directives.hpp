#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::stdffi {
// The recipes' private HIR names are not source-addressable. Only functions
// that have actually been materialized and lowered receive metadata.
void attach_directives(mir::Module&);
}

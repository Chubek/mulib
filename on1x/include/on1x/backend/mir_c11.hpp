#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::mir {
// An empty target generates portable C; target-specific placement is then
// omitted. The target selects hints, never C flags, headers or link inputs.
std::string emit_directed_c(const Module&, std::optional<std::size_t> stack_bytes,
                            std::string_view target, bool speculate);
}

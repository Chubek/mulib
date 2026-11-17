#pragma once
#include <on1x/ast.hpp>

namespace on1x::canonical {
// Source range -> checked preheader/While/latch. Resolve exits after macros,
// PPX and staging. Structured HIR is the C11 bridge; MIR owns verified CFG SSA.
void lower_control(Unit& unit);
bool needs_control(const Unit& unit);
// Bounded independent verification. Does not trust serialized target IDs,
// source label text or a lowered flag.
void verify_control(const Unit& unit);
}

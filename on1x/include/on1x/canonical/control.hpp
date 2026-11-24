#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x::canonical {
// Private names in the source expansion are alpha-renamed before PPX. The
// length marker fits Unit-v1 ValueData, and is erased by semantic checking.
inline constexpr std::string_view iteration_name_prefix="@for_in_";
inline constexpr std::string_view iteration_length_marker="@for-in";
inline constexpr const char* iteration_type_error=
 "ON1X-FOR-IN: for-in requires a known fixed array or snapshot slice";
// Source range -> checked preheader/While/latch. Resolve exits after macros,
// PPX and staging. Structured HIR is the C11 bridge; MIR owns verified CFG SSA.
void lower_control(Unit& unit);
bool needs_control(const Unit& unit);
// Bounded independent verification. Does not trust serialized target IDs,
// source label text or a lowered flag.
void verify_control(const Unit& unit);
}

#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x {
// Decimal binary64 source grammar, independent of locale and C literal parsing.
bool float_spelling(std::string_view text);
std::int64_t float_literal_bits(std::string_view text,const Location& where);
}

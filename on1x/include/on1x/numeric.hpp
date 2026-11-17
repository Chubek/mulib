#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x {
// Decimal f32/f64 and unsigned literal spellings, independent of locale and C
// literal parsing. Unsuffixed literals retain their i64/f64 types.
bool float_spelling(std::string_view text);
bool unsigned_spelling(std::string_view text);
Type numeric_literal_type(std::string_view text);
// Scalar bits are exact storage payloads, not signed encodings of unsigned math.
std::int64_t integer_literal_bits(std::string_view text,const Location& where);
std::int64_t float_literal_bits(std::string_view text,const Location& where);
}

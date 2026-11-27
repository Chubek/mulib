#pragma once
#include <on1x/stdffi.hpp>
#include <array>

namespace on1x::stdffi::tables {
enum class Code { Bool, Integer, Param, Lt, Le, Gt, Ge, Eq, And, Or, Add, Sub, If };
struct Node {
 Code op;
 bool boolean;
 std::size_t a,b,c;
 std::int64_t immediate;
};
struct Recipe { std::size_t guard,body,directives_begin,directives_count; };
struct Directive { std::string_view backend,target,kind,action; bool speculative; };
}

#include "on1x_stdffi_tables.hpp"

#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace on1x::meta {
inline constexpr std::uint32_t no_node = std::numeric_limits<std::uint32_t>::max();
enum class Phase { Canonicalize, Desugar, Semantic };
enum class NodeKind { Capture, Unary, Binary, Boolean, Integer, Unit, Select };
enum class Guard { Always, Boolean, Integer, Pure };
struct Node {
 NodeKind kind;
 const char* text;
 std::array<std::uint32_t, 3> children;
 std::uint32_t arity;
};
struct Rule {
 const char* name;
 Phase phase;
 std::uint32_t priority, match, replacement;
 Guard guard;
 const char* file;
 std::uint32_t line, column;
};
struct Constraint {
 const char* name;
 const char* kind;
 const char* property;
 const char* value;
 const char* message;
 const char* file;
 std::uint32_t line, column;
};
struct Budgets { std::uint32_t steps, nodes, depth; };
}

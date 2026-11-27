#pragma once
#include <on1x/ast.hpp>

namespace on1x {
// Source-only observations; the loader has already resolved these bindings.
enum class UnusedKind { Import, Variable, Type };
struct UnusedBinding {
 UnusedKind kind;
 Location where;
 std::string name, binding_kind;
};
}

#pragma once
#include <on1x/ast.hpp>

namespace on1x {
// Compiler-side budgets, not runtime limits. Lower values also permit focused
// regression tests. No inference term IDs escape this pass or enter CDecl.
struct InferenceLimits {
 std::size_t nodes = 65536;
 std::size_t work = 4 * 1024 * 1024;
 std::size_t depth = max_syntax_depth + 1; // Includes the outer declaration signature.
 std::size_t type_nodes = 16384; // Expanded size, not merely shared DAG nodes.
};

// Solves monotypes only, without generalization/defaulting. Replaces every
// inference hole before the ordinary type/resource/ABI checker is allowed to
// admit the program. Fully annotated units keep the existing checking path.
void infer_types(Unit& unit, const InferenceLimits& limits = {});
}

#pragma once
#include <on1x/ast.hpp>

namespace on1x {
// Compiler-side budgets, not runtime limits. Lower values also permit focused
// regression tests. Unification graph storage never escapes this pass; quantified
// HIR identities are compiler-private and have no foreign CDecl meaning.
struct InferenceLimits {
 std::size_t nodes = 65536;
 std::size_t work = 4 * 1024 * 1024;
 std::size_t depth = max_syntax_depth + 1; // Includes the outer declaration signature.
 std::size_t type_nodes = 16384; // Expanded size, not merely shared DAG nodes.
 std::size_t specializations = 1024;
 std::size_t expanded_ast = 262144;
};

// Solves monotypes, global recursive-component schemes, value-restricted local
// schemes (including patterns), and explicit predicative universal parameters.
// Elaborates concrete uses into bounded C11 specializations/bundles. Prototypes remain
// checkable HIR, never runtime values. No defaulting or ABI inference.
void infer_types(Unit& unit, const InferenceLimits& limits = {});
}

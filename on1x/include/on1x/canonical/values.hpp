#pragma once
#include <on1x/ast.hpp>
#include <set>
namespace on1x::canonical {
bool needs_values(const Unit& unit);
void lower_values(Unit& unit);
// Typed region verification includes definite initialization and tag domination.
void verify_values(const Unit& unit);
std::set<std::size_t> exit_targets(const Function& function,Stmt::Kind kind);
}

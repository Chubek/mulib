#pragma once
#include <on1x/ast.hpp>
#include <functional>
#include <map>

namespace on1x::patterns {
using Bindings=std::map<std::string,Type>;
using HiddenRepresentation=std::function<bool(const Type&)>;
Expr literal(const Pattern& pattern);
// Validates concrete patterns and all alternative/binder obligations.
Bindings check(Pattern& pattern,const Type& type,bool root=true,
               const HiddenRepresentation& hidden={});
void require_binding(const Pattern& pattern);
// Finite-constructor matrix analysis; only literal-true guards certify coverage.
void coverage(const Expr& match,const HiddenRepresentation& hidden={});
bool terminates(const std::vector<Stmt>& body);
bool terminates(const Expr& expression);
}

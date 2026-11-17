#pragma once
#include <on1x/ast.hpp>
namespace on1x::csp {
Type send_result();
Type receive_result();
bool contains(const Type&);
bool present(const Unit&);
// Type/shape and affine lifetime verification. Both source and canonical mode
// are checked independently; no serialized proof/capability is accepted.
void verify(const Unit&,bool canonical);
void lower(Unit&);
}

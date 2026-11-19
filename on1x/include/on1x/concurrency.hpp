#pragma once
#include <on1x/ast.hpp>
namespace on1x::csp {
Type send_result();
Type receive_result(Type payload=Type::I64);
Type try_send_result();
Type try_receive_result(Type payload);
Type split_result(const Type& channel);
// Fixed-size, pointer-free values are copied by MuLIB. The diagnostic is empty
// exactly when the payload is admitted; it includes the bounded-size profile.
std::string payload_diagnostic(const Type&);
bool payload_supported(const Type&);
bool contains(const Type&);
bool present(const Unit&);
// Type/shape and affine lifetime verification. Both source and canonical mode
// are checked independently; no serialized proof/capability is accepted.
void verify(const Unit&,bool canonical);
void lower(Unit&);
}

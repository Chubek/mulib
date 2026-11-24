#pragma once
#include <on1x/ast.hpp>
#include <on1x/ffi.hpp>

namespace on1x::ffi {
// HIR keys are not native link names. The descriptor ID binds the exact
// baseline shape and plan, not external review or permission to link.
std::string foreign_hir_name(const HirForeign&);
inline bool is_foreign_hir_name(std::string_view name){return name.starts_with("@ffi_");}
Type surface_type(SurfaceKind);
const Descriptor* verify_foreign_declaration(const Function&,std::string_view expected_target={});
void verify_foreign_reference(const Expr&,const Function&);

// Call-site policy is independent of the emitter and admission. Scalar v2
// parameters are exact values; the retained rich profiles project their
// existing constraints here until verified rich descriptors replace them.
struct ArgumentPolicy {
 enum class Mode { Value, BoundedSlice, ExclusiveOwner } mode=Mode::Value;
 std::size_t max_extent=0;
};
ArgumentPolicy argument_policy(const Function&,std::size_t);
bool argument_type(ArgumentPolicy,const Type& expected,const Type& actual);
bool requires_call_loan(const Function&);

inline Expr::Reference foreign_reference(ForeignMode mode){
 return requires_unsafe(mode)?Expr::Reference::UnsafeForeign:Expr::Reference::ScalarForeign;
}
}

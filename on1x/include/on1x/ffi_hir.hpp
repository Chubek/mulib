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
 // ConsumeOwner uses normal HIR ownership transfer: an owned temporary or
 // an explicit move, never a copied or borrowed place.
 enum class Mode { Value, BoundedSlice, ConsumeOwner, ExclusiveOwner } mode=Mode::Value;
 std::size_t max_extent=0;
};
ArgumentPolicy argument_policy(const Function&,std::size_t);
bool argument_type(ArgumentPolicy,const Type& expected,const Type& actual);
bool requires_call_loan(const Function&);

// Unqualified raw C strings carry no retention, ownership or release fact.
// This predicate is for advisory analysis, not admission.
bool unclassified_string_resource(const Function&, const Type&);
// Closed, body-audited runtime adapters; identifies only the generated
// caller's i64 out cell, never the externally owned IPC Pair/Session payload.
bool ipc_call_scoped_i64_out(const Function&);

inline Expr::Reference foreign_reference(ForeignMode mode){
 return requires_unsafe(mode)?Expr::Reference::UnsafeForeign:Expr::Reference::ScalarForeign;
}
}

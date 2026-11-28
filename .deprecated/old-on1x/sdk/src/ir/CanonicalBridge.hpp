#ifndef ON1X_SDK_CANONICAL_BRIDGE_HPP
#define ON1X_SDK_CANONICAL_BRIDGE_HPP

#include "IRInternal.hpp"

namespace on1x::sdk::ir {

/*
 * Private host bridge. The candidate Unit is cloned before canonical checking,
 * so failure cannot mutate caller state. No equivalent public C constructor
 * exists for CHECKED_HIR or VERIFIED_MIR.
 */
On1xSDK_Status check_hir_candidate(
    const Unit &candidate,
    const On1xSDK_IRIdentity &identity,
    std::string_view provenance,
    On1xSDK_HIRSnapshot **out,
    On1xSDK_IRFailure *failure,
    const On1xSDK_DiagnosticSink *diagnostics = nullptr) noexcept;

On1xSDK_Status admit_mir(
    const mir::Module &candidate,
    const On1xSDK_IRIdentity &identity,
    std::string_view provenance,
    On1xSDK_MIRSnapshot **out,
    On1xSDK_IRFailure *failure,
    const On1xSDK_DiagnosticSink *diagnostics = nullptr) noexcept;

} // namespace on1x::sdk::ir

#endif

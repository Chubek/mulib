/*
 * Built-in canonical semantic services. These are host operations, not
 * registrable extension callbacks.
 */
#ifndef ON1X_SDK_SEMANTICS_H
#define ON1X_SDK_SEMANTICS_H

#include "On1xSDK-HIR.h"
#include "On1xSDK-MIR.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Re-runs immutable canonical HIR invariants. CANDIDATE wire HIR cannot be
 * promoted because HIR-v1 is intentionally an inspection schema.
 */
On1xSDK_Status on1x_sdk_semantics_verify_checked_hir(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

/*
 * Requires CHECKED_HIR created by the authoritative host bridge. Canonical
 * lowering and canonical MIR verification both run before a VERIFIED_MIR
 * snapshot is returned. Unsupported aggregate/resource/effect constructs
 * reject with UNSUPPORTED and are never erased.
 */
On1xSDK_Status on1x_sdk_semantics_lower_checked_hir(
    const On1xSDK_HIRSnapshot *snapshot,
    uint64_t output_revision,
    const On1xSDK_IRLimits *limits,
    On1xSDK_MIRSnapshot **out_mir,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

/*
 * Canonical structural verification. It never upgrades a CANDIDATE snapshot;
 * publication remains a separate host-only operation.
 */
On1xSDK_Status on1x_sdk_semantics_verify_mir(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

#ifdef __cplusplus
}
#endif

#endif

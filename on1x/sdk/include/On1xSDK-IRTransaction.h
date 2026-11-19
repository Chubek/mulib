/* Thread-confined, rollback-capable candidate MIR transactions. */
#ifndef ON1X_SDK_IR_TRANSACTION_H
#define ON1X_SDK_IR_TRANSACTION_H

#include "On1xSDK-MIR.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_MIRTransaction On1xSDK_MIRTransaction;

/*
 * expected_source must exactly match source. A transaction retains source,
 * owns a private module copy, and is confined to the creating thread.
 */
On1xSDK_Status on1x_sdk_mir_transaction_begin(
    const On1xSDK_MIRSnapshot *source,
    const On1xSDK_IRIdentity *expected_source,
    On1xSDK_MIRTransaction **out_transaction,
    On1xSDK_IRFailure *out_failure);

On1xSDK_Status on1x_sdk_mir_transaction_set_immediate(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *instruction,
    int64_t immediate);
On1xSDK_Status on1x_sdk_mir_transaction_set_operand(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *instruction,
    size_t operand_index,
    uint64_t value);
On1xSDK_Status on1x_sdk_mir_transaction_rename_function(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *function,
    On1xSDK_IRStringView name);

/*
 * Verification is canonical MIR verification and does not commit or certify
 * equivalence. Failure leaves both source and transaction unchanged.
 */
On1xSDK_Status on1x_sdk_mir_transaction_verify(
    const On1xSDK_MIRTransaction *transaction,
    const On1xSDK_IRLimits *limits,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

/*
 * Commit requires the exact source identity again and a strictly newer,
 * nonzero revision. It verifies a copy, then atomically returns a CANDIDATE
 * snapshot. The transaction is consumed only on success. It never publishes or
 * claims behavioral equivalence.
 */
On1xSDK_Status on1x_sdk_mir_transaction_commit(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_IRIdentity *expected_source,
    uint64_t new_revision,
    On1xSDK_MIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

/* Explicit rollback. Safe on NULL; no partial revision is observable. */
void on1x_sdk_mir_transaction_destroy(On1xSDK_MIRTransaction *transaction);

#ifdef __cplusplus
}
#endif

#endif

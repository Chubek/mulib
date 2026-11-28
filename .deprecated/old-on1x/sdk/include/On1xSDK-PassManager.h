/* Transactional built-in pass pipelines over an observed verified MIR revision. */
#ifndef ON1X_SDK_PASS_MANAGER_H
#define ON1X_SDK_PASS_MANAGER_H

#include "On1xSDK-AnalysisManager.h"
#include "On1xSDK-Pipeline.h"
#include "On1xSDK-PassInstrumentation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_PassResult On1xSDK_PassResult;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_AnalysisKey source;
    const On1xSDK_Pipeline *pipeline; /* Borrowed for this call. */
    /* Effective pass work is min(work_limit, control.work_limit). */
    uint64_t work_limit;
    uint64_t rewrite_limit;
    uint64_t verifier_node_limit;
    uint64_t verifier_work_limit;
    /*
     * result_limit must be zero because pass results have specialized limits.
     * Callbacks are synchronous and may not mutate or dispose this manager.
     */
    On1xSDK_AnalysisControl control;
    On1xSDK_PassInstrumentationFn instrument;
    void *instrument_userdata;
} On1xSDK_PassRun;

/*
 * A non-OK result publishes no candidate and does not alter the observed
 * source. Success returns borrowed candidate bytes; only the host may verify
 * and publish them as a new revision. The result and bytes have the analysis
 * manager's borrowed-result lifetime and are invalidated by its next mutation.
 */
On1xSDK_Status on1x_sdk_pass_manager_run(
    On1xSDK_AnalysisManager *manager, const On1xSDK_PassRun *run,
    const On1xSDK_PassResult **out_result);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_AnalysisKey source;
    uint64_t deterministic_work;
    uint64_t attempted_rewrites;
    uint64_t event_count;
    uint32_t changed;
    uint32_t complete;
} On1xSDK_PassResultInfo;

On1xSDK_Status on1x_sdk_pass_result_info(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_PassResult *result,
    On1xSDK_PassResultInfo *out_info);
On1xSDK_Status on1x_sdk_pass_result_bytes(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_PassResult *result,
    const uint8_t **out_bytes, size_t *out_size);
size_t on1x_sdk_pass_result_event_count(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_PassResult *result);
On1xSDK_Status on1x_sdk_pass_result_event_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_PassResult *result, size_t index,
    On1xSDK_PassEvent *out_event);

/*
 * Structural/preservation validation only. It is not an equivalence proof and
 * does not publish candidate_bytes.
 */
On1xSDK_Status on1x_sdk_pass_verify_transition(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisKey *source,
    On1xSDK_PassKind pass,
    const void *candidate_bytes, size_t candidate_size,
    uint64_t verifier_node_limit, uint64_t verifier_work_limit);

#ifdef __cplusplus
}
#endif

#endif

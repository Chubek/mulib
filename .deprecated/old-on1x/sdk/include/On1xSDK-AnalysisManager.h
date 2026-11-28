/*
 * Revision-aware caches over host-created VERIFIED_MIR Program objects.
 * This API never publishes an IR revision.
 */
#ifndef ON1X_SDK_ANALYSIS_MANAGER_H
#define ON1X_SDK_ANALYSIS_MANAGER_H

#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Query.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_ANALYSIS_MAX_SOURCES 64u
#define ON1X_SDK_ANALYSIS_MAX_CACHE_ENTRIES 128u

typedef struct On1xSDK_AnalysisManager On1xSDK_AnalysisManager;
typedef struct On1xSDK_AnalysisResult On1xSDK_AnalysisResult;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t owner_token;
    uint32_t max_sources;
    uint32_t max_cache_entries;
} On1xSDK_AnalysisManagerConfig;

/*
 * The caller owns suitably aligned storage. Internal allocations do not cross
 * the ABI and are released by dispose. This avoids selecting an allocator
 * outside the owning host's allocator domain. Managers are thread-confined.
 */
size_t on1x_sdk_analysis_manager_storage_size(void);
size_t on1x_sdk_analysis_manager_storage_alignment(void);
On1xSDK_Status on1x_sdk_analysis_manager_initialize(
    void *storage, size_t storage_size,
    const On1xSDK_AnalysisManagerConfig *config,
    On1xSDK_AnalysisManager **out_manager);
On1xSDK_Status on1x_sdk_analysis_manager_dispose(
    On1xSDK_AnalysisManager *manager);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_AnalysisKey key;
    const On1xSDK_Program *program; /* Borrowed for this call only. */
} On1xSDK_ObservedMIR;

/*
 * Called by the host only after canonical MIR verification/publication.
 * The manager independently decodes, verifies, and canonicalizes the bytes.
 * A newer revision atomically replaces the observed source and invalidates
 * affected cache entries. Failure leaves the previous observation unchanged.
 */
On1xSDK_Status on1x_sdk_analysis_manager_observe(
    On1xSDK_AnalysisManager *manager, const On1xSDK_ObservedMIR *source);

/*
 * Returns a borrowed immutable result. It remains valid only until the next
 * mutating call on manager (observe, an uncached query, pass run, or dispose).
 * A control callback may read current immutable results but may not re-enter a
 * mutating manager operation; such an attempt returns ON1X_SDK_BUSY.
 */
On1xSDK_Status on1x_sdk_analysis_manager_query(
    On1xSDK_AnalysisManager *manager, const On1xSDK_Query *query,
    const On1xSDK_AnalysisResult **out_result);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_AnalysisKey source;
    On1xSDK_AnalysisKind kind;
    uint32_t analysis_version;
    uint64_t deterministic_work;
    uint64_t record_count;
    uint64_t cache_generation;
    uint32_t cache_hit;
    On1xSDK_AnalysisBackend backend;
} On1xSDK_AnalysisResultInfo;

On1xSDK_Status on1x_sdk_analysis_result_info(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result,
    On1xSDK_AnalysisResultInfo *out_info);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t generation;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t observed_sources;
    uint32_t cache_entries;
} On1xSDK_AnalysisManagerStats;

On1xSDK_Status on1x_sdk_analysis_manager_stats(
    const On1xSDK_AnalysisManager *manager,
    On1xSDK_AnalysisManagerStats *out_stats);

#ifdef __cplusplus
}
#endif

#endif

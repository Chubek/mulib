/* ABI-2 bounded query descriptors shared by MIR analyses and pass control. */
#ifndef ON1X_SDK_QUERY_H
#define ON1X_SDK_QUERY_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_ANALYSIS_ABI_VERSION 2u
#define ON1X_SDK_ANALYSIS_VERSION 1u
#define ON1X_SDK_ANALYSIS_TARGET_MAX 256u
#define ON1X_SDK_ANALYSIS_MAX_RESULTS 262144u
#define ON1X_SDK_ANALYSIS_MAX_WORK UINT64_C(67108864)

/* Source-compatible analysis names for the shared ABI-2 status contract. */
#define ON1X_SDK_ANALYSIS_CANCELLED ON1X_SDK_CANCELLED
#define ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED ON1X_SDK_DEADLINE_EXCEEDED
#define ON1X_SDK_ANALYSIS_STALE_REVISION ON1X_SDK_STALE
#define ON1X_SDK_ANALYSIS_REVISION_CONFLICT ON1X_SDK_CONFLICT
#define ON1X_SDK_ANALYSIS_NOT_FOUND ON1X_SDK_NOT_FOUND
#define ON1X_SDK_ANALYSIS_INVALID_STATE ON1X_SDK_INVALID_STATE
#define ON1X_SDK_ANALYSIS_VERIFICATION_FAILED ON1X_SDK_VERIFICATION_FAILED

typedef struct {
    const char *data;
    size_t size;
} On1xSDK_AnalysisStringView;

typedef struct {
    uint64_t high;
    uint64_t low;
} On1xSDK_RepresentationIdentity;

/*
 * A key is copied for a call only. target is length-aware printable non-space
 * ASCII. owner_token, identity, and revision must be nonzero.
 */
typedef struct {
    uint64_t owner_token;
    On1xSDK_RepresentationIdentity identity;
    uint64_t revision;
    On1xSDK_AnalysisStringView target;
    uint64_t options_key;
} On1xSDK_AnalysisKey;

typedef uint32_t On1xSDK_AnalysisKind;
enum {
    ON1X_SDK_ANALYSIS_CFG = 1,
    ON1X_SDK_ANALYSIS_DOMINATORS = 2,
    ON1X_SDK_ANALYSIS_LOOPS = 3,
    ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF = 4,
    ON1X_SDK_ANALYSIS_EFFECTS = 5,
    /* Scalar MIR has no address values; general alias queries reject. */
    ON1X_SDK_ANALYSIS_ALIAS = 6
};

typedef uint32_t On1xSDK_AnalysisBackend;
enum {
    ON1X_SDK_ANALYSIS_BACKEND_CANONICAL = 1,
    ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE = 2,
    ON1X_SDK_ANALYSIS_BACKEND_WETSTONE = 3
};

/*
 * Return OK to continue, ANALYSIS_CANCELLED, or
 * ANALYSIS_DEADLINE_EXCEEDED. Any other value is a callback failure.
 * The callback and userdata are borrowed synchronously. It may not mutate or
 * dispose the manager that is executing the query.
 */
typedef uint32_t (*On1xSDK_AnalysisControlFn)(void *userdata,
                                             uint64_t deterministic_work);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t work_limit;
    uint64_t result_limit;
    On1xSDK_AnalysisControlFn poll;
    void *userdata;
} On1xSDK_AnalysisControl;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_AnalysisKey source;
    On1xSDK_AnalysisKind kind;
    uint32_t analysis_version;
    On1xSDK_AnalysisBackend backend;
    uint32_t reserved;
    On1xSDK_AnalysisControl control;
} On1xSDK_Query;

#ifdef __cplusplus
}
#endif

#endif

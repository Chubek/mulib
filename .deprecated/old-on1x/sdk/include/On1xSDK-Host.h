#ifndef ON1X_SDK_HOST_H
#define ON1X_SDK_HOST_H

#include "On1xSDK-Allocator.h"
#include "On1xSDK-Budget.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-Capability.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Handle.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t On1xSDK_HostFeatures;
enum {
    ON1X_SDK_HOST_FEATURE_ALLOCATOR = UINT64_C(1) << 0,
    ON1X_SDK_HOST_FEATURE_DIAGNOSTICS = UINT64_C(1) << 1,
    ON1X_SDK_HOST_FEATURE_CANCELLATION = UINT64_C(1) << 2,
    ON1X_SDK_HOST_FEATURE_BUDGET = UINT64_C(1) << 3,
    ON1X_SDK_HOST_FEATURE_HANDLES = UINT64_C(1) << 4,
    ON1X_SDK_HOST_FEATURE_CANDIDATES = UINT64_C(1) << 5,
    ON1X_SDK_HOST_FEATURE_CAPABILITIES = UINT64_C(1) << 6,
    ON1X_SDK_HOST_FEATURE_LOGGING = UINT64_C(1) << 7
};

typedef On1xSDK_LogFn On1xSDK_HostLogFn;
typedef On1xSDK_Status (*On1xSDK_HostDiagnosticFn)(
    void *context_token, const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
);
typedef On1xSDK_Status (*On1xSDK_HostPollFn)(
    void *context_token, const On1xSDK_Cancellation *cancellation
);
typedef On1xSDK_Status (*On1xSDK_HostConsumeFn)(
    void *context_token, On1xSDK_Budget *budget,
    On1xSDK_BudgetKind kind, uint64_t amount
);
typedef On1xSDK_Status (*On1xSDK_HostHandleFn)(
    void *context_token, const On1xSDK_HandleRef *reference
);
typedef int (*On1xSDK_HostCapabilityFn)(
    void *context_token, On1xSDK_StringView name, uint32_t minimum_revision
);
typedef On1xSDK_Status (*On1xSDK_HostSubmitFn)(
    void *context_token, On1xSDK_StringView schema, uint32_t schema_version,
    On1xSDK_ByteView candidate, uint64_t source_revision,
    On1xSDK_Handle *candidate_handle
);

struct On1xSDK_Host {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t feature_bits;
    uint64_t owner_context;
    void *context_token;
    const On1xSDK_Allocator *allocator;
    On1xSDK_HostLogFn log;
    On1xSDK_HostDiagnosticFn emit_diagnostic;
    On1xSDK_HostPollFn poll_cancellation;
    On1xSDK_HostConsumeFn consume_budget;
    On1xSDK_HostHandleFn retain_handle;
    On1xSDK_HostHandleFn release_handle;
    On1xSDK_HostCapabilityFn capability_granted;
    On1xSDK_HostSubmitFn submit_candidate;
};

/*
 * Returns a borrowed immutable table. A caller must still check struct_size
 * before using each trailing field.
 */
On1xSDK_Status on1x_sdk_host_negotiate(
    const On1xSDK_Host *offered,
    uint32_t requested_abi,
    uint32_t required_size,
    const On1xSDK_Host **selected
);

#ifdef __cplusplus
}
#endif

#endif

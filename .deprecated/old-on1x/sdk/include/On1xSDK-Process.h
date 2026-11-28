/* Host-authorized argv process requests.  This API never invokes a shell. */
#ifndef ON1X_SDK_PROCESS_H
#define ON1X_SDK_PROCESS_H

#include "On1xSDK-Project.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PROCESS_MAX_ARGUMENTS 256u
#define ON1X_SDK_PROCESS_MAX_ENVIRONMENT 128u
#define ON1X_SDK_PROCESS_MAX_ARGUMENT_BYTES (64u * 1024u)

typedef struct On1xSDK_ProcessRequest {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString executable; /* Explicit absolute path; never name-resolved. */
    On1xSDK_ProjectString working_directory; /* Explicit absolute path. */
    size_t argument_count;
    const On1xSDK_ProjectString *arguments; /* argv[0] is executable by convention. */
    size_t environment_count;
    const On1xSDK_ProjectString *environment; /* NAME=value, exact replacement environment. */
    uint64_t work_units;            /* Deterministic budget; zero means host policy. */
    uint64_t output_bytes;          /* Combined bounded output; zero means host policy. */
    uint64_t deadline_milliseconds; /* Wall-clock deadline; distinct from work_units. */
} On1xSDK_ProcessRequest;

typedef struct On1xSDK_ProcessResult {
    uint32_t abi_version;
    uint32_t struct_size;
    int32_t exit_code;     /* Nonnegative normal exit; -1 when not normally exited. */
    int32_t signal_number; /* Zero unless terminated by a signal. */
    uint64_t output_bytes;
} On1xSDK_ProcessResult;

/* Compatibility name for the shared, thread-safe cancellation object. */
typedef On1xSDK_Cancellation On1xSDK_ProcessCancellation;

typedef On1xSDK_Status (*On1xSDK_ProcessAuthorizeFn)(
    void *context, const On1xSDK_ProcessRequest *request,
    uint64_t *out_authorization);
typedef On1xSDK_Status (*On1xSDK_ProcessExecuteFn)(
    void *context, uint64_t authorization,
    const On1xSDK_ProcessRequest *request,
    const On1xSDK_ProcessCancellation *cancellation,
    On1xSDK_ProcessResult *result);
typedef On1xSDK_Status (*On1xSDK_ProcessFinishFn)(
    void *context, uint64_t authorization, On1xSDK_Status operation_status);

typedef struct On1xSDK_ProcessHost {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    On1xSDK_ProcessAuthorizeFn authorize;
    On1xSDK_ProcessExecuteFn execute;
    On1xSDK_ProcessFinishFn finish;
} On1xSDK_ProcessHost;

ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_process_request_validate(
    const On1xSDK_ProcessRequest *request);

/*
 * Calls authorize, then execute, then finish exactly once for every successful
 * authorization.  Arguments are passed as length-aware array elements without
 * interpolation.  A zero authorization token is rejected.  Unknown callback
 * statuses become ERROR.  Result is cleared on failure.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_process_dispatch(
    const On1xSDK_ProcessHost *host,
    const On1xSDK_ProcessRequest *request,
    const On1xSDK_ProcessCancellation *cancellation,
    On1xSDK_ProcessResult *result);

#ifdef __cplusplus
}
#endif

#endif

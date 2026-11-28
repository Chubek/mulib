/*
 * Host-approved scalar bridge for the On1x-native SDK reference extension.
 *
 * This is a deliberately narrow in-process test/reference profile, not the
 * general SDK ABI 2 host table and not foreign-safety certification.  Only
 * fixed-width scalars cross the generated-On1x boundary.  Host-only descriptor
 * APIs below use ordinary C pointers whose lifetimes are stated per call.
 */
#ifndef ON1X_SDK_NATIVE_TRUSTED_BRIDGE_H
#define ON1X_SDK_NATIVE_TRUSTED_BRIDGE_H

#include "On1xSDK-ABI.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-Span.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_NATIVE_BRIDGE_ABI 1u
#define ON1X_SDK_NATIVE_SDK_ABI ON1X_SDK_ABI_VERSION_V2
#define ON1X_SDK_NATIVE_BRIDGE_TABLE_SIZE 16u
#define ON1X_SDK_NATIVE_METADATA_SCHEMA 1u
#define ON1X_SDK_NATIVE_SOURCE_SCHEMA 1u
#define ON1X_SDK_NATIVE_CANDIDATE_SCHEMA 1u

#define ON1X_SDK_NATIVE_SOURCE_MAX 64u
#define ON1X_SDK_NATIVE_CANDIDATE_MAX 16u
#define ON1X_SDK_NATIVE_DIAGNOSTIC_MAX 8u
#define ON1X_SDK_NATIVE_RELATED_MAX 1u

#define ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT UINT64_C(1)
#define ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT UINT64_C(2)

#define ON1X_SDK_NATIVE_PROFILE_TRUSTED_STATIC 1u
#define ON1X_SDK_NATIVE_EXTENSION_ID 8u
#define ON1X_SDK_NATIVE_EXTENSION_VERSION_MAJOR 1u
#define ON1X_SDK_NATIVE_EXTENSION_VERSION_MINOR 0u

#define ON1X_SDK_NATIVE_DIAGNOSTIC_CODE 1001u
#define ON1X_SDK_NATIVE_DIAGNOSTIC_MESSAGE 1u
#define ON1X_SDK_NATIVE_DIAGNOSTIC_SEVERITY_WARNING 2u

typedef On1xSDK_Status On1xSDKNative_Status;
#define ON1X_SDK_NATIVE_OK ON1X_SDK_OK
#define ON1X_SDK_NATIVE_ERROR ON1X_SDK_ERROR
#define ON1X_SDK_NATIVE_UNSUPPORTED ON1X_SDK_UNSUPPORTED
#define ON1X_SDK_NATIVE_LIMIT ON1X_SDK_LIMIT
#define ON1X_SDK_NATIVE_INVALID_ARGUMENT ON1X_SDK_INVALID_ARGUMENT
#define ON1X_SDK_NATIVE_ABI_MISMATCH ON1X_SDK_ABI_MISMATCH
#define ON1X_SDK_NATIVE_DUPLICATE ON1X_SDK_DUPLICATE
#define ON1X_SDK_NATIVE_BUSY ON1X_SDK_BUSY
#define ON1X_SDK_NATIVE_CANCELLED ON1X_SDK_CANCELLED
#define ON1X_SDK_NATIVE_DEADLINE_EXPIRED ON1X_SDK_DEADLINE_EXCEEDED
#define ON1X_SDK_NATIVE_STALE ON1X_SDK_STALE
#define ON1X_SDK_NATIVE_CONFLICT ON1X_SDK_CONFLICT
#define ON1X_SDK_NATIVE_NOT_FOUND ON1X_SDK_NOT_FOUND
#define ON1X_SDK_NATIVE_INVALID_STATE ON1X_SDK_INVALID_STATE
#define ON1X_SDK_NATIVE_VERIFICATION_FAILED ON1X_SDK_VERIFICATION_FAILED
#define ON1X_SDK_NATIVE_CAPABILITY_DENIED ON1X_SDK_CAPABILITY_DENIED
#define ON1X_SDK_NATIVE_IO ON1X_SDK_IO_ERROR
#define ON1X_SDK_NATIVE_PROTOCOL_ERROR ON1X_SDK_PROTOCOL_ERROR
#define ON1X_SDK_NATIVE_CALLBACK_FAILED ON1X_SDK_CALLBACK_FAILED
#define ON1X_SDK_NATIVE_UNSATISFIABLE ON1X_SDK_UNSATISFIABLE

typedef On1xSDK_Handle On1xSDKNative_Handle;
typedef On1xSDK_ABIHeader On1xSDKNative_ABIHeader;

/*
 * Host-only creation descriptor. source_data is borrowed only for the call and
 * is deep-copied into bounded host storage. UINT32_MAX disables cancellation
 * and diagnostic failure injection. Unknown trailing fields are ignored.
 */
typedef struct On1xSDKNative_ContextConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t sdk_abi_version;
    uint32_t bridge_abi_version;
    uint32_t bridge_table_size;
    uint32_t source_schema;
    uint32_t candidate_schema;
    uint32_t candidate_limit;
    uint64_t source_revision;
    uint64_t granted_capabilities;
    const uint8_t *source_data;
    size_t source_size;
    uint32_t work_limit;
    uint32_t cancel_after_polls;
    uint32_t fail_diagnostic_after;
    uint32_t fail_initialize;
    uint32_t fail_publish;
} On1xSDKNative_ContextConfig;

typedef On1xSDK_SourceSpan On1xSDKNative_Span;

/*
 * Host-owned copied diagnostic view. Text arrays are not NUL contracts; their
 * explicit lengths are authoritative.
 */
typedef struct On1xSDKNative_Diagnostic {
    uint32_t machine_code_id;
    uint32_t severity;
    uint32_t message_id;
    uint32_t machine_code_length;
    uint32_t message_length;
    char machine_code[40];
    char message[64];
    On1xSDKNative_Span primary;
    uint32_t related_count;
    On1xSDKNative_Span related[ON1X_SDK_NATIVE_RELATED_MAX];
} On1xSDKNative_Diagnostic;

typedef On1xSDKNative_Status (*On1xSDKNative_InvokeFn)(
    On1xSDKNative_Handle context,
    uint32_t operation
);

/* Declarative discovery reads this descriptor and executes no extension code. */
typedef struct On1xSDKNative_Module {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t sdk_abi_version;
    uint32_t bridge_abi_version;
    uint32_t source_schema;
    uint32_t candidate_schema;
    uint64_t requested_capabilities;
    const char *name;
    size_t name_length;
    On1xSDKNative_InvokeFn invoke;
} On1xSDKNative_Module;

enum {
    ON1X_SDK_NATIVE_OP_INITIALIZE = 1u,
    ON1X_SDK_NATIVE_OP_ANALYZE = 2u,
    ON1X_SDK_NATIVE_OP_SHUTDOWN = 3u,
    ON1X_SDK_NATIVE_OP_HANDLE_PROBE = 4u,
    ON1X_SDK_NATIVE_OP_V1_STATUS_PROBE = 5u,
    ON1X_SDK_NATIVE_OP_DIAGNOSTIC_PROBE = 6u
};

extern const On1xSDKNative_Module on1x_sdk_native_reference_module;

On1xSDKNative_Status on1x_sdk_native_context_create(
    const On1xSDKNative_ContextConfig *config,
    On1xSDKNative_Handle *out_context
);
On1xSDKNative_Status on1x_sdk_native_context_destroy(
    On1xSDKNative_Handle context
);
On1xSDKNative_Status on1x_sdk_native_module_validate(
    const On1xSDKNative_Module *module
);
On1xSDKNative_Status on1x_sdk_native_context_diagnostic_count(
    On1xSDKNative_Handle context,
    size_t *out_count
);
On1xSDKNative_Status on1x_sdk_native_context_diagnostic(
    On1xSDKNative_Handle context,
    size_t index,
    On1xSDKNative_Diagnostic *out_diagnostic
);
/* The returned candidate view is borrowed until context mutation/destruction. */
On1xSDKNative_Status on1x_sdk_native_context_candidate(
    On1xSDKNative_Handle context,
    const uint8_t **out_bytes,
    size_t *out_size
);
On1xSDKNative_Status on1x_sdk_native_context_live_objects(
    On1xSDKNative_Handle context,
    size_t *out_count
);
On1xSDKNative_Status on1x_sdk_native_context_metadata_state(
    On1xSDKNative_Handle context,
    uint32_t *out_state
);

/*
 * The functions below are the exact scalar leaves declared in
 * stdlib/sdk/_Trusted.on1x. A valid context ID owns every object ID.
 */
uint32_t basis_sdk_bridge_negotiate(
    uint64_t context,
    uint32_t sdk_abi,
    uint32_t bridge_abi,
    uint32_t bridge_size
);
uint32_t basis_sdk_bridge_metadata_register(
    uint64_t context,
    uint32_t metadata_schema,
    uint32_t extension_id,
    uint32_t version_major,
    uint32_t version_minor,
    uint64_t requested_capabilities,
    uint32_t execution_profile
);
uint32_t basis_sdk_bridge_metadata_rollback(uint64_t context);
uint32_t basis_sdk_bridge_lifecycle(uint64_t context, uint32_t event);
uint32_t basis_sdk_bridge_require_active(uint64_t context);
uint32_t basis_sdk_bridge_capability_check(
    uint64_t context,
    uint32_t capability
);
uint32_t basis_sdk_bridge_cancel_check(uint64_t context);
uint32_t basis_sdk_bridge_work_consume(uint64_t context, uint32_t units);

uint64_t basis_sdk_bridge_snapshot_open(
    uint64_t context,
    uint32_t source_schema,
    uint64_t maximum_bytes
);
uint32_t basis_sdk_bridge_snapshot_check(
    uint64_t context,
    uint64_t snapshot
);
uint32_t basis_sdk_bridge_snapshot_close(
    uint64_t context,
    uint64_t snapshot
);
uint64_t basis_sdk_bridge_snapshot_revision(
    uint64_t context,
    uint64_t snapshot
);
uint64_t basis_sdk_bridge_snapshot_length(
    uint64_t context,
    uint64_t snapshot
);
uint32_t basis_sdk_bridge_snapshot_schema(
    uint64_t context,
    uint64_t snapshot
);
/* Values 0..255 are bytes; values 256+status are failures. */
uint32_t basis_sdk_bridge_snapshot_read(
    uint64_t context,
    uint64_t snapshot,
    uint64_t revision,
    uint64_t index
);

uint32_t basis_sdk_bridge_diagnostic_emit(
    uint64_t context,
    uint64_t snapshot,
    uint64_t revision,
    uint32_t machine_code,
    uint32_t severity,
    uint32_t message,
    uint32_t message_length,
    uint64_t span_start,
    uint64_t span_end,
    uint32_t related_count,
    uint64_t related_start,
    uint64_t related_end
);

uint64_t basis_sdk_bridge_candidate_open(
    uint64_t context,
    uint32_t candidate_schema,
    uint64_t source_revision,
    uint32_t maximum_bytes
);
uint32_t basis_sdk_bridge_candidate_append(
    uint64_t context,
    uint64_t candidate,
    uint8_t value
);
uint32_t basis_sdk_bridge_candidate_commit(
    uint64_t context,
    uint64_t candidate,
    uint32_t candidate_schema,
    uint64_t source_revision
);
uint32_t basis_sdk_bridge_candidate_abort(
    uint64_t context,
    uint64_t candidate
);

#ifdef __cplusplus
}
#endif

#endif

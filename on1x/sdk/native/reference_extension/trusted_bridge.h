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

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_NATIVE_BRIDGE_ABI 1u
#define ON1X_SDK_NATIVE_SDK_ABI 2u
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

typedef uint32_t On1xSDKNative_Status;
enum {
    ON1X_SDK_NATIVE_OK = 0u,
    ON1X_SDK_NATIVE_ERROR = 1u,
    ON1X_SDK_NATIVE_UNSUPPORTED = 2u,
    ON1X_SDK_NATIVE_LIMIT = 3u,
    ON1X_SDK_NATIVE_INVALID_ARGUMENT = 4u,
    ON1X_SDK_NATIVE_ABI_MISMATCH = 5u,
    ON1X_SDK_NATIVE_DUPLICATE = 6u,
    ON1X_SDK_NATIVE_BUSY = 7u,
    ON1X_SDK_NATIVE_CANCELLED = 8u,
    ON1X_SDK_NATIVE_DEADLINE_EXPIRED = 9u,
    ON1X_SDK_NATIVE_STALE = 10u,
    ON1X_SDK_NATIVE_CONFLICT = 11u,
    ON1X_SDK_NATIVE_NOT_FOUND = 12u,
    ON1X_SDK_NATIVE_INVALID_STATE = 13u,
    ON1X_SDK_NATIVE_VERIFICATION_FAILED = 14u,
    ON1X_SDK_NATIVE_CAPABILITY_DENIED = 15u,
    ON1X_SDK_NATIVE_IO = 16u
};

typedef uint64_t On1xSDKNative_Handle;

typedef struct On1xSDKNative_ABIHeader {
    uint32_t abi_version;
    uint32_t struct_size;
} On1xSDKNative_ABIHeader;

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

typedef struct On1xSDKNative_Span {
    uint64_t revision;
    uint64_t start;
    uint64_t end;
} On1xSDKNative_Span;

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

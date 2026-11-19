#ifndef ON1X_SDK_HANDLE_H
#define ON1X_SDK_HANDLE_H

#include "On1xSDK-ABI.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wire encoding: high 32 bits are a nonzero generation, low 32 bits are a
 * bounded nonzero slot number. Zero is always invalid.
 */
typedef uint64_t On1xSDK_Handle;

typedef uint32_t On1xSDK_HandleKind;
enum {
    ON1X_SDK_HANDLE_KIND_NONE = 0,
    ON1X_SDK_HANDLE_KIND_CONTEXT = 1,
    ON1X_SDK_HANDLE_KIND_REGISTRY = 2,
    ON1X_SDK_HANDLE_KIND_SNAPSHOT = 3,
    ON1X_SDK_HANDLE_KIND_SHARED_MEMORY = 4,
    ON1X_SDK_HANDLE_KIND_PLUGIN = 5,
    ON1X_SDK_HANDLE_KIND_REQUEST = 6,
    ON1X_SDK_HANDLE_KIND_CANDIDATE = 7
};
#define ON1X_SDK_HANDLE_KIND_HOST_PRIVATE_MIN UINT32_C(0x80000000)

typedef uint32_t On1xSDK_HandleState;
enum {
    ON1X_SDK_HANDLE_STATE_CREATED = 1u << 0,
    ON1X_SDK_HANDLE_STATE_ACTIVE = 1u << 1,
    ON1X_SDK_HANDLE_STATE_FROZEN = 1u << 2,
    ON1X_SDK_HANDLE_STATE_CLOSING = 1u << 3,
    ON1X_SDK_HANDLE_STATE_CLOSED = 1u << 4,
    ON1X_SDK_HANDLE_STATE_FAILED = 1u << 5
};

/* Pointer-free reference carried by protocol/schema records. */
typedef struct On1xSDK_HandleRef {
    uint64_t owner_context;
    On1xSDK_Handle handle;
    uint32_t kind;
    uint32_t state;
} On1xSDK_HandleRef;

typedef void (*On1xSDK_HandleDropFn)(void *userdata, void *host_object);

typedef struct On1xSDK_HandleDescriptor {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t kind;
    uint32_t initial_state;
    void *host_object;
    On1xSDK_HandleDropFn drop;
    void *drop_userdata;
} On1xSDK_HandleDescriptor;

On1xSDK_Status on1x_sdk_handle_insert(
    On1xSDK_Context *context,
    const On1xSDK_HandleDescriptor *descriptor,
    On1xSDK_HandleRef *out
);
On1xSDK_Status on1x_sdk_handle_retain(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference
);
On1xSDK_Status on1x_sdk_handle_release(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference
);
On1xSDK_Status on1x_sdk_handle_resolve(
    On1xSDK_Context *context,
    const On1xSDK_HandleRef *reference,
    uint32_t expected_kind,
    uint32_t allowed_states,
    void **borrowed_host_object
);
On1xSDK_Status on1x_sdk_handle_transition(
    On1xSDK_Context *context,
    On1xSDK_HandleRef *reference,
    uint32_t expected_states,
    uint32_t new_state
);
uint32_t on1x_sdk_handle_slot(On1xSDK_Handle handle);
uint32_t on1x_sdk_handle_generation(On1xSDK_Handle handle);

#ifdef __cplusplus
}
#endif

#endif

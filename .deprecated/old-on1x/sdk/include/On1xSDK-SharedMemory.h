#ifndef ON1X_SDK_SHARED_MEMORY_H
#define ON1X_SDK_SHARED_MEMORY_H

#include "On1xSDK-Context.h"
#include "On1xSDK-Handle.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_SHARED_MEMORY_SCHEMA_MAJOR 1u
#define ON1X_SDK_SHARED_MEMORY_SCHEMA_MINOR 0u
#define ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE 88u
#define ON1X_SDK_SHARED_MEMORY_MAX_BYTES (64u * 1024u * 1024u)

typedef uint32_t On1xSDK_SharedMemoryFlags;
enum {
    ON1X_SDK_SHARED_MEMORY_FROZEN = 1u << 0,
    ON1X_SDK_SHARED_MEMORY_STAGING = 1u << 1,
    ON1X_SDK_SHARED_MEMORY_READ_ONLY_IMPORT = 1u << 2
};

/* Pointer-free transfer record. platform_id is never interpreted as a pointer. */
typedef struct On1xSDK_SharedMemoryDescriptor {
    uint16_t schema_major;
    uint16_t schema_minor;
    uint32_t flags;
    uint64_t owner_context;
    On1xSDK_Handle handle;
    uint32_t kind;
    uint32_t state;
    int32_t platform_id;
    uint32_t reserved;
    uint64_t capacity;
    uint64_t size;
    uint8_t digest_sha256[32];
} On1xSDK_SharedMemoryDescriptor;

/*
 * A locally created mapping is confined to its context's handle thread,
 * including its final release. Transfer mappings with an authenticated
 * descriptor rather than sharing this pointer across threads or processes.
 */
On1xSDK_Status on1x_sdk_shared_memory_create_staging(
    On1xSDK_Context *context, size_t capacity, On1xSDK_SharedMemory **out
);
On1xSDK_Status on1x_sdk_shared_memory_create_frozen(
    On1xSDK_Context *context,
    On1xSDK_ByteView bytes,
    On1xSDK_SharedMemory **out
);
void on1x_sdk_shared_memory_retain(On1xSDK_SharedMemory *memory);
void on1x_sdk_shared_memory_release(On1xSDK_SharedMemory *memory);
On1xSDK_Status on1x_sdk_shared_memory_mutable_bytes(
    On1xSDK_SharedMemory *memory, uint8_t **data, size_t *capacity
);
On1xSDK_Status on1x_sdk_shared_memory_freeze(
    On1xSDK_SharedMemory *memory, size_t used_size
);
On1xSDK_Status on1x_sdk_shared_memory_bytes(
    const On1xSDK_SharedMemory *memory,
    const uint8_t **data,
    size_t *size
);
On1xSDK_Status on1x_sdk_shared_memory_export(
    const On1xSDK_SharedMemory *memory,
    On1xSDK_SharedMemoryDescriptor *out
);
/* Explicit host-only staging transfer for an authorized worker output plane. */
On1xSDK_Status on1x_sdk_shared_memory_export_staging(
    const On1xSDK_SharedMemory *memory,
    On1xSDK_SharedMemoryDescriptor *out
);
/*
 * The caller must have authorized descriptor through its authenticated control
 * plane. The SDK validates schema, bounds, mapping mode, size and digest.
 */
On1xSDK_Status on1x_sdk_shared_memory_import_authorized(
    const On1xSDK_SharedMemoryDescriptor *descriptor,
    uint32_t required_flags,
    On1xSDK_SharedMemory **out
);
On1xSDK_Status on1x_sdk_shared_memory_descriptor_encode(
    const On1xSDK_SharedMemoryDescriptor *descriptor,
    void *output,
    size_t output_size
);
On1xSDK_Status on1x_sdk_shared_memory_descriptor_decode(
    const void *input,
    size_t input_size,
    On1xSDK_SharedMemoryDescriptor *out
);
On1xSDK_Status on1x_sdk_shared_memory_validate_local(
    On1xSDK_Context *context,
    const On1xSDK_SharedMemoryDescriptor *descriptor
);

#ifdef __cplusplus
}
#endif

#endif

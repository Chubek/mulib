#ifndef ON1X_SDK_ALLOCATOR_H
#define ON1X_SDK_ALLOCATOR_H

#include "On1xSDK-ABI.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_ALLOCATION_BYTES ((size_t)1u << 30)
#define ON1X_SDK_MAX_ALIGNMENT ((size_t)1u << 20)

typedef void *(*On1xSDK_AllocateFn)(
    void *userdata, size_t size, size_t alignment
);
typedef void *(*On1xSDK_ReallocateFn)(
    void *userdata, void *memory, size_t old_size, size_t new_size,
    size_t alignment
);
typedef void (*On1xSDK_ReleaseFn)(
    void *userdata, void *memory, size_t size, size_t alignment
);

typedef struct On1xSDK_Allocator {
    uint32_t abi_version;
    uint32_t struct_size;
    void *userdata;
    On1xSDK_AllocateFn allocate;
    On1xSDK_ReallocateFn reallocate;
    On1xSDK_ReleaseFn release;
} On1xSDK_Allocator;

const On1xSDK_Allocator *on1x_sdk_default_allocator(void);
On1xSDK_Status on1x_sdk_allocator_validate(const On1xSDK_Allocator *allocator);
On1xSDK_Status on1x_sdk_allocate(
    const On1xSDK_Allocator *allocator, size_t size, size_t alignment, void **out
);
On1xSDK_Status on1x_sdk_reallocate(
    const On1xSDK_Allocator *allocator, void *memory, size_t old_size,
    size_t new_size, size_t alignment, void **out
);
On1xSDK_Status on1x_sdk_release(
    const On1xSDK_Allocator *allocator, void *memory, size_t size,
    size_t alignment
);

#ifdef __cplusplus
}
#endif

#endif

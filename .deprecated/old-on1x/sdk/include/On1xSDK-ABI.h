#ifndef ON1X_SDK_ABI_H
#define ON1X_SDK_ABI_H

#include "On1xSDK-Types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_ABIHeader {
    uint32_t abi_version;
    uint32_t struct_size;
} On1xSDK_ABIHeader;

#define ON1X_SDK_ABI_HEADER_INIT_V2(type_) \
    { ON1X_SDK_ABI_VERSION_V2, (uint32_t)sizeof(type_) }

/*
 * True only when a complete field is covered by an advertised structure size.
 * The expression does not dereference ptr_ when it is NULL.
 */
#define ON1X_SDK_STRUCT_HAS(ptr_, type_, field_) \
    ((ptr_) != NULL && (ptr_)->struct_size >= \
        (uint32_t)(offsetof(type_, field_) + sizeof(((type_ *)0)->field_)))

/*
 * Reads only the two-field prefix. required_size is the last required byte,
 * not necessarily sizeof the newest known structure.
 */
On1xSDK_Status on1x_sdk_abi_header_validate(
    const void *descriptor,
    uint32_t required_size,
    uint32_t supported_abi,
    On1xSDK_ABIHeader *copied_header
);

#ifdef __cplusplus
}
#endif

#endif

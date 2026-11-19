/* Validated target data-layout metadata. */
#ifndef ON1X_SDK_DATA_LAYOUT_H
#define ON1X_SDK_DATA_LAYOUT_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_TARGET_ABI_VERSION 2u

typedef uint32_t On1xSDK_ByteOrder;
enum {
    ON1X_SDK_BYTE_ORDER_LITTLE = 1u,
    ON1X_SDK_BYTE_ORDER_BIG = 2u
};

/*
 * Widths and alignments are in bits. Every alignment is a nonzero power of
 * two. The descriptor is copied into an immutable target on successful target
 * creation.
 */
typedef struct On1xSDK_DataLayout {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ByteOrder byte_order;
    uint32_t pointer_bits;
    uint32_t pointer_alignment;
    uint32_t integer_alignment;
    uint32_t stack_alignment;
} On1xSDK_DataLayout;

/* Performs structural validation only; it does not establish target support. */
On1xSDK_Status on1x_sdk_data_layout_validate(const On1xSDK_DataLayout *layout);

#ifdef __cplusplus
}
#endif

#endif

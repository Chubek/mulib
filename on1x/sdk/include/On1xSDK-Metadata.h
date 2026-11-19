#ifndef ON1X_SDK_METADATA_H
#define ON1X_SDK_METADATA_H

#include "On1xSDK-String.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_METADATA_ITEMS 64u

typedef uint32_t On1xSDK_MetadataKind;
enum {
    ON1X_SDK_METADATA_INTEGER = 1,
    ON1X_SDK_METADATA_BOOLEAN = 2,
    ON1X_SDK_METADATA_STRING = 3,
    ON1X_SDK_METADATA_BYTES = 4
};

typedef struct On1xSDK_MetadataValue {
    uint32_t kind;
    uint32_t reserved;
    int64_t integer;
    On1xSDK_StringView string;
    On1xSDK_ByteView bytes;
} On1xSDK_MetadataValue;

typedef struct On1xSDK_MetadataItem {
    On1xSDK_StringView key;
    On1xSDK_MetadataValue value;
} On1xSDK_MetadataItem;

On1xSDK_Status on1x_sdk_metadata_validate(
    const On1xSDK_MetadataItem *items, size_t count
);
const On1xSDK_MetadataItem *on1x_sdk_metadata_find(
    const On1xSDK_MetadataItem *items, size_t count, On1xSDK_StringView key
);

#ifdef __cplusplus
}
#endif

#endif

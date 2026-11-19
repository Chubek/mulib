#ifndef ON1X_SDK_STRING_H
#define ON1X_SDK_STRING_H

#include "On1xSDK-ABI.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_TEXT_BYTES (1024u * 1024u)

typedef struct On1xSDK_StringView {
    const char *data;
    size_t size;
} On1xSDK_StringView;

typedef struct On1xSDK_ByteView {
    const uint8_t *data;
    size_t size;
} On1xSDK_ByteView;

struct On1xSDK_Allocator;

typedef struct On1xSDK_OwnedBytes {
    uint8_t *data;
    size_t size;
    size_t capacity;
    const struct On1xSDK_Allocator *allocator;
    uint64_t owner_context;
    /* Optional retained owner keeping allocator storage alive. */
    On1xSDK_Context *retained_context;
} On1xSDK_OwnedBytes;

On1xSDK_StringView on1x_sdk_string_view(const char *data, size_t size);
On1xSDK_ByteView on1x_sdk_byte_view(const void *data, size_t size);
On1xSDK_Status on1x_sdk_string_validate(On1xSDK_StringView text, size_t limit);
On1xSDK_Status on1x_sdk_string_validate_utf8(On1xSDK_StringView text, size_t limit);
On1xSDK_Status on1x_sdk_identifier_validate(On1xSDK_StringView text);
int on1x_sdk_string_equal(On1xSDK_StringView left, On1xSDK_StringView right);
void on1x_sdk_owned_bytes_release(On1xSDK_OwnedBytes *bytes);

#ifdef __cplusplus
}
#endif

#endif

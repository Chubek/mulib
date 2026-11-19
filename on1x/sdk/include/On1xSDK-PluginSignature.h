#ifndef ON1X_SDK_PLUGIN_SIGNATURE_H
#define ON1X_SDK_PLUGIN_SIGNATURE_H

#include "On1xSDK-String.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_sha256(
    On1xSDK_ByteView bytes, uint8_t digest[32]
);
int on1x_sdk_digest_equal(
    const uint8_t left[32], const uint8_t right[32]
);

#ifdef __cplusplus
}
#endif

#endif

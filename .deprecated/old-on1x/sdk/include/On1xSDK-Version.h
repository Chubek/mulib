#ifndef ON1X_SDK_VERSION_H
#define ON1X_SDK_VERSION_H

#include "On1xSDK-ABI.h"
#include "On1xSDK-String.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_VERSION_MAJOR 2u
#define ON1X_SDK_VERSION_MINOR 0u
#define ON1X_SDK_VERSION_PATCH 0u

typedef struct On1xSDK_Version {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    On1xSDK_StringView prerelease;
} On1xSDK_Version;

On1xSDK_Status on1x_sdk_version(On1xSDK_Version *out);
On1xSDK_Status on1x_sdk_negotiate_abi(
    uint32_t minimum_abi, uint32_t maximum_abi, uint32_t *selected_abi
);

#ifdef __cplusplus
}
#endif

#endif

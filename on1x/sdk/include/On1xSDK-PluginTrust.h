#ifndef ON1X_SDK_PLUGIN_TRUST_H
#define ON1X_SDK_PLUGIN_TRUST_H

#include "On1xSDK-PluginManifest.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_PluginTrustFlags;
enum {
    ON1X_SDK_PLUGIN_TRUST_EXPLICIT_IMAGE = 1u << 0,
    ON1X_SDK_PLUGIN_TRUST_REVIEWED_PROVIDER = 1u << 1,
    ON1X_SDK_PLUGIN_TRUST_ALLOW_IN_PROCESS = 1u << 2,
    ON1X_SDK_PLUGIN_TRUST_ALLOW_ISOLATED = 1u << 3
};

typedef struct On1xSDK_PluginTrustDecision {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t granted_profile;
    uint32_t trust_flags;
} On1xSDK_PluginTrustDecision;

typedef On1xSDK_Status (*On1xSDK_PluginTrustFn)(
    void *userdata,
    const On1xSDK_PluginManifest *manifest,
    On1xSDK_ByteView image_sha256,
    On1xSDK_PluginTrustDecision *decision
);

#ifdef __cplusplus
}
#endif

#endif

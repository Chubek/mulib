#ifndef ON1X_SDK_PLUGIN_WORKER_H
#define ON1X_SDK_PLUGIN_WORKER_H

#include "On1xSDK-PluginHost.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_PluginWorkerFeatures {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t protocol_major;
    uint32_t isolated_native_available;
    uint32_t sealed_shared_memory_available;
    uint32_t wasm_available;
} On1xSDK_PluginWorkerFeatures;

On1xSDK_Status on1x_sdk_plugin_worker_features(
    On1xSDK_PluginWorkerFeatures *out
);

#ifdef __cplusplus
}
#endif

#endif

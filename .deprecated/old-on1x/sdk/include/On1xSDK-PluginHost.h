#ifndef ON1X_SDK_PLUGIN_HOST_H
#define ON1X_SDK_PLUGIN_HOST_H

#include "On1xSDK-Plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_PluginProfileSupport {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t profile;
    uint32_t supported;
    On1xSDK_Status reason;
} On1xSDK_PluginProfileSupport;

On1xSDK_Status on1x_sdk_plugin_profile_support(
    uint32_t profile, On1xSDK_PluginProfileSupport *out
);

#ifdef __cplusplus
}
#endif

#endif

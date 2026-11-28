#ifndef ON1X_SDK_PLUGIN_DEPENDENCY_H
#define ON1X_SDK_PLUGIN_DEPENDENCY_H

#include "On1xSDK-Extension.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_plugin_dependency_validate(
    const On1xSDK_PluginDependency *dependency
);
int on1x_sdk_plugin_dependency_matches(
    const On1xSDK_PluginDependency *dependency,
    On1xSDK_StringView provider_name,
    On1xSDK_StringView provider_version
);

#ifdef __cplusplus
}
#endif

#endif

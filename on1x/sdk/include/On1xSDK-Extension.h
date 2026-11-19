#ifndef ON1X_SDK_EXTENSION_H
#define ON1X_SDK_EXTENSION_H

#include "On1xSDK-Capability.h"
#include "On1xSDK-Host.h"
#include "On1xSDK-Lifecycle.h"
#include "On1xSDK-Registry.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_PluginDependency {
    On1xSDK_StringView name;
    /* Inclusive MAJOR.MINOR.PATCH range. Empty maximum means unbounded. */
    On1xSDK_StringView minimum_version;
    On1xSDK_StringView maximum_version;
} On1xSDK_PluginDependency;

typedef On1xSDK_Status (*On1xSDK_RegisterFnV2)(
    const On1xSDK_Host *host, On1xSDK_Registry *registry, void *userdata
);
typedef On1xSDK_Status (*On1xSDK_InitializeFnV2)(
    const On1xSDK_Host *host, void *userdata
);
typedef On1xSDK_Status (*On1xSDK_ShutdownFnV2)(
    const On1xSDK_Host *host, void *userdata
);

struct On1xSDK_ExtensionV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_StringView name;
    On1xSDK_StringView version;
    On1xSDK_StringView registration_key;
    const On1xSDK_PluginDependency *dependencies;
    size_t dependency_count;
    const On1xSDK_CapabilityRequest *requested_capabilities;
    size_t requested_capability_count;
    On1xSDK_RegisterFnV2 register_all;
    On1xSDK_InitializeFnV2 initialize;
    On1xSDK_ShutdownFnV2 shutdown;
    void *userdata; /* Provider-owned until shutdown completes. */
};

#define ON1X_SDK_EXTENSION_V2_REQUIRED_SIZE \
    ((uint32_t)(offsetof(On1xSDK_ExtensionV2, register_all) + \
                sizeof(((On1xSDK_ExtensionV2 *)0)->register_all)))

On1xSDK_Status on1x_sdk_extension_v2_validate(
    const On1xSDK_ExtensionV2 *extension
);

#ifdef __cplusplus
}
#endif

#endif

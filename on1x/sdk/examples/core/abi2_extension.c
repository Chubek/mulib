#include "On1xSDK-Extension.h"

#include <string.h>

static On1xSDK_Status initialize(
    const On1xSDK_Host *host, void *userdata
) {
    (void)userdata;
    if (!host ||
        !ON1X_SDK_STRUCT_HAS(host, On1xSDK_Host, capability_granted) ||
        !host->capability_granted ||
        !host->capability_granted(
            host->context_token,
            (On1xSDK_StringView){"diagnostics.emit", 16}, 1)) {
        return ON1X_SDK_CAPABILITY_DENIED;
    }
    if (ON1X_SDK_STRUCT_HAS(host, On1xSDK_Host, log) && host->log) {
        return host->log(
            host->context_token, ON1X_SDK_LOG_INFO,
            (On1xSDK_StringView){"example", 7},
            (On1xSDK_StringView){"initialized", 11}
        );
    }
    return ON1X_SDK_OK;
}

static On1xSDK_Status register_all(
    const On1xSDK_Host *host, On1xSDK_Registry *registry, void *userdata
) {
    (void)host;
    (void)registry;
    (void)userdata;
    /* Register public facet descriptors here. Registration is transactional. */
    return ON1X_SDK_OK;
}

static On1xSDK_Status shutdown(
    const On1xSDK_Host *host, void *userdata
) {
    (void)host;
    (void)userdata;
    return ON1X_SDK_OK;
}

static const On1xSDK_CapabilityRequest capabilities[] = {
    {
        {"diagnostics.emit", 16}, 1, 0,
        ON1X_SDK_CAPABILITY_REQUIRED, 0
    }
};

const On1xSDK_ExtensionV2 on1x_sdk_abi2_example = {
    ON1X_SDK_ABI_VERSION_V2,
    sizeof(On1xSDK_ExtensionV2),
    {"example.core", 12},
    {"1.0.0", 5},
    {"example.core", 12},
    NULL, 0,
    capabilities, 1,
    register_all,
    initialize,
    shutdown,
    NULL
};

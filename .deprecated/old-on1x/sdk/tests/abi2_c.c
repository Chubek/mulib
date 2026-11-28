#include "On1xSDK-ABI.h"
#include "On1xSDK-Allocator.h"
#include "On1xSDK-Budget.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-Capability.h"
#include "On1xSDK-Catalogue.h"
#include "On1xSDK-Configuration.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-Event.h"
#include "On1xSDK-Extension.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-Host.h"
#include "On1xSDK-Lifecycle.h"
#include "On1xSDK-Metadata.h"
#include "On1xSDK-Plugin.h"
#include "On1xSDK-PluginDependency.h"
#include "On1xSDK-PluginDiscovery.h"
#include "On1xSDK-PluginEvent.h"
#include "On1xSDK-PluginHost.h"
#include "On1xSDK-PluginManifest.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-PluginSignature.h"
#include "On1xSDK-PluginTrust.h"
#include "On1xSDK-PluginWorker.h"
#include "On1xSDK-Provenance.h"
#include "On1xSDK-Sandbox.h"
#include "On1xSDK-Serialization.h"
#include "On1xSDK-SharedMemory.h"
#include "On1xSDK-Span.h"
#include "On1xSDK-Status.h"
#include "On1xSDK-String.h"
#include "On1xSDK-Transport.h"
#include "On1xSDK-Version.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(ON1X_SDK_ABI_VERSION == 1u, "v1 source ABI changed");
_Static_assert(ON1X_SDK_ABI_VERSION_V2 == 2u, "v2 ABI missing");
_Static_assert(ON1X_SDK_OK == 0 && ON1X_SDK_BUSY == 7,
               "v1 status values changed");
_Static_assert(offsetof(On1xSDK_ABIHeader, abi_version) == 0,
               "ABI prefix changed");
_Static_assert(offsetof(On1xSDK_ABIHeader, struct_size) == 4,
               "ABI prefix changed");

int main(void) {
    uint32_t selected = 0;
    On1xSDK_Version version = {
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_Version),
        0, 0, 0, {NULL, 0}
    };
    On1xSDK_Context *context = NULL;
    On1xSDK_ContextConfig config;
    memset(&config, 0, sizeof config);
    config.abi_version = ON1X_SDK_ABI_VERSION_V2;
    config.struct_size = sizeof config;
    if (on1x_sdk_negotiate_abi(1, 2, &selected) != ON1X_SDK_OK ||
        selected != ON1X_SDK_ABI_VERSION_V2 ||
        on1x_sdk_version(&version) != ON1X_SDK_OK ||
        version.major != ON1X_SDK_VERSION_MAJOR ||
        on1x_sdk_context_create(&config, &context) != ON1X_SDK_OK ||
        context == NULL ||
        on1x_sdk_context_host(context) == NULL) {
        on1x_sdk_context_release(context);
        return 1;
    }
    on1x_sdk_context_release(context);
    return 0;
}

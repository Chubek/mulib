#include "On1xSDK.h"

static int calls;

static On1xSDK_Status register_all(const On1xSDK_Host *host,
                                   On1xSDK_Registry *registry)
{
    if (!host || !registry)
        return ON1X_SDK_ERROR;
    ++calls;
    return ON1X_SDK_OK;
}

int main(void)
{
    On1xSDK_Registry *registry = 0;
    const On1xSDK_Extension descriptor = {
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Extension), "c-fixture", "1", register_all
    };
    if (on1x_sdk_registry_create(&registry) != ON1X_SDK_OK)
        return 1;
    if (on1x_sdk_registry_register(registry, &descriptor) != ON1X_SDK_OK || calls != 1) {
        on1x_sdk_registry_destroy(registry);
        return 2;
    }
    on1x_sdk_registry_destroy(registry);
    return 0;
}

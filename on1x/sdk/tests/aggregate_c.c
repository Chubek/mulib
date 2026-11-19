#include "On1xSDK.h"

#include <stdint.h>

_Static_assert(ON1X_SDK_ABI_VERSION == 1u,
               "the ABI-v1 compatibility macro must remain frozen");
_Static_assert(ON1X_SDK_ABI_CURRENT_VERSION == 2u,
               "the current SDK ABI must be version 2");
_Static_assert(ON1X_SDK_UNSATISFIABLE == 19,
               "the ABI-2 status range changed");
_Static_assert(sizeof(On1xSDK_MIREffect) == sizeof(uint32_t),
               "the shared MIR effect carrier must remain 32-bit");

int main(void)
{
    On1xSDK_Version version = {
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_Version),
        0,
        0,
        0,
        {0, 0},
    };
    uint32_t selected = 0;
    if (on1x_sdk_version(&version) != ON1X_SDK_OK ||
        version.major != 2u || version.minor != 0u || version.patch != 0u)
        return 1;
    if (on1x_sdk_negotiate_abi(
            ON1X_SDK_ABI_VERSION_V1,
            ON1X_SDK_ABI_VERSION_V2,
            &selected) != ON1X_SDK_OK ||
        selected != ON1X_SDK_ABI_VERSION_V2)
        return 2;
    return on1x_sdk_status_is_v1(ON1X_SDK_BUSY) &&
                   on1x_sdk_status_is_known(ON1X_SDK_UNSATISFIABLE)
               ? 0
               : 3;
}

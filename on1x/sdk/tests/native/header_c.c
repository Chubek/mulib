#include "trusted_bridge.h"

_Static_assert(sizeof(On1xSDKNative_ABIHeader) == 2u * sizeof(uint32_t),
               "native ABI prefix must remain two u32 fields");
_Static_assert(sizeof(On1xSDKNative_Status) == sizeof(uint32_t),
               "native status carrier must remain u32");
_Static_assert(ON1X_SDK_NATIVE_OK == 0u, "ABI-v1 OK value drift");
_Static_assert(ON1X_SDK_NATIVE_BUSY == 7u, "ABI-v1 BUSY value drift");
_Static_assert(ON1X_SDK_NATIVE_UNSATISFIABLE == 19u,
               "ABI-2 status range drift");

int on1x_sdk_native_header_c_probe(void)
{
    return ON1X_SDK_NATIVE_BRIDGE_ABI == 1u ? 0 : 1;
}

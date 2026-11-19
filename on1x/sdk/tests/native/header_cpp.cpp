#include "trusted_bridge.h"

#include <type_traits>

static_assert(std::is_standard_layout_v<On1xSDKNative_ABIHeader>);
static_assert(std::is_same_v<On1xSDKNative_ABIHeader, On1xSDK_ABIHeader>);
static_assert(std::is_same_v<On1xSDKNative_Status, On1xSDK_Status>);
static_assert(std::is_same_v<On1xSDKNative_Handle, On1xSDK_Handle>);
static_assert(std::is_same_v<On1xSDKNative_Span, On1xSDK_SourceSpan>);
static_assert(sizeof(On1xSDKNative_Status) == sizeof(::uint32_t));
static_assert(ON1X_SDK_NATIVE_OK == 0u);
static_assert(ON1X_SDK_NATIVE_BUSY == 7u);
static_assert(ON1X_SDK_NATIVE_UNSATISFIABLE == 19u);

int on1x_sdk_native_header_cpp_probe()
{
    return on1x_sdk_native_reference_module.abi_version ==
        ON1X_SDK_NATIVE_BRIDGE_ABI ? 0 : 1;
}

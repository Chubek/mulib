#include "On1xSDK-FFIDescriptor.h"

extern "C" On1xSDK_Status
on1x_sdk_ffi_descriptor_find(const On1xSDK_FFIDescriptorQuery *query,
                            On1xSDK_FFIDescriptorView *out) {
    constexpr auto abi = ON1X_SDK_ABI_VERSION_V2;
    if (!out)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (out->abi_version != abi)
        return ON1X_SDK_ABI_MISMATCH;
    if (out->struct_size < sizeof(*out))
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto capacity = out->struct_size;
    *out = {};
    out->abi_version = abi;
    out->struct_size = capacity;

    if (!query)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (query->abi_version != abi)
        return ON1X_SDK_ABI_MISMATCH;
    if (query->struct_size < sizeof(*query))
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto by_name = query->module.size != 0 || query->member.size != 0;
    const auto pinned = query->descriptor_id.size != 0;
    if (!on1x_sdk_ffi_text_valid(query->target, 0) ||
        (by_name && (!on1x_sdk_ffi_text_valid(query->module, 0) ||
                     !on1x_sdk_ffi_text_valid(query->member, 0))) ||
        (!by_name && !pinned) ||
        (pinned && !on1x_sdk_ffi_sha256_valid(query->descriptor_id)))
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_UNSUPPORTED;
}

#include "trusted_bridge.h"

/*
 * This symbol is emitted by the pinned canonical on1xc C11 backend. The
 * adapter is intentionally rebuilt with that generated C; it is not claimed
 * as a stable general On1x function ABI.
 */
extern uint32_t on1x_fn_basis_sdk_reference_extension(
    uint64_t context,
    uint32_t operation
);

static On1xSDKNative_Status invoke_reference(
    On1xSDKNative_Handle context,
    uint32_t operation
)
{
    const uint32_t status =
        on1x_fn_basis_sdk_reference_extension(context, operation);
    return status <= ON1X_SDK_NATIVE_IO ? status : ON1X_SDK_NATIVE_ERROR;
}

const On1xSDKNative_Module on1x_sdk_native_reference_module = {
    ON1X_SDK_NATIVE_BRIDGE_ABI,
    (uint32_t)sizeof(On1xSDKNative_Module),
    ON1X_SDK_NATIVE_SDK_ABI,
    ON1X_SDK_NATIVE_BRIDGE_ABI,
    ON1X_SDK_NATIVE_SOURCE_SCHEMA,
    ON1X_SDK_NATIVE_CANDIDATE_SCHEMA,
    ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT |
        ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT,
    "org.on1x.reference-byte-analyzer",
    sizeof("org.on1x.reference-byte-analyzer") - 1u,
    invoke_reference
};

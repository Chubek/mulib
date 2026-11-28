#include "external_backend.h"

#include <stdint.h>

static On1xSDK_Status receipt(
    void *userdata,
    const On1xSDK_BackendInvocation *invocation,
    On1xSDK_OutputSink *artifact
) {
    static const char prefix[] = "verified-mir:";
    const uint8_t *bytes = NULL;
    size_t size = 0;
    On1xSDK_Status status;
    (void)userdata;
    if (invocation == NULL || invocation->program == NULL ||
        invocation->input_schema != 1u ||
        invocation->artifact_kind != ON1X_SDK_ARTIFACT_MIR_TEXT)
        return ON1X_SDK_INVALID_ARGUMENT;
    status = on1x_sdk_backend_poll(invocation);
    if (status != ON1X_SDK_OK)
        return status;
    status = on1x_sdk_program_bytes(invocation->program, &bytes, &size);
    if (status != ON1X_SDK_OK)
        return status;
    status = on1x_sdk_output_write(artifact, prefix, sizeof(prefix) - 1u);
    if (status != ON1X_SDK_OK)
        return status;
    return on1x_sdk_output_write(artifact, bytes, size);
}

On1xSDK_Status on1x_sdk_example_backend_register(On1xSDK_Registry *registry) {
    static const On1xSDK_BackendCapability capability = {
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR,
        1u,
        1u,
        ON1X_SDK_ARTIFACT_MIR_TEXT,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        NULL,
        0u,
        NULL,
        0u,
        NULL,
        0u
    };
    static const On1xSDK_BackendProvider provider = {
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
        sizeof(On1xSDK_BackendProvider),
        "example.receipt",
        sizeof("example.receipt") - 1u,
        0,
        0u,
        &capability,
        1u,
        receipt,
        NULL
    };
    return on1x_sdk_backend_provider_register(registry, &provider);
}

#include "On1xSDK.h"

#include <string.h>

static On1xSDK_Status inspect_mir(void *userdata, const On1xSDK_Program *program,
                                   On1xSDK_AnalyzerSink *sink)
{
    const uint8_t *bytes = NULL;
    size_t size = 0;
    On1xSDK_Phase phase;
    static const char prefix[] = "(on1x.mir ";
    if (on1x_sdk_program_phase(program, &phase) != ON1X_SDK_OK ||
        phase != ON1X_SDK_VERIFIED_MIR ||
        on1x_sdk_program_bytes(program, &bytes, &size) != ON1X_SDK_OK ||
        size < sizeof(prefix) - 1 || memcmp(bytes, prefix, sizeof(prefix) - 1) != 0)
        return ON1X_SDK_ERROR;

    const On1xSDK_Diagnostic record = {
        userdata ? ON1X_SDK_DIAGNOSTIC_ERROR : ON1X_SDK_NOTE,
        userdata ? "SDK-FIXTURE-REJECT" : "SDK-FIXTURE-MIR",
        userdata ? "fixture rejected verified MIR" : "fixture received verified MIR",
        0, sizeof(prefix) - 1
    };
    return on1x_sdk_analyzer_emit(sink, &record);
}

static On1xSDK_Status copy_mir(void *userdata, const On1xSDK_Program *program,
                                On1xSDK_OutputSink *sink)
{
    (void)userdata;
    const uint8_t *bytes = NULL;
    size_t size = 0;
    On1xSDK_Status status = on1x_sdk_program_bytes(program, &bytes, &size);
    return status == ON1X_SDK_OK ? on1x_sdk_output_write(sink, bytes, size) : status;
}

static On1xSDK_Status fail(void *userdata, const On1xSDK_Program *program,
                             On1xSDK_OutputSink *sink)
{
    (void)program;
    if (on1x_sdk_output_write(sink, "partial", 7) != ON1X_SDK_OK)
        return ON1X_SDK_ERROR;
    return userdata ? ON1X_SDK_LIMIT : ON1X_SDK_ERROR;
}

static On1xSDK_Status register_all(const On1xSDK_Host *host, On1xSDK_Registry *registry)
{
    if (!host)
        return ON1X_SDK_ERROR;
    static int reject, limit;
    const On1xSDK_Analyzer analyzers[] = {
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Analyzer), "mir-check",
         ON1X_SDK_VERIFIED_MIR, inspect_mir, NULL},
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Analyzer), "mir-reject",
         ON1X_SDK_VERIFIED_MIR, inspect_mir, &reject}
    };
    const On1xSDK_Backend backends[] = {
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend), "mir-copy",
         ON1X_SDK_VERIFIED_MIR, copy_mir, NULL},
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend), "mir-fail",
         ON1X_SDK_VERIFIED_MIR, fail, NULL},
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend), "mir-limit",
         ON1X_SDK_VERIFIED_MIR, fail, &limit}
    };
    for (size_t i = 0; i < sizeof(analyzers) / sizeof(analyzers[0]); ++i) {
        On1xSDK_Status status = on1x_sdk_analyzer_register(registry, &analyzers[i]);
        if (status != ON1X_SDK_OK)
            return status;
    }
    for (size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); ++i) {
        On1xSDK_Status status = on1x_sdk_backend_register(registry, &backends[i]);
        if (status != ON1X_SDK_OK)
            return status;
    }
    return ON1X_SDK_OK;
}

const On1xSDK_Extension *on1x_sdk_linked_extension(void)
{
    static const On1xSDK_Extension extension = {
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Extension), "mir-fixture", "1", register_all
    };
    return &extension;
}

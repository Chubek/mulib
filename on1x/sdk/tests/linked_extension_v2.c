#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Extension.h"

#include <string.h>

#define TEXT(s) { (s), sizeof(s) - 1 }

enum { GOOD, BAD_UTF8, BAD_SPAN, REJECT, OVERFLOW, UNKNOWN, BAD_SHUTDOWN };
static int modes[] = {GOOD, BAD_UTF8, BAD_SPAN, REJECT, OVERFLOW, UNKNOWN, BAD_SHUTDOWN};
static int active, fail_shutdown;

typedef struct { unsigned multiply_count; int saw_19; } Evidence;

static On1xSDK_Status inspect(void *userdata, const On1xSDK_MIRSnapshot *snapshot,
                              const On1xSDK_MIRRef *instruction)
{
    Evidence *evidence = userdata;
    On1xSDK_MIROp op;
    On1xSDK_Status status = on1x_sdk_mir_instruction_op(snapshot, instruction, &op);
    if (status != ON1X_SDK_OK) return status;
    if (op == ON1X_SDK_MIR_MUL) ++evidence->multiply_count;
    if (op == ON1X_SDK_MIR_CONSTANT) {
        int64_t value;
        status = on1x_sdk_mir_instruction_immediate(snapshot, instruction, &value);
        if (status == ON1X_SDK_OK && value == 19) evidence->saw_19 = 1;
    }
    return status;
}

static On1xSDK_Status run(void *userdata, const On1xSDK_MIRSnapshot *snapshot,
                          const On1xSDK_DiagnosticSinkV2 *sink)
{
    const int mode = *(const int *)userdata;
    On1xSDK_IRIdentity identity;
    On1xSDK_MIRRef function;
    On1xSDK_IRStringView name;
    size_t count;
    Evidence evidence = {0};
    if (!active || on1x_sdk_mir_snapshot_identity(snapshot, &identity) != ON1X_SDK_OK ||
        identity.state != ON1X_SDK_IR_VERIFIED_MIR || !identity.revision ||
        on1x_sdk_mir_snapshot_function_count(snapshot, &count) != ON1X_SDK_OK ||
        count != 1 || on1x_sdk_mir_snapshot_function(snapshot, 0, &function) != ON1X_SDK_OK ||
        on1x_sdk_mir_function_name(snapshot, &function, &name) != ON1X_SDK_OK ||
        name.size != 4 || memcmp(name.data, "main", 4) != 0)
        return ON1X_SDK_VERIFICATION_FAILED;
    const On1xSDK_IRLimits limits = {
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof limits, 64, 1024, 16
    };
    On1xSDK_Status status = on1x_sdk_mir_visit_instructions(snapshot, &limits, inspect, &evidence, NULL);
    if (status != ON1X_SDK_OK || evidence.multiply_count != 1 || !evidence.saw_19)
        return ON1X_SDK_VERIFICATION_FAILED;

    char message[] = "verified main has one multiply and constant 19";
    On1xSDK_DiagnosticV2 record = {
        .abi_version = ON1X_SDK_ABI_VERSION_V2,
        .struct_size = sizeof record,
        .severity = mode == REJECT ? ON1X_SDK_DIAGNOSTIC_ERROR : ON1X_SDK_NOTE,
        .code = TEXT("SDK-V2-FIXTURE"),
        .message = {message, sizeof message - 1},
        .primary = on1x_sdk_span_unlocated()
    };
    status = sink->emit(sink->userdata, &record);
    if (status != ON1X_SDK_OK) return status;
    /* Distinguish a copied record from borrowed callback storage. */
    memset(message, 'x', sizeof message - 1);
    switch (mode) {
    case BAD_UTF8:
        record.message = (On1xSDK_StringView)TEXT("\xc0\xaf");
        (void)sink->emit(sink->userdata, &record);
        break;
    case BAD_SPAN:
        record.primary.source_id = 1;
        record.primary.revision = identity.revision;
        record.primary.byte_begin = 0;
        record.primary.byte_end = 1;
        (void)sink->emit(sink->userdata, &record);
        break;
    case OVERFLOW:
        for (size_t i = 0; i < ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS; ++i)
            (void)sink->emit(sink->userdata, &record);
        break;
    case UNKNOWN: return (On1xSDK_Status)200;
    case BAD_SHUTDOWN: fail_shutdown = 1; break;
    default: break;
    }
    return ON1X_SDK_OK; /* Ignoring a sink failure must not publish earlier entries. */
}

static On1xSDK_Status register_all(const On1xSDK_Host *host, On1xSDK_Registry *registry,
                                   void *userdata)
{
    (void)host;
    (void)userdata;
    const On1xSDK_StringView names[] = {
        TEXT("inspect"), TEXT("bad-utf8"), TEXT("bad-span"), TEXT("reject"),
        TEXT("overflow"), TEXT("unknown"), TEXT("bad-shutdown")
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        On1xSDK_AnalyzerV2 analyzer = {
            ON1X_SDK_ABI_VERSION_V2, sizeof analyzer, names[i],
            ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION, 0, run, &modes[i]
        };
        On1xSDK_Status status = on1x_sdk_analyzer_register_v2(registry, &analyzer);
        if (status != ON1X_SDK_OK) return status;
    }
    return ON1X_SDK_OK;
}

static On1xSDK_Status initialize(const On1xSDK_Host *host, void *userdata)
{
    (void)userdata;
    if (!ON1X_SDK_STRUCT_HAS(host, On1xSDK_Host, capability_granted) ||
        !host->capability_granted ||
        !host->capability_granted(host->context_token,
                                  (On1xSDK_StringView)TEXT(ON1X_SDK_MIR_ANALYZE_CAPABILITY),
                                  ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION) ||
        (host->feature_bits & ON1X_SDK_HOST_FEATURE_CANDIDATES))
        return ON1X_SDK_CAPABILITY_DENIED;
    active = 1;
    return ON1X_SDK_OK;
}

static On1xSDK_Status shutdown(const On1xSDK_Host *host, void *userdata)
{
    (void)host;
    (void)userdata;
    active = 0;
    return fail_shutdown ? ON1X_SDK_CALLBACK_FAILED : ON1X_SDK_OK;
}

const On1xSDK_ExtensionV2 *on1x_sdk_linked_extension_v2(void)
{
    static const On1xSDK_CapabilityRequest requests[] = {
        {TEXT(ON1X_SDK_MIR_ANALYZE_CAPABILITY),
         ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION, ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION,
         ON1X_SDK_CAPABILITY_REQUIRED, 0}
    };
    static const On1xSDK_ExtensionV2 extension = {
        ON1X_SDK_ABI_VERSION_V2, sizeof extension, TEXT("sdk-v2-fixture"),
        TEXT("1.0.0"), TEXT("sdk-v2-fixture"), NULL, 0, requests, 1,
        register_all, initialize, shutdown, NULL
    };
    return &extension;
}

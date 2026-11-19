#include "trusted_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                     \
                    __FILE__, __LINE__, #condition);                           \
            return 1;                                                          \
        }                                                                      \
    } while (0)

#define CHECK_STATUS(actual, expected)                                         \
    do {                                                                       \
        const On1xSDKNative_Status check_actual = (actual);                    \
        const On1xSDKNative_Status check_expected = (expected);                \
        if (check_actual != check_expected) {                                  \
            fprintf(stderr, "%s:%d: status %u, expected %u\n",               \
                    __FILE__, __LINE__, (unsigned)check_actual,                \
                    (unsigned)check_expected);                                 \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static On1xSDKNative_ContextConfig default_config(
    const uint8_t *source,
    size_t source_size
)
{
    On1xSDKNative_ContextConfig config;
    memset(&config, 0, sizeof(config));
    config.abi_version = ON1X_SDK_NATIVE_BRIDGE_ABI;
    config.struct_size = (uint32_t)sizeof(config);
    config.sdk_abi_version = ON1X_SDK_NATIVE_SDK_ABI;
    config.bridge_abi_version = ON1X_SDK_NATIVE_BRIDGE_ABI;
    config.bridge_table_size = ON1X_SDK_NATIVE_BRIDGE_TABLE_SIZE;
    config.source_schema = ON1X_SDK_NATIVE_SOURCE_SCHEMA;
    config.candidate_schema = ON1X_SDK_NATIVE_CANDIDATE_SCHEMA;
    config.candidate_limit = ON1X_SDK_NATIVE_CANDIDATE_MAX;
    config.source_revision = UINT64_C(0x102030405060708);
    config.granted_capabilities =
        ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT |
        ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT;
    config.source_data = source;
    config.source_size = source_size;
    config.work_limit = 128u;
    config.cancel_after_polls = UINT32_MAX;
    config.fail_diagnostic_after = UINT32_MAX;
    return config;
}

static int clean_context(On1xSDKNative_Handle context)
{
    size_t count = 99u;
    CHECK_STATUS(on1x_sdk_native_context_live_objects(context, &count),
                 ON1X_SDK_NATIVE_OK);
    CHECK(count == 0u);
    return 0;
}

static int initialize(
    const On1xSDKNative_ContextConfig *config,
    On1xSDKNative_Handle *out_context
)
{
    CHECK_STATUS(on1x_sdk_native_context_create(config, out_context),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     *out_context, ON1X_SDK_NATIVE_OP_INITIALIZE),
                 ON1X_SDK_NATIVE_OK);
    return 0;
}

static int shutdown_and_destroy(On1xSDKNative_Handle context)
{
    CHECK(clean_context(context) == 0);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_SHUTDOWN),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_context_destroy(context),
                 ON1X_SDK_NATIVE_OK);
    return 0;
}

static int test_descriptor(void)
{
    On1xSDKNative_Module bad = on1x_sdk_native_reference_module;
    CHECK_STATUS(on1x_sdk_native_module_validate(
                     &on1x_sdk_native_reference_module),
                 ON1X_SDK_NATIVE_OK);
    bad.struct_size = sizeof(On1xSDKNative_ABIHeader);
    CHECK_STATUS(on1x_sdk_native_module_validate(&bad),
                 ON1X_SDK_NATIVE_ABI_MISMATCH);
    bad = on1x_sdk_native_reference_module;
    bad.requested_capabilities = UINT64_C(0xffff);
    CHECK_STATUS(on1x_sdk_native_module_validate(&bad),
                 ON1X_SDK_NATIVE_INVALID_ARGUMENT);
    return 0;
}

static int test_success(void)
{
    static const uint8_t source[] = "alpha!beta!";
    static const uint8_t expected_candidate[] = {'O', 'N', '1', 'X', 1u, 2u};
    On1xSDKNative_ContextConfig config =
        default_config(source, sizeof(source) - 1u);
    On1xSDKNative_Handle context = 0u;
    const uint8_t *candidate = NULL;
    size_t candidate_size = 0u;
    size_t diagnostic_count = 0u;
    uint32_t metadata_state = 0u;
    On1xSDKNative_Diagnostic diagnostic;

    CHECK(initialize(&config, &context) == 0);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_V1_STATUS_PROBE),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_HANDLE_PROBE),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_DIAGNOSTIC_PROBE),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, 999u),
                 ON1X_SDK_NATIVE_UNSUPPORTED);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_ANALYZE),
                 ON1X_SDK_NATIVE_OK);
    CHECK(clean_context(context) == 0);

    CHECK_STATUS(on1x_sdk_native_context_diagnostic_count(
                     context, &diagnostic_count),
                 ON1X_SDK_NATIVE_OK);
    CHECK(diagnostic_count == 2u);
    CHECK_STATUS(on1x_sdk_native_context_diagnostic(
                     context, 0u, &diagnostic),
                 ON1X_SDK_NATIVE_OK);
    CHECK(diagnostic.machine_code_id == ON1X_SDK_NATIVE_DIAGNOSTIC_CODE);
    CHECK(diagnostic.severity ==
          ON1X_SDK_NATIVE_DIAGNOSTIC_SEVERITY_WARNING);
    CHECK(diagnostic.message_id == ON1X_SDK_NATIVE_DIAGNOSTIC_MESSAGE);
    CHECK(diagnostic.machine_code_length ==
          sizeof("ON1X-SDK-NATIVE-EXCLAMATION") - 1u);
    CHECK(diagnostic.message_length ==
          sizeof("discouraged exclamation byte") - 1u);
    CHECK(memcmp(diagnostic.machine_code, "ON1X-SDK-NATIVE-EXCLAMATION",
                 diagnostic.machine_code_length) == 0);
    CHECK(memcmp(diagnostic.message, "discouraged exclamation byte",
                 diagnostic.message_length) == 0);
    CHECK(diagnostic.primary.revision == config.source_revision);
    CHECK(diagnostic.primary.start == 5u);
    CHECK(diagnostic.primary.end == 6u);
    CHECK(diagnostic.related_count == 0u);

    CHECK_STATUS(on1x_sdk_native_context_candidate(
                     context, &candidate, &candidate_size),
                 ON1X_SDK_NATIVE_OK);
    CHECK(candidate_size == sizeof(expected_candidate));
    CHECK(memcmp(candidate, expected_candidate, candidate_size) == 0);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_ANALYZE),
                 ON1X_SDK_NATIVE_CONFLICT);
    CHECK(clean_context(context) == 0);
    CHECK_STATUS(on1x_sdk_native_context_candidate(
                     context, &candidate, &candidate_size),
                 ON1X_SDK_NATIVE_OK);
    CHECK(candidate_size == sizeof(expected_candidate));
    CHECK(memcmp(candidate, expected_candidate, candidate_size) == 0);
    CHECK_STATUS(on1x_sdk_native_context_metadata_state(
                     context, &metadata_state),
                 ON1X_SDK_NATIVE_OK);
    CHECK(metadata_state == 2u);
    CHECK(shutdown_and_destroy(context) == 0);
    return 0;
}

static int test_negotiation_and_registration_failures(void)
{
    static const uint8_t source[] = "ok";
    On1xSDKNative_ContextConfig config =
        default_config(source, sizeof(source) - 1u);
    On1xSDKNative_Handle context = 0u;
    uint32_t metadata_state = 99u;

    config.sdk_abi_version = 1u;
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_INITIALIZE),
                 ON1X_SDK_NATIVE_ABI_MISMATCH);
    CHECK_STATUS(on1x_sdk_native_context_metadata_state(
                     context, &metadata_state),
                 ON1X_SDK_NATIVE_OK);
    CHECK(metadata_state == 0u);
    CHECK_STATUS(on1x_sdk_native_context_destroy(context),
                 ON1X_SDK_NATIVE_OK);

    config = default_config(source, sizeof(source) - 1u);
    config.bridge_table_size = ON1X_SDK_NATIVE_BRIDGE_TABLE_SIZE - 1u;
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_INITIALIZE),
                 ON1X_SDK_NATIVE_ABI_MISMATCH);
    CHECK_STATUS(on1x_sdk_native_context_destroy(context),
                 ON1X_SDK_NATIVE_OK);

    config = default_config(source, sizeof(source) - 1u);
    config.fail_initialize = 1u;
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_INITIALIZE),
                 ON1X_SDK_NATIVE_ERROR);
    CHECK_STATUS(on1x_sdk_native_context_metadata_state(
                     context, &metadata_state),
                 ON1X_SDK_NATIVE_OK);
    CHECK(metadata_state == 0u);
    CHECK(clean_context(context) == 0);
    CHECK_STATUS(on1x_sdk_native_context_destroy(context),
                 ON1X_SDK_NATIVE_OK);
    return 0;
}

static int run_analysis_failure(
    On1xSDKNative_ContextConfig config,
    On1xSDKNative_Status expected
)
{
    On1xSDKNative_Handle context = 0u;
    const uint8_t *candidate = NULL;
    size_t candidate_size = 99u;
    CHECK(initialize(&config, &context) == 0);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_ANALYZE),
                 expected);
    CHECK(clean_context(context) == 0);
    CHECK_STATUS(on1x_sdk_native_context_candidate(
                     context, &candidate, &candidate_size),
                 ON1X_SDK_NATIVE_NOT_FOUND);
    CHECK(candidate == NULL);
    CHECK(candidate_size == 0u);
    CHECK(shutdown_and_destroy(context) == 0);
    return 0;
}

static int test_schema_capability_and_limits(void)
{
    static const uint8_t source[] = "a!b!";
    On1xSDKNative_ContextConfig config =
        default_config(source, sizeof(source) - 1u);

    config.source_schema = 2u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_ABI_MISMATCH) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.candidate_schema = 2u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_ABI_MISMATCH) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.granted_capabilities = ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT;
    CHECK(run_analysis_failure(
              config, ON1X_SDK_NATIVE_CAPABILITY_DENIED) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.granted_capabilities = ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT;
    CHECK(run_analysis_failure(
              config, ON1X_SDK_NATIVE_CAPABILITY_DENIED) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.candidate_limit = 4u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_LIMIT) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.work_limit = 2u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_LIMIT) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.cancel_after_polls = 2u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_CANCELLED) == 0);
    return 0;
}

static int test_host_failures_cleanup(void)
{
    static const uint8_t source[] = "!work";
    On1xSDKNative_ContextConfig config =
        default_config(source, sizeof(source) - 1u);

    config.fail_diagnostic_after = 0u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_IO) == 0);

    config = default_config(source, sizeof(source) - 1u);
    config.fail_publish = 1u;
    CHECK(run_analysis_failure(config, ON1X_SDK_NATIVE_IO) == 0);
    return 0;
}

static int test_malformed_and_lifecycle(void)
{
    static const uint8_t source[] = "life";
    static const uint8_t malformed[] = {0xc0u, 0x80u};
    uint8_t too_large[ON1X_SDK_NATIVE_SOURCE_MAX + 1u];
    On1xSDKNative_ContextConfig config =
        default_config(malformed, sizeof(malformed));
    On1xSDKNative_Handle context = 0u;

    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_VERIFICATION_FAILED);
    CHECK(context == 0u);

    memset(too_large, 'x', sizeof(too_large));
    config = default_config(too_large, sizeof(too_large));
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_LIMIT);
    CHECK(context == 0u);

    config = default_config(source, sizeof(source) - 1u);
    config.struct_size = sizeof(On1xSDKNative_ABIHeader);
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_ABI_MISMATCH);

    config = default_config(source, sizeof(source) - 1u);
    CHECK_STATUS(on1x_sdk_native_context_create(&config, &context),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_ANALYZE),
                 ON1X_SDK_NATIVE_INVALID_STATE);
    CHECK_STATUS(on1x_sdk_native_reference_module.invoke(
                     context, ON1X_SDK_NATIVE_OP_SHUTDOWN),
                 ON1X_SDK_NATIVE_INVALID_STATE);
    CHECK_STATUS(on1x_sdk_native_context_destroy(context),
                 ON1X_SDK_NATIVE_OK);
    return 0;
}

static int test_owner_related_and_busy(void)
{
    static const uint8_t source[] = "!owner";
    On1xSDKNative_ContextConfig config =
        default_config(source, sizeof(source) - 1u);
    On1xSDKNative_Handle first = 0u;
    On1xSDKNative_Handle second = 0u;
    uint64_t snapshot;

    CHECK(initialize(&config, &first) == 0);
    config.source_revision += 1u;
    CHECK(initialize(&config, &second) == 0);
    snapshot = basis_sdk_bridge_snapshot_open(
        first, ON1X_SDK_NATIVE_SOURCE_SCHEMA, ON1X_SDK_NATIVE_SOURCE_MAX);
    CHECK(snapshot >= UINT64_C(0x100000000));
    CHECK_STATUS(basis_sdk_bridge_snapshot_check(second, snapshot),
                 ON1X_SDK_NATIVE_CONFLICT);
    CHECK_STATUS(basis_sdk_bridge_diagnostic_emit(
                     first, snapshot, config.source_revision - 1u,
                     ON1X_SDK_NATIVE_DIAGNOSTIC_CODE,
                     ON1X_SDK_NATIVE_DIAGNOSTIC_SEVERITY_WARNING,
                     ON1X_SDK_NATIVE_DIAGNOSTIC_MESSAGE,
                     (uint32_t)(sizeof("discouraged exclamation byte") - 1u),
                     0u, 1u, 2u, 0u, 0u),
                 ON1X_SDK_NATIVE_LIMIT);
    CHECK_STATUS(on1x_sdk_native_context_destroy(first),
                 ON1X_SDK_NATIVE_BUSY);
    CHECK_STATUS(basis_sdk_bridge_snapshot_close(first, snapshot),
                 ON1X_SDK_NATIVE_OK);
    CHECK_STATUS(basis_sdk_bridge_snapshot_check(first, snapshot),
                 ON1X_SDK_NATIVE_STALE);
    CHECK(shutdown_and_destroy(first) == 0);
    CHECK(shutdown_and_destroy(second) == 0);
    return 0;
}

int main(void)
{
    CHECK(test_descriptor() == 0);
    CHECK(test_success() == 0);
    CHECK(test_negotiation_and_registration_failures() == 0);
    CHECK(test_schema_capability_and_limits() == 0);
    CHECK(test_host_failures_cleanup() == 0);
    CHECK(test_malformed_and_lifecycle() == 0);
    CHECK(test_owner_related_and_busy() == 0);
    puts("native SDK host profile: all checks passed");
    return 0;
}

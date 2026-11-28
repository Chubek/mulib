#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Extension.h"
#include "On1xSDK-Registry.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

namespace {
using Registry = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;
using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;

enum class Mode { good, fail, exception, allocation, invalid, bad_code, bad_severity, too_many, too_long };
struct State {
    Mode mode = Mode::good;
    size_t calls = 0;
};

constexpr uint8_t expected[] = {'a', 0, 'b'};

extern "C" On1xSDK_Status analyze(void *userdata, const On1xSDK_Program *program,
                                  On1xSDK_AnalyzerSink *sink) {
    auto &state = *static_cast<State *>(userdata);
    ++state.calls;
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    On1xSDK_Phase phase{};
    CHECK(on1x_sdk_program_phase(program, &phase) == ON1X_SDK_OK && phase == ON1X_SDK_SOURCE);
    CHECK(on1x_sdk_program_bytes(program, &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == sizeof expected && std::memcmp(bytes, expected, size) == 0);

    char code[] = "lint.note";
    char message[] = "first";
    On1xSDK_Diagnostic record{ON1X_SDK_NOTE, code, message, 1, 3};
    CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_OK);
    code[0] = '!';
    message[0] = '!';
    record = {ON1X_SDK_DIAGNOSTIC_ERROR, "lint.error", "second", SIZE_MAX, SIZE_MAX};
    switch (state.mode) {
    case Mode::good:
        return on1x_sdk_analyzer_emit(sink, &record);
    case Mode::fail:
        return ON1X_SDK_UNSUPPORTED;
    case Mode::exception:
        throw 1;
    case Mode::allocation:
        throw std::bad_alloc{};
    case Mode::invalid:
        record.byte_begin = 0;
        record.byte_end = size + 1;
        CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_INVALID_ARGUMENT);
        CHECK(on1x_sdk_analyzer_emit(sink, nullptr) == ON1X_SDK_INVALID_ARGUMENT);
        return ON1X_SDK_OK;
    case Mode::bad_code:
        record.code = "../bad";
        CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_INVALID_ARGUMENT);
        return ON1X_SDK_OK;
    case Mode::bad_severity:
        record.severity = 55;
        CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_INVALID_ARGUMENT);
        return ON1X_SDK_OK;
    case Mode::too_many:
        for (size_t i = 1; i < ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS; ++i)
            CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_OK);
        CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_LIMIT);
        return ON1X_SDK_OK;
    case Mode::too_long: {
        std::array<char, ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES + 2> long_message;
        long_message.fill('x');
        long_message.back() = '\0';
        record.message = long_message.data();
        CHECK(on1x_sdk_analyzer_emit(sink, &record) == ON1X_SDK_LIMIT);
        return ON1X_SDK_ERROR; // Staging LIMIT still takes precedence.
    }
    }
    std::abort();
}

extern "C" void collect(void *userdata, const On1xSDK_Diagnostic *record) {
    auto &count = *static_cast<size_t *>(userdata);
    switch (count++) {
    case 0:
        CHECK(record->severity == ON1X_SDK_NOTE);
        CHECK(std::string_view(record->code) == "lint.note" && std::string_view(record->message) == "first");
        CHECK(record->byte_begin == 1 && record->byte_end == 3);
        break;
    case 1:
        CHECK(record->severity == ON1X_SDK_DIAGNOSTIC_ERROR);
        CHECK(std::string_view(record->code) == "lint.error" && std::string_view(record->message) == "second");
        CHECK(record->byte_begin == SIZE_MAX && record->byte_end == SIZE_MAX);
        break;
    default:
        std::abort();
    }
}

extern "C" On1xSDK_Status never_run_v2(void *, const On1xSDK_MIRSnapshot *,
                                        const On1xSDK_DiagnosticSinkV2 *) {
    std::abort();
}

const On1xSDK_AnalyzerV2 analyzer_v2{
    ON1X_SDK_ABI_VERSION_V2, sizeof analyzer_v2, {"mir-lint", 8},
    ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION, 0, never_run_v2, nullptr
};

extern "C" On1xSDK_Status register_v2(const On1xSDK_Host *, On1xSDK_Registry *registry, void *) {
    return on1x_sdk_analyzer_register_v2(registry, &analyzer_v2);
}

extern "C" On1xSDK_Status initialize_v2(const On1xSDK_Host *, void *userdata) {
    return *static_cast<const bool *>(userdata) ? ON1X_SDK_ERROR : ON1X_SDK_OK;
}

extern "C" On1xSDK_Status shutdown_v2(const On1xSDK_Host *, void *) {
    return ON1X_SDK_OK;
}

void registration_v2(On1xSDK_Registry *legacy) {
    CHECK(on1x_sdk_analyzer_register_v2(legacy, &analyzer_v2) == ON1X_SDK_CAPABILITY_DENIED);
    const On1xSDK_ABIHeader short_header{ON1X_SDK_ABI_VERSION_V2, sizeof short_header};
    CHECK(on1x_sdk_analyzer_register_v2(
        legacy, reinterpret_cast<const On1xSDK_AnalyzerV2 *>(&short_header)) == ON1X_SDK_ABI_MISMATCH);
    auto unsupported = analyzer_v2;
    ++unsupported.input_schema;
    CHECK(on1x_sdk_analyzer_register_v2(legacy, &unsupported) == ON1X_SDK_UNSUPPORTED);
    constexpr On1xSDK_StringView capability{
        ON1X_SDK_MIR_ANALYZE_CAPABILITY, sizeof(ON1X_SDK_MIR_ANALYZE_CAPABILITY) - 1
    };
    const On1xSDK_CapabilityRequest request{
        capability, ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION, 0, ON1X_SDK_CAPABILITY_REQUIRED, 0
    };
    bool fail = false;
    On1xSDK_ExtensionV2 extension{
        ON1X_SDK_ABI_VERSION_V2, sizeof extension, {"test", 4}, {"1.0.0", 5}, {"key", 3},
        nullptr, 0, nullptr, 0, register_v2, initialize_v2, shutdown_v2, &fail
    };
    // No granted capability, even though the extension is ABI 2.
    CHECK(on1x_sdk_registry_register_v2(legacy, &extension, nullptr) == ON1X_SDK_CAPABILITY_DENIED);
    const On1xSDK_CapabilityGrant grant{capability, ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION, 0};
    const On1xSDK_RegistryConfig config{
        ON1X_SDK_ABI_VERSION_V2, sizeof config, nullptr, &grant, 1, 1, 0
    };
    On1xSDK_Registry *raw = nullptr;
    CHECK(on1x_sdk_registry_create_v2(&config, &raw) == ON1X_SDK_OK);
    Registry registry(raw, on1x_sdk_registry_destroy);
    // A host grant not requested by the provider cannot register the facet.
    CHECK(on1x_sdk_registry_register_v2(registry.get(), &extension, nullptr) == ON1X_SDK_CAPABILITY_DENIED);
    extension.requested_capabilities = &request;
    extension.requested_capability_count = 1;
    fail = true;
    const auto revision = on1x_sdk_registry_revision(registry.get());
    CHECK(on1x_sdk_registry_register_v2(registry.get(), &extension, nullptr) == ON1X_SDK_ERROR);
    CHECK(on1x_sdk_registry_revision(registry.get()) == revision);
    fail = false;
    CHECK(on1x_sdk_registry_register_v2(registry.get(), &extension, nullptr) == ON1X_SDK_OK);
    CHECK(on1x_sdk_registry_revision(registry.get()) > revision);
    CHECK(on1x_sdk_registry_close(registry.get()) == ON1X_SDK_OK);
}
} // namespace

int main() {
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    Registry registry(raw_registry, on1x_sdk_registry_destroy);

    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x_sdk_program_create_source(nullptr, 0, &raw_program) == ON1X_SDK_OK);
    Program empty(raw_program, on1x_sdk_program_destroy);
    const uint8_t *view = expected;
    size_t size = sizeof expected;
    CHECK(on1x_sdk_program_bytes(empty.get(), &view, &size) == ON1X_SDK_OK);
    CHECK(view == nullptr && size == 0);
    CHECK(on1x_sdk_program_bytes(empty.get(), nullptr, &size) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_program_phase(nullptr, nullptr) == ON1X_SDK_INVALID_ARGUMENT);

    CHECK(on1x_sdk_program_create_source(nullptr, 1, &raw_program) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(raw_program == nullptr);
    CHECK(on1x_sdk_program_create_source(expected, SIZE_MAX, &raw_program) == ON1X_SDK_LIMIT);
    CHECK(raw_program == nullptr);
    CHECK(on1x_sdk_program_create_source(expected, sizeof expected, nullptr) == ON1X_SDK_INVALID_ARGUMENT);

    uint8_t local[] = {'a', 0, 'b'};
    CHECK(on1x_sdk_program_create_source(local, sizeof local, &raw_program) == ON1X_SDK_OK);
    Program program(raw_program, on1x_sdk_program_destroy);
    std::memset(local, 'x', sizeof local);

    State state;
    char name[] = "lint";
    On1xSDK_Analyzer descriptor{ON1X_SDK_ABI_VERSION, sizeof descriptor, name,
                                ON1X_SDK_SOURCE, analyze, &state};
    auto malformed = descriptor;
    malformed.abi_version = 0;
    CHECK(on1x_sdk_analyzer_register(registry.get(), &malformed) == ON1X_SDK_ABI_MISMATCH);
    malformed = descriptor;
    malformed.struct_size = sizeof descriptor - 1;
    CHECK(on1x_sdk_analyzer_register(registry.get(), &malformed) == ON1X_SDK_ABI_MISMATCH);
    alignas(On1xSDK_Analyzer) const uint32_t header[2] = {ON1X_SDK_ABI_VERSION, sizeof header};
    CHECK(on1x_sdk_analyzer_register(registry.get(), reinterpret_cast<const On1xSDK_Analyzer *>(header))
          == ON1X_SDK_ABI_MISMATCH);
    malformed = descriptor;
    malformed.phase = ON1X_SDK_CHECKED_HIR;
    CHECK(on1x_sdk_analyzer_register(registry.get(), &malformed) == ON1X_SDK_UNSUPPORTED);
    malformed = descriptor;
    malformed.run = nullptr;
    CHECK(on1x_sdk_analyzer_register(registry.get(), &malformed) == ON1X_SDK_INVALID_ARGUMENT);
    std::array<char, ON1X_SDK_NAME_MAX + 1> overlong_name;
    overlong_name.fill('n');  // No NUL within the allowed window.
    malformed = descriptor;
    malformed.name = overlong_name.data();
    CHECK(on1x_sdk_analyzer_register(registry.get(), &malformed) == ON1X_SDK_LIMIT);

    CHECK(on1x_sdk_analyzer_register(registry.get(), &descriptor) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analyzer_register(registry.get(), &descriptor) == ON1X_SDK_DUPLICATE);
    name[0] = '!';
    descriptor.run = nullptr;
    descriptor.userdata = nullptr;

    const On1xSDK_Analyzer optional_mir{ON1X_SDK_ABI_VERSION, sizeof optional_mir, "mir",
                                      ON1X_SDK_VERIFIED_MIR, analyze, &state};
    CHECK(on1x_sdk_analyzer_register(registry.get(), &optional_mir) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "mir", program.get(), nullptr) == ON1X_SDK_UNSUPPORTED);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "absent", program.get(), nullptr) == ON1X_SDK_UNSUPPORTED);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "../lint", program.get(), nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(state.calls == 0);

    size_t diagnostics = 0;
    const On1xSDK_DiagnosticSink sink{collect, &diagnostics};
    CHECK(on1x_sdk_analyzer_run(registry.get(), "lint", program.get(), &sink) == ON1X_SDK_OK);
    CHECK(state.calls == 1 && diagnostics == 2);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "lint", program.get(), nullptr) == ON1X_SDK_OK);
    CHECK(state.calls == 2 && diagnostics == 2);

    for (const auto &[mode, status] : {
            std::pair{Mode::fail, ON1X_SDK_UNSUPPORTED},
            std::pair{Mode::exception, ON1X_SDK_ERROR},
            std::pair{Mode::allocation, ON1X_SDK_LIMIT},
            std::pair{Mode::invalid, ON1X_SDK_INVALID_ARGUMENT},
            std::pair{Mode::bad_code, ON1X_SDK_INVALID_ARGUMENT},
            std::pair{Mode::bad_severity, ON1X_SDK_INVALID_ARGUMENT},
            std::pair{Mode::too_many, ON1X_SDK_LIMIT},
            std::pair{Mode::too_long, ON1X_SDK_LIMIT}}) {
        const auto calls_before = state.calls;
        state.mode = mode;
        diagnostics = 0;
        CHECK(on1x_sdk_analyzer_run(registry.get(), "lint", program.get(), &sink) == status);
        CHECK(state.calls == calls_before + 1 && diagnostics == 0);
    }
    CHECK(on1x_sdk_analyzer_emit(nullptr, nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    registration_v2(registry.get());
}

#include "On1xSDK-Analyzer.h"
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
}

#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

namespace {
using Registry = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;
using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;
using Artifact = std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>;

// Canonical --emit-ir wire from `fn main() -> i64 { return 42; }` with filename
// normalized to main.on1x. This tests the SDK host wrapper, not MIR verification.
constexpr std::string_view mir = R"((on1x.mir 1 1 ((function 2 "main" i64 (at "main.on1x" 0 1 1) () ((block 3 4 (at "main.on1x" 0 1 1) () ((instruction 5 const i64 () 42 0 0 0 (at "main.on1x" 26 1 27))) (exit 6 return 5 4 () (at "main.on1x" 19 1 20)))))))"
                                 "\n";

enum class Mode { good, zero, fail, exception, overrun, invalid };
struct State {
    On1xSDK_Phase expected_phase;
    size_t calls = 0;
    Mode mode = Mode::good;
};

extern "C" On1xSDK_Status produce(void *userdata, const On1xSDK_Program *program,
                                  On1xSDK_OutputSink *output) {
    auto &state = *static_cast<State *>(userdata);
    ++state.calls;
    On1xSDK_Phase phase{};
    CHECK(on1x_sdk_program_phase(program, &phase) == ON1X_SDK_OK && phase == state.expected_phase);
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_program_bytes(program, &bytes, &size) == ON1X_SDK_OK);
    const std::string_view text(reinterpret_cast<const char *>(bytes), size);
    CHECK(text == (phase == ON1X_SDK_VERIFIED_MIR ? mir : "on1x source"));

    CHECK(on1x_sdk_output_write(output, nullptr, 0) == ON1X_SDK_OK);
    if (state.mode == Mode::zero) return ON1X_SDK_OK;
    char prefix[] = {'A', '\0'};
    CHECK(on1x_sdk_output_write(output, prefix, sizeof prefix) == ON1X_SDK_OK);
    prefix[0] = 'Z'; // The sink must have copied the first chunk.
    switch (state.mode) {
    case Mode::good:
        return on1x_sdk_output_write(output, "B", 1);
    case Mode::fail:
        return ON1X_SDK_UNSUPPORTED;
    case Mode::exception:
        throw 1;
    case Mode::overrun:
        CHECK(on1x_sdk_output_write(output, "B", SIZE_MAX) == ON1X_SDK_LIMIT);
        CHECK(on1x_sdk_output_write(output, "B", 1) == ON1X_SDK_LIMIT);
        return ON1X_SDK_OK;
    case Mode::invalid:
        CHECK(on1x_sdk_output_write(output, nullptr, 1) == ON1X_SDK_INVALID_ARGUMENT);
        return ON1X_SDK_OK;
    case Mode::zero:
        break;
    }
    std::abort();
}

extern "C" On1xSDK_Status inspect_mir(void *userdata, const On1xSDK_Program *program,
                                      On1xSDK_AnalyzerSink *) {
    auto &calls = *static_cast<unsigned *>(userdata);
    ++calls;
    On1xSDK_Phase phase{};
    CHECK(on1x_sdk_program_phase(program, &phase) == ON1X_SDK_OK && phase == ON1X_SDK_VERIFIED_MIR);
    return ON1X_SDK_OK;
}
} // namespace

int main() {
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    Registry registry(raw_registry, on1x_sdk_registry_destroy);

    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x_sdk_program_create_source("on1x source", 11, &raw_program) == ON1X_SDK_OK);
    Program source(raw_program, on1x_sdk_program_destroy);
    CHECK(on1x_sdk_program_create_source(mir.data(), mir.size(), &raw_program) == ON1X_SDK_OK);
    Program unverified(raw_program, on1x_sdk_program_destroy);
    std::string wire(mir);
    CHECK(on1x::sdk::make_verified_mir(wire, &raw_program) == ON1X_SDK_OK);
    Program verified(raw_program, on1x_sdk_program_destroy);
    wire.assign("modified");

    State backend_state{ON1X_SDK_VERIFIED_MIR};
    char name[] = "output";
    struct {
        On1xSDK_Backend descriptor;
        uint32_t tail;
    } extended{{ON1X_SDK_ABI_VERSION, sizeof extended, name,
                ON1X_SDK_VERIFIED_MIR, produce, &backend_state}, 0};
    auto malformed = extended.descriptor;
    malformed.phase = ON1X_SDK_SOURCE;
    CHECK(on1x_sdk_backend_register(registry.get(), &malformed) == ON1X_SDK_UNSUPPORTED);
    malformed = extended.descriptor;
    malformed.struct_size = sizeof malformed - 1;
    CHECK(on1x_sdk_backend_register(registry.get(), &malformed) == ON1X_SDK_ABI_MISMATCH);
    CHECK(on1x_sdk_backend_register(registry.get(), &extended.descriptor) == ON1X_SDK_OK);
    CHECK(on1x_sdk_backend_register(registry.get(), &extended.descriptor) == ON1X_SDK_DUPLICATE);
    name[0] = '!';
    extended.descriptor.run = nullptr;
    extended.descriptor.userdata = nullptr;

    State transformer_state{ON1X_SDK_SOURCE};
    On1xSDK_Transformer transformer{ON1X_SDK_ABI_VERSION, sizeof transformer, "output",
                                   ON1X_SDK_VERIFIED_MIR, produce, &transformer_state};
    CHECK(on1x_sdk_transformer_register(registry.get(), &transformer) == ON1X_SDK_UNSUPPORTED);
    transformer.phase = ON1X_SDK_SOURCE;
    CHECK(on1x_sdk_transformer_register(registry.get(), &transformer) == ON1X_SDK_OK);
    CHECK(on1x_sdk_transformer_register(registry.get(), &transformer) == ON1X_SDK_DUPLICATE);

    unsigned mir_calls = 0;
    const On1xSDK_Analyzer analyzer{ON1X_SDK_ABI_VERSION, sizeof analyzer, "output",
                                  ON1X_SDK_VERIFIED_MIR, inspect_mir, &mir_calls};
    CHECK(on1x_sdk_analyzer_register(registry.get(), &analyzer) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "output", unverified.get(), nullptr) == ON1X_SDK_UNSUPPORTED);
    CHECK(mir_calls == 0);
    CHECK(on1x_sdk_analyzer_run(registry.get(), "output", verified.get(), nullptr) == ON1X_SDK_OK);
    CHECK(mir_calls == 1);

    On1xSDK_Artifact *raw_artifact = nullptr;
    CHECK(on1x_sdk_backend_run(registry.get(), "output", unverified.get(), 3, &raw_artifact) == ON1X_SDK_UNSUPPORTED);
    CHECK(raw_artifact == nullptr);
    CHECK(on1x_sdk_backend_run(registry.get(), "unknown", verified.get(), 3, &raw_artifact) == ON1X_SDK_UNSUPPORTED);
    CHECK(on1x_sdk_backend_run(registry.get(), "output", verified.get(), SIZE_MAX, &raw_artifact) == ON1X_SDK_LIMIT);
    CHECK(on1x_sdk_backend_run(registry.get(), "output", verified.get(), 3, nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(backend_state.calls == 0);
    CHECK(on1x_sdk_backend_run(registry.get(), "output", verified.get(), 3, &raw_artifact) == ON1X_SDK_OK);
    Artifact artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const uint8_t *bytes = nullptr;
    size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == 3 && std::memcmp(bytes, "A\0B", size) == 0);
    CHECK(on1x_sdk_artifact_bytes(nullptr, &bytes, &size) == ON1X_SDK_INVALID_ARGUMENT);

    CHECK(on1x_sdk_transformer_run(registry.get(), "output", verified.get(), 3, &raw_program) == ON1X_SDK_UNSUPPORTED);
    CHECK(raw_program == nullptr);
    CHECK(on1x_sdk_transformer_run(registry.get(), "output", source.get(), SIZE_MAX, &raw_program) == ON1X_SDK_LIMIT);
    CHECK(transformer_state.calls == 0);
    CHECK(on1x_sdk_transformer_run(registry.get(), "output", source.get(), 3, &raw_program) == ON1X_SDK_OK);
    Program replacement(raw_program, on1x_sdk_program_destroy);
    On1xSDK_Phase phase{};
    CHECK(on1x_sdk_program_phase(replacement.get(), &phase) == ON1X_SDK_OK && phase == ON1X_SDK_SOURCE);
    CHECK(on1x_sdk_program_bytes(replacement.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == 3 && std::memcmp(bytes, "A\0B", size) == 0);
    CHECK(on1x_sdk_program_bytes(source.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == 11 && std::memcmp(bytes, "on1x source", size) == 0);

    for (const auto &[mode, status] : {
            std::pair{Mode::fail, ON1X_SDK_UNSUPPORTED},
            std::pair{Mode::exception, ON1X_SDK_ERROR},
            std::pair{Mode::overrun, ON1X_SDK_LIMIT},
            std::pair{Mode::invalid, ON1X_SDK_INVALID_ARGUMENT}}) {
        backend_state.mode = transformer_state.mode = mode;
        raw_artifact = artifact.get();
        CHECK(on1x_sdk_backend_run(registry.get(), "output", verified.get(), 2, &raw_artifact) == status);
        CHECK(raw_artifact == nullptr);
        raw_program = replacement.get();
        CHECK(on1x_sdk_transformer_run(registry.get(), "output", source.get(), 2, &raw_program) == status);
        CHECK(raw_program == nullptr);
    }

    backend_state.mode = transformer_state.mode = Mode::zero;
    CHECK(on1x_sdk_backend_run(registry.get(), "output", verified.get(), 0, &raw_artifact) == ON1X_SDK_OK);
    Artifact empty_artifact(raw_artifact, on1x_sdk_artifact_destroy);
    CHECK(on1x_sdk_artifact_bytes(empty_artifact.get(), &bytes, &size) == ON1X_SDK_OK && !bytes && size == 0);
    CHECK(on1x_sdk_transformer_run(registry.get(), "output", source.get(), 0, &raw_program) == ON1X_SDK_OK);
    Program empty_replacement(raw_program, on1x_sdk_program_destroy);
    CHECK(on1x_sdk_program_bytes(empty_replacement.get(), &bytes, &size) == ON1X_SDK_OK && !bytes && size == 0);
    CHECK(on1x_sdk_output_write(nullptr, nullptr, 0) == ON1X_SDK_INVALID_ARGUMENT);

    // Output lifetime is independent of the registry and verified input.
    registry.reset();
    verified.reset();
    source.reset();
    CHECK(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == 3 && std::memcmp(bytes, "A\0B", size) == 0);
    CHECK(on1x_sdk_program_bytes(replacement.get(), &bytes, &size) == ON1X_SDK_OK);
    CHECK(size == 3 && std::memcmp(bytes, "A\0B", size) == 0);
}

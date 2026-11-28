#include "Builtin.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-IRSerialize.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

extern "C" On1xSDK_Status on1x_builtin_mir_c11_register(
    On1xSDK_Registry *, on1x::backends::Context *);

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {
namespace mir = on1x::mir;
using Snapshot = std::unique_ptr<On1xSDK_MIRSnapshot, decltype(&on1x_sdk_mir_snapshot_release)>;
using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;
using Target = std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>;

constexpr std::string_view triple = "x86_64-unknown-linux-gnu";

Snapshot decode(std::string_view wire, On1xSDK_IRWireFormat format,
                const On1xSDK_IRDecodeOptions &options) {
    On1xSDK_MIRSnapshot *out = nullptr;
    CHECK(on1x_sdk_mir_deserialize(wire.data(), wire.size(), format, &options,
                                   &out, nullptr, nullptr) == ON1X_SDK_OK);
    return {out, on1x_sdk_mir_snapshot_release};
}

std::string_view view(On1xSDK_IRStringView value) {
    return {value.data, value.size};
}

std::string encode(const Snapshot &snapshot, On1xSDK_IRWireFormat format) {
    On1xSDK_IRBlob *out = nullptr;
    CHECK(on1x_sdk_mir_serialize(snapshot.get(), format, &out) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_IRBlob, decltype(&on1x_sdk_ir_blob_release)>
        blob(out, on1x_sdk_ir_blob_release);
    On1xSDK_IRBytesView bytes{};
    CHECK(on1x_sdk_ir_blob_bytes(blob.get(), &bytes) == ON1X_SDK_OK);
    return {reinterpret_cast<const char *>(bytes.data), bytes.size};
}

void inspect(const mir::Module &plain, const mir::Module &portable) {
    On1xSDK_ContextConfig config{};
    config.abi_version = ON1X_SDK_ABI_VERSION_V2;
    config.struct_size = sizeof config;
    On1xSDK_Context *owner = nullptr;
    CHECK(on1x_sdk_context_create(&config, &owner) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Context, decltype(&on1x_sdk_context_release)>
        context(owner, on1x_sdk_context_release);
    On1xSDK_IRDecodeOptions options{};
    options.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION;
    options.struct_size = sizeof options;
    options.identity = {
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof options.identity, owner, 1, 1,
        ON1X_SDK_IR_CANDIDATE, ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION
    };
    const auto canonical = ON1X_SDK_IR_WIRE_MIR_CANONICAL;
    std::size_t count = 42;
    for (const auto version : {1u, 2u, 3u}) {
        auto legacy = plain;
        legacy.version = version;
        const auto snapshot = decode(mir::serialize(legacy), canonical, options);
        std::uint32_t reported = 0;
        CHECK(on1x_sdk_mir_snapshot_format_version(snapshot.get(), &reported) == ON1X_SDK_OK);
        CHECK(reported == version);
        CHECK(on1x_sdk_mir_snapshot_directive_count(snapshot.get(), &count) == ON1X_SDK_OK);
        CHECK(count == 0);
    }
    const auto wire = mir::serialize(portable);
    const auto snapshot = decode(wire, canonical, options);
    CHECK(encode(snapshot, canonical) == wire);
    const auto envelope = ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1;
    const auto blob = encode(snapshot, envelope);
    const auto roundtrip = decode(blob, envelope, options);
    CHECK(encode(roundtrip, canonical) == wire);
    CHECK(encode(roundtrip, envelope) == blob);

    std::uint32_t version = 0;
    CHECK(on1x_sdk_mir_snapshot_format_version(roundtrip.get(), &version) == ON1X_SDK_OK);
    CHECK(version == 4);
    CHECK(on1x_sdk_mir_snapshot_directive_count(nullptr, &count) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_mir_snapshot_directive_count(roundtrip.get(), nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_mir_snapshot_directive_count(roundtrip.get(), &count) == ON1X_SDK_OK);
    CHECK(count == 2);
    CHECK(on1x_sdk_mir_snapshot_directive(roundtrip.get(), 0, nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    On1xSDK_MIRDirectiveView entry{};
    entry.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION + 1;
    entry.struct_size = sizeof entry;
    CHECK(on1x_sdk_mir_snapshot_directive(roundtrip.get(), 0, &entry) == ON1X_SDK_ABI_MISMATCH);
    entry.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION;
    --entry.struct_size;
    CHECK(on1x_sdk_mir_snapshot_directive(roundtrip.get(), 0, &entry) == ON1X_SDK_ABI_MISMATCH);
    entry.struct_size = sizeof entry;
    CHECK(on1x_sdk_mir_snapshot_directive(roundtrip.get(), count, &entry) == ON1X_SDK_INVALID_ARGUMENT);
    for (std::size_t index = 0; index < count; ++index) {
        CHECK(on1x_sdk_mir_snapshot_directive(roundtrip.get(), index, &entry) == ON1X_SDK_OK);
        const auto &expected = portable.directives[index];
        CHECK(view(entry.backend) == expected.backend && view(entry.target) == expected.target);
        CHECK(view(entry.kind) == expected.kind && view(entry.action) == expected.action);
        CHECK(entry.function == expected.function && entry.speculative == expected.speculative);
        CHECK(entry.reserved == 0);
    }

    On1xSDK_IRIdentity identity{};
    CHECK(on1x_sdk_mir_snapshot_identity(roundtrip.get(), &identity) == ON1X_SDK_OK);
    CHECK(identity.state == ON1X_SDK_IR_CANDIDATE);
    const On1xSDK_MIREvaluateOptions evaluate{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof(On1xSDK_MIREvaluateOptions),
        {"main", 4}, nullptr, 0, 1000, 1
    };
    On1xSDK_MIREvaluation *result = nullptr;
    CHECK(on1x_sdk_mir_evaluate(roundtrip.get(), &evaluate, &result) == ON1X_SDK_UNSUPPORTED);
    CHECK(!result);

    auto future = wire;
    future[std::string_view("(on1x.mir ").size()] = '5';
    On1xSDK_MIRSnapshot *invalid = nullptr;
    CHECK(on1x_sdk_mir_deserialize(future.data(), future.size(), canonical, &options,
                                   &invalid, nullptr, nullptr) == ON1X_SDK_UNSUPPORTED);
    CHECK(!invalid);
}

Target target(std::string_view name) {
    const On1xSDK_TargetDescriptor descriptor{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
        name.data(), name.size(), nullptr, 0,
        {ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
         ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, 128},
        ON1X_SDK_OBJECT_FORMAT_ELF, ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64, nullptr, 0
    };
    On1xSDK_Target *out = nullptr;
    CHECK(on1x_sdk_target_create(&descriptor, &out) == ON1X_SDK_OK);
    return {out, on1x_sdk_target_destroy};
}

On1xSDK_Status observe(void *data, const On1xSDK_BackendInvocation *, On1xSDK_OutputSink *) {
    ++*static_cast<unsigned *>(data);
    return ON1X_SDK_OK;
}

void backend(const mir::Module &plain, const mir::Module &portable) {
    using on1x::backends::run_mir;
    const auto baseline = run_mir(plain, "mir-c11").bytes;
    const auto wire = mir::serialize(portable);
    CHECK(run_mir(portable, "mir-text", triple, true).bytes == wire);
    CHECK(run_mir(portable, "mir-c11").bytes == baseline);
    CHECK(run_mir(portable, "mir-c11", "x86_64-linux-gnu").bytes == baseline);
    auto wrong_backend = portable;
    wrong_backend.directives[0].backend = "mir-eval";
    CHECK(run_mir(wrong_backend, "mir-c11", triple).bytes == baseline);
    auto speculative = portable;
    speculative.directives[0].speculative = true;
    CHECK(run_mir(speculative, "mir-c11", triple).bytes == baseline);
    const auto selected = run_mir(portable, "mir-c11", triple).bytes;
    CHECK(selected != baseline);
    CHECK(run_mir(speculative, "mir-c11", triple, true).bytes == selected);
    const auto evaluation = run_mir(portable, "mir-eval", triple);
    CHECK(evaluation.exit_code == 0 && evaluation.diagnostic.empty() && evaluation.bytes == "42\n");
    auto hints_only = portable;
    hints_only.directives.erase(hints_only.directives.begin());
    CHECK(run_mir(hints_only, "mir-c11", triple, true).bytes == baseline);
    hints_only.directives.front() = {
        "mir-c11", std::string(triple), "hint", "private-code", plain.functions.front().id, false
    };
    CHECK(run_mir(hints_only, "mir-c11").bytes == baseline);
    CHECK(run_mir(hints_only, "mir-c11", triple).bytes.find(
        "ON1X_MIR_PRIVATE int64_t on1x_mir_f" + std::to_string(plain.functions.front().id)) !=
        std::string::npos);

    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    on1x::backends::Context context;
    CHECK(on1x_builtin_mir_c11_register(registry.get(), &context) == ON1X_SDK_OK);
    On1xSDK_Program *out_program = nullptr;
    CHECK(on1x::sdk::make_verified_mir(wire, &out_program) == ON1X_SDK_OK);
    const Program program(out_program, on1x_sdk_program_destroy);
    const auto selected_target = target(triple);
    On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program.get(), selected_target.get(), ON1X_SDK_ARTIFACT_C11_SOURCE, 4,
        "mir-c11", 7, ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY, 0,
        ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr
    };
    const auto produce = [&] {
        On1xSDK_Artifact *out = nullptr;
        CHECK(on1x_sdk_backend_produce(registry.get(), &request, &out) == ON1X_SDK_OK);
        const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
            artifact(out, on1x_sdk_artifact_destroy);
        const std::uint8_t *data = nullptr;
        std::size_t size = 0;
        CHECK(on1x_sdk_artifact_bytes(artifact.get(), &data, &size) == ON1X_SDK_OK);
        return std::string(reinterpret_cast<const char *>(data), size);
    };
    CHECK(produce() == selected);
    const auto speculative_wire = mir::serialize(speculative);
    CHECK(on1x::sdk::make_verified_mir(speculative_wire, &out_program) == ON1X_SDK_OK);
    const Program speculative_program(out_program, on1x_sdk_program_destroy);
    request.program = speculative_program.get();
    CHECK(produce() == baseline);
    request.program = program.get();
    request.target = nullptr;
    CHECK(produce() == baseline);

    // Naming a provider in a hint does not activate it or admit private MIR.
    unsigned calls = 0;
    On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 4, 4, ON1X_SDK_ARTIFACT_C11_SOURCE,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0
    };
    const On1xSDK_BackendProvider external{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "hinted-provider", 15, -1, 0, &capability, 1, observe, &calls
    };
    CHECK(on1x_sdk_backend_provider_register(registry.get(), &external) == ON1X_SDK_OK);
    request.target = selected_target.get();
    request.provider_name = nullptr;
    request.provider_name_size = 0;
    CHECK(produce() == selected);
    CHECK(calls == 0);
    CHECK(on1x::sdk::make_builtin_mir(portable, &out_program) == ON1X_SDK_OK);
    const Program private_program(out_program, on1x_sdk_program_destroy);
    request.program = private_program.get();
    request.provider_name = external.name;
    request.provider_name_size = external.name_size;
    On1xSDK_Artifact *denied = nullptr;
    CHECK(on1x_sdk_backend_produce(registry.get(), &request, &denied) == ON1X_SDK_CAPABILITY_DENIED);
    CHECK(!denied && calls == 0);
}

#ifdef ON1X_TEST_MIR_ARCHON
void native_backend(const mir::Module &plain, mir::Module portable) {
    using on1x::backends::run_mir_native;
    const auto baseline = run_mir_native(plain, triple).bytes;
    CHECK(!baseline.empty());
    CHECK(run_mir_native(portable, triple).bytes == baseline);
    portable.directives.front().backend = "mir-archon";
    portable.directives.front().target = "x86_64-linux-gnu";
    CHECK(run_mir_native(portable, triple).bytes == baseline);
    portable.directives.front().target = triple;
    const auto selected = run_mir_native(portable, triple).bytes;
    CHECK(!selected.empty() && selected != baseline);
    portable.directives.front().speculative = true;
    CHECK(run_mir_native(portable, triple).bytes == baseline);
    on1x::backends::NativeOptions options;
    options.speculate = true;
    CHECK(run_mir_native(portable, triple, options).bytes == selected);
}
#endif
} // namespace

int main() {
    // Raw canonical input keeps the identity instruction observable independently
    // of source-lowering hints and optimizations.
    const auto plain = mir::deserialize(R"(
        (on1x.mir 1 1
          ((function 2 "identity" i64 (at "sdk.on1x" 0 1 1) ()
            ((block 3 4 (at "sdk.on1x" 0 1 1)
              ((value 5 i64 (at "sdk.on1x" 0 1 1)))
              ((instruction 6 const i64 () 0 0 0 0 (at "sdk.on1x" 0 1 1))
               (instruction 7 add.checked i64 (5 6) 0 0 4 8 (at "sdk.on1x" 0 1 1)))
              (exit 9 return 7 8 () (at "sdk.on1x" 0 1 1)))))
           (function 10 "main" void (at "sdk.on1x" 0 1 1) ()
            ((block 11 12 (at "sdk.on1x" 0 1 1) ()
              ((instruction 13 const i64 () 42 0 0 0 (at "sdk.on1x" 0 1 1))
               (instruction 14 call i64 (13) 0 2 12 15 (at "sdk.on1x" 0 1 1))
               (instruction 16 print void (14) 0 0 15 17 (at "sdk.on1x" 0 1 1)))
              (exit 18 return 0 17 () (at "sdk.on1x" 0 1 1)))))))
    )");
    auto portable = plain;
    portable.version = mir::directive_format_version;
    portable.directives = {
        {"mir-c11", std::string(triple), "rewrite", "fold-identity", portable.functions.front().id, false},
        {"hinted-provider", "", "hint", "kernel-hot", 0, true}
    };
    inspect(plain, portable);
    backend(plain, portable);
#ifdef ON1X_TEST_MIR_ARCHON
    native_backend(plain, portable);
#endif
}

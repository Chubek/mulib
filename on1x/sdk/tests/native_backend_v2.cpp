#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"
#include "ProgramInternal.hpp"
#include "backend/BackendProviderInternal.hpp"
#include <on1x/backend/mir.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

extern "C" On1xSDK_Status on1x_builtin_mir_archon_register(
    On1xSDK_Registry *, on1x::backends::Context *);

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    std::abort(); \
} } while (false)

namespace {
using Registry = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;
using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;
using Target = std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>;
using Artifact = std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>;

constexpr std::string_view triple = ON1X_ARCHON_TARGET_TRIPLE;
constexpr std::string_view wire =
    "(on1x.mir 1 1 ((function 2 \"main\" i64 (at \"main.on1x\" 0 1 1) () "
    "((block 3 4 (at \"main.on1x\" 0 1 1) () "
    "((instruction 5 const i64 () 42 0 0 0 (at \"main.on1x\" 26 1 27))) "
    "(exit 6 return 5 4 () (at \"main.on1x\" 19 1 20)))))))\n";

Target target(std::string_view name = triple, unsigned stack = 128,
              const On1xSDK_TargetFeature *features = nullptr, std::size_t count = 0) {
    const On1xSDK_DataLayout layout{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
        ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, stack};
    const auto calling = name.starts_with("x86_64-")
        ? ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64
        : name.starts_with("aarch64-") ? ON1X_SDK_CALLING_CONVENTION_AAPCS64
                                     : ON1X_SDK_CALLING_CONVENTION_RISCV_LP64;
    const On1xSDK_TargetDescriptor descriptor{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
        name.data(), name.size(), nullptr, 0, layout, ON1X_SDK_OBJECT_FORMAT_ELF,
        calling, features, count};
    On1xSDK_Target *raw = nullptr;
    CHECK(on1x_sdk_target_create(&descriptor, &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_target_destroy};
}

struct Poll {
    std::size_t calls = 0, stop = 0;
    On1xSDK_Status status = ON1X_SDK_CANCELLED;
};
On1xSDK_Status poll(void *userdata) {
    auto &state = *static_cast<Poll *>(userdata);
    // Deliberately non-sticky: the adapter must preserve the first failure.
    return ++state.calls == state.stop ? state.status : ON1X_SDK_OK;
}

struct Probe { unsigned calls = 0; };
On1xSDK_Status reject_after_write(void *userdata,
                                 const On1xSDK_BackendInvocation *,
                                 On1xSDK_OutputSink *sink) {
    ++static_cast<Probe *>(userdata)->calls;
    (void)on1x_sdk_output_write(sink, "partial", 7);
    return ON1X_SDK_UNSUPPORTED;
}

std::string bytes(const On1xSDK_Artifact *artifact) {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    CHECK(on1x_sdk_artifact_bytes(artifact, &data, &size) == ON1X_SDK_OK);
    return size ? std::string(reinterpret_cast<const char *>(data), size) : std::string{};
}
} // namespace

int main(int argc, char **argv) {
    CHECK(argc == 1 || argc == 2);
    On1xSDK_Registry *raw_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_registry) == ON1X_SDK_OK);
    Registry registry(raw_registry, on1x_sdk_registry_destroy);
    on1x::backends::Context context;
    CHECK(on1x_builtin_mir_archon_register(registry.get(), &context) == ON1X_SDK_OK);

    // A public enum, a copied built-in name, and even a matching native target
    // cannot grant compiler-linked authority.
    const On1xSDK_BackendTarget exact{triple.data(), triple.size()};
    On1xSDK_BackendCapability cap{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 1, 3, ON1X_SDK_ARTIFACT_ASSEMBLY,
        ON1X_SDK_BACKEND_DETERMINISTIC, &exact, 1, nullptr, 0, nullptr, 0};
    Probe probe;
    On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-archon", 10, 0, 0, &cap, 1, reject_after_write, &probe};
    for (auto kind : {ON1X_SDK_ARTIFACT_ASSEMBLY, ON1X_SDK_ARTIFACT_OBJECT,
                      ON1X_SDK_ARTIFACT_EXECUTABLE}) {
        cap.artifact_kind = kind;
        CHECK(on1x_sdk_backend_provider_register(registry.get(), &provider) ==
              ON1X_SDK_UNSUPPORTED);
        if (kind != ON1X_SDK_ARTIFACT_ASSEMBLY)
            CHECK(on1x::sdk::register_builtin_backend_provider(registry.get(), &provider) ==
                  ON1X_SDK_UNSUPPORTED);
    }
    cap.artifact_kind = ON1X_SDK_ARTIFACT_ASSEMBLY;
    cap.flags |= ON1X_SDK_BACKEND_TARGET_INDEPENDENT;
    CHECK(on1x::sdk::register_builtin_backend_provider(registry.get(), &provider) ==
          ON1X_SDK_UNSUPPORTED);
    cap.flags = ON1X_SDK_BACKEND_DETERMINISTIC;
    cap.input_schema_min = 0;
    CHECK(on1x::sdk::register_builtin_backend_provider(registry.get(), &provider) ==
          ON1X_SDK_UNSUPPORTED);
    cap.input_phase = ON1X_SDK_CHECKED_HIR;
    cap.input_schema_max = 0;
    CHECK(on1x::sdk::register_builtin_backend_provider(registry.get(), &provider) ==
          ON1X_SDK_UNSUPPORTED);
    cap.input_phase = ON1X_SDK_VERIFIED_MIR;
    cap.input_schema_min = 1;
    cap.input_schema_max = 3;

    // Real canonical verifier, real Archon native backend, real assembly.
    auto module = on1x::mir::deserialize(wire);
    on1x::mir::verify(module);
    On1xSDK_Program *raw_program = nullptr;
    CHECK(on1x::sdk::make_builtin_mir(module, &raw_program) == ON1X_SDK_OK);
    Program program(raw_program, on1x_sdk_program_destroy);
    auto selected_target = target();
    On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program.get(), selected_target.get(), ON1X_SDK_ARTIFACT_ASSEMBLY,
        module.version, "mir-archon", 10,
        ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC | ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY,
        0, ON1X_SDK_MAX_ARTIFACT_BYTES, nullptr, nullptr};
    On1xSDK_Artifact *raw = nullptr;
    CHECK(on1x_sdk_backend_produce(registry.get(), &request, &raw) == ON1X_SDK_OK);
    Artifact assembly(raw, on1x_sdk_artifact_destroy);
    CHECK(!bytes(assembly.get()).empty());
    const On1xSDK_ArtifactInfo *info = nullptr;
    CHECK(on1x_sdk_artifact_info(assembly.get(), &info) == ON1X_SDK_OK);
    CHECK(info->kind == ON1X_SDK_ARTIFACT_ASSEMBLY);
    CHECK(info->state == ON1X_SDK_ARTIFACT_STAGED);
    CHECK(info->flags & ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED);
    CHECK(std::string_view(info->target_triple, info->target_triple_size) == triple);
    CHECK(std::string_view(info->provider_name, info->provider_name_size) == "mir-archon");
    if (argc == 2) {
        std::ofstream output(argv[1], std::ios::binary);
        output << bytes(assembly.get());
        CHECK(output.good());
    }
    auto fails = [&](On1xSDK_Status expected) {
        raw = assembly.get();
        const auto status = on1x_sdk_backend_produce(registry.get(), &request, &raw);
        if (status != expected)
            std::fprintf(stderr, "expected SDK status %d, got %d\n", expected, status);
        CHECK(status == expected);
        CHECK(raw == nullptr);
    };
    request.flags = 0;
    request.provider_name = nullptr;
    request.provider_name_size = 0;
    fails(ON1X_SDK_CAPABILITY_DENIED);
    request.provider_name = "mir-archon";
    request.provider_name_size = 10;
    request.target = nullptr;
    fails(ON1X_SDK_UNSUPPORTED);
    auto wrong_target = target(triple.starts_with("x86_64-")
        ? "aarch64-unknown-linux-gnu" : "x86_64-unknown-linux-gnu");
    request.target = wrong_target.get();
    fails(ON1X_SDK_UNSUPPORTED);
    auto wrong_layout = target(triple, 64);
    request.target = wrong_layout.get();
    fails(ON1X_SDK_UNSUPPORTED);
    request.target = selected_target.get();
    request.input_schema = 2;
    fails(ON1X_SDK_INVALID_ARGUMENT);
    request.input_schema = on1x::mir::directive_format_version;
    fails(ON1X_SDK_INVALID_ARGUMENT); // In range, but the private module is still v1.
    request.input_schema = on1x::mir::directive_format_version + 1;
    fails(ON1X_SDK_UNSUPPORTED);
    request.input_schema = module.version;
    request.artifact_kind = ON1X_SDK_ARTIFACT_OBJECT;
    fails(ON1X_SDK_UNSUPPORTED);
    request.artifact_kind = ON1X_SDK_ARTIFACT_ASSEMBLY;

    CHECK(on1x_sdk_program_create_source(wire.data(), wire.size(), &raw_program) == ON1X_SDK_OK);
    Program source(raw_program, on1x_sdk_program_destroy);
    request.program = source.get();
    fails(ON1X_SDK_UNSUPPORTED);
    request.program = program.get();
    // A host private wrapper is not an excuse for skipping canonical verification.
    module.functions.front().blocks.front().exit.value = 9999;
    fails(ON1X_SDK_VERIFICATION_FAILED);
    module = on1x::mir::deserialize(wire);
    context.error = {};

    // Both host-private and serialized verified handoffs produce the same bytes.
    CHECK(on1x::sdk::make_verified_mir(on1x::mir::serialize(module), &raw_program) == ON1X_SDK_OK);
    Program serialized(raw_program, on1x_sdk_program_destroy);
    request.program = serialized.get();
    request.input_schema = 0;
    CHECK(on1x_sdk_backend_produce(registry.get(), &request, &raw) == ON1X_SDK_OK);
    Artifact wire_assembly(raw, on1x_sdk_artifact_destroy);
    CHECK(bytes(wire_assembly.get()) == bytes(assembly.get()));
    request.program = program.get();
    request.input_schema = module.version;

    Poll cancellation;
    request.poll = poll;
    request.poll_userdata = &cancellation;
    CHECK(on1x_sdk_backend_produce(registry.get(), &request, &raw) == ON1X_SDK_OK);
    Artifact counted(raw, on1x_sdk_artifact_destroy);
    const auto polls = cancellation.calls;
    CHECK(polls >= 4);
    for (std::size_t stop = 1; stop <= polls; ++stop) {
        for (auto status : {ON1X_SDK_CANCELLED, ON1X_SDK_DEADLINE_EXCEEDED, ON1X_SDK_LIMIT}) {
            cancellation = {0, stop, status};
            fails(status);
        }
    }
    request.flags = ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY;
    cancellation = {0, polls + 1, ON1X_SDK_CANCELLED};
    fails(ON1X_SDK_CANCELLED); // Completed first generation is still only staged.
    request.flags = 0;
    request.poll = nullptr;
    request.poll_userdata = nullptr;
    request.byte_limit = 0;
    fails(ON1X_SDK_LIMIT);
    request.byte_limit = 1;
    fails(ON1X_SDK_LIMIT);
    request.byte_limit = ON1X_SDK_MAX_ARTIFACT_BYTES + 1u;
    fails(ON1X_SDK_LIMIT);
    request.byte_limit = ON1X_SDK_MAX_ARTIFACT_BYTES;

    // Target feature constraints still apply to trusted native capabilities.
    provider.name = "native-probe";
    provider.name_size = 12;
    const On1xSDK_TargetFeature required{"native_test_feature", 19};
    const On1xSDK_TargetFeature forbidden{"native_forbidden", 16};
    cap.required_features = &required;
    cap.required_feature_count = 1;
    cap.forbidden_features = &forbidden;
    cap.forbidden_feature_count = 1;
    CHECK(on1x::sdk::register_builtin_backend_provider(registry.get(), &provider) == ON1X_SDK_OK);
    request.provider_name = provider.name;
    request.provider_name_size = provider.name_size;
    fails(ON1X_SDK_UNSUPPORTED);
    CHECK(probe.calls == 0);
    const On1xSDK_TargetFeature both[]{required, forbidden};
    auto forbidden_target = target(triple, 128, both, 2);
    request.target = forbidden_target.get();
    fails(ON1X_SDK_UNSUPPORTED);
    CHECK(probe.calls == 0);
    auto allowed_target = target(triple, 128, &required, 1);
    request.target = allowed_target.get();
    fails(ON1X_SDK_UNSUPPORTED); // Callback writes a partial result then fails.
    CHECK(probe.calls == 1);
    request.byte_limit = 1;
    fails(ON1X_SDK_LIMIT); // Sticky sink failure takes precedence.
    CHECK(probe.calls == 2);
    request.byte_limit = ON1X_SDK_MAX_ARTIFACT_BYTES;

    provider.name = "external";
    provider.name_size = 8;
    cap.artifact_kind = ON1X_SDK_ARTIFACT_MIR_TEXT;
    CHECK(on1x_sdk_backend_provider_register(registry.get(), &provider) == ON1X_SDK_OK);
    request.provider_name = provider.name;
    request.provider_name_size = provider.name_size;
    request.artifact_kind = cap.artifact_kind;
    fails(ON1X_SDK_CAPABILITY_DENIED);
    CHECK(probe.calls == 2); // No private IR pointer crosses the extension boundary.

    // Legacy entrypoints cannot invoke the native target-bound capability.
    CHECK(on1x_sdk_backend_run(registry.get(), "mir-archon", serialized.get(),
                              ON1X_SDK_MAX_ARTIFACT_BYTES, &raw) == ON1X_SDK_UNSUPPORTED);
    CHECK(raw == nullptr);
}

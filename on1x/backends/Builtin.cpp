#include "Builtin.hpp"
#include "On1xSDK.h"
#include "ProgramInternal.hpp"

#include <memory>
#include <stdexcept>

extern "C" {
#define ON1X_BUILTIN_REGISTRAR(key, symbol) \
    On1xSDK_Status symbol(On1xSDK_Registry *, on1x::backends::Context *);
#include <BuiltinRegistrars.inc>
#undef ON1X_BUILTIN_REGISTRAR
}

namespace on1x::backends {
namespace {
using Register = On1xSDK_Status (*)(On1xSDK_Registry *, Context *);

struct Entry { std::string_view key; Register registrar; };
constexpr Entry registrars[]{
#define ON1X_BUILTIN_REGISTRAR(key, symbol) {key, symbol},
#include <BuiltinRegistrars.inc>
#undef ON1X_BUILTIN_REGISTRAR
};

Register registrar_for(std::string_view key)
{
    for (const auto &entry : registrars)
        if (entry.key == key) return entry.registrar;
    throw std::logic_error("unknown builtin backend: " + std::string(key));
}

void check(On1xSDK_Status status, const char *operation, const char *backend)
{
    if (status == ON1X_SDK_OK) return;
    throw std::runtime_error(std::string("builtin backend ") + backend + " " + operation +
                             ": " + on1x_sdk_status_name(status));
}

// Names, triples and feature names come from compiler-owned SDK descriptors,
// whose identifier grammars are ASCII. Escape JSON punctuation and controls.
void json_string(std::string &out, std::string_view text)
{
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (c < 0x20) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else {
            out += static_cast<char>(c);
        }
    }
    out += '"';
}

std::string_view artifact_name(On1xSDK_ArtifactKind kind)
{
    switch (kind) {
    case ON1X_SDK_ARTIFACT_C11_SOURCE: return "c11-source";
    case ON1X_SDK_ARTIFACT_MIR_TEXT: return "mir-text";
    case ON1X_SDK_ARTIFACT_CFG_DOT: return "cfg-dot";
    case ON1X_SDK_ARTIFACT_PROGRAM_OUTPUT: return "program-output";
    case ON1X_SDK_ARTIFACT_ASSEMBLY: return "assembly";
    default: throw std::logic_error("unsupported builtin backend report artifact kind");
    }
}

void describe_capability(std::string &out, const On1xSDK_BackendCapability &cap)
{
    switch (cap.input_phase) {
    case ON1X_SDK_CHECKED_HIR:
        out += R"({"phase":"checked-hir","representation":"on1x-hir","input_schema":null)";
        break;
    case ON1X_SDK_VERIFIED_MIR:
        out += R"({"phase":"verified-mir","representation":"on1x-mir","input_schema":{"min":)";
        out += std::to_string(cap.input_schema_min);
        out += R"(,"max":)";
        out += std::to_string(cap.input_schema_max);
        out += '}';
        break;
    default: throw std::logic_error("unknown builtin backend input phase");
    }
    out += R"(,"artifact":)";
    json_string(out, artifact_name(cap.artifact_kind));
    out += R"(,"target":{"kind":)";
    json_string(out, (cap.flags & ON1X_SDK_BACKEND_TARGET_INDEPENDENT) ? "independent" : "exact");
    out += R"(,"triples":[)";
    for (std::size_t i = 0; i < cap.target_count; ++i) {
        if (i) out += ',';
        json_string(out, {cap.targets[i].triple, cap.targets[i].triple_size});
    }
    const auto features = [&](const On1xSDK_TargetFeature *set, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            if (i) out += ',';
            json_string(out, {set[i].data, set[i].size});
        }
    };
    out += R"(],"required_features":[)";
    features(cap.required_features, cap.required_feature_count);
    out += R"(],"forbidden_features":[)";
    features(cap.forbidden_features, cap.forbidden_feature_count);
    out += R"(]},"declared_deterministic":)";
    out += (cap.flags & ON1X_SDK_BACKEND_DETERMINISTIC) ? "true" : "false";
    out += R"(,"admission":{"input":"not-checked","policy":)";
    json_string(out, cap.artifact_kind == ON1X_SDK_ARTIFACT_ASSEMBLY
        ? "explicit-trusted-native-assembly" : "compiler-linked");
    out += "}}";
}

struct Description {
    std::string &out;
    std::string_view key;
    bool provided = false;
};

void describe_provider(void *userdata, const On1xSDK_BackendProvider &provider,
                       bool prerequisite_satisfied)
{
    auto &description = *static_cast<Description *>(userdata);
    if (description.provided ||
        std::string_view(provider.name, provider.name_size) != description.key)
        throw std::logic_error("builtin backend registrar does not describe its linked key");
    description.provided = true;
    auto &out = description.out;
    out += R"({"name":)";
    json_string(out, description.key);
    out += R"(,"discovery":"linked-builtin","registration_prerequisite":)";
    json_string(out, prerequisite_satisfied ? "satisfied" : "unsupported");
    out += R"(,"build_link_readiness":"compiler-linked","external_build_link":"not-probed")";
    out += R"(,"execution_evidence":"not-collected","capabilities":[)";
    for (std::size_t i = 0; i < provider.capability_count; ++i) {
        if (i) out += ',';
        describe_capability(out, provider.capabilities[i]);
    }
    out += "]}";
}

template<class Prepare>
Product run(const char *name, Register registrar, Prepare prepare, Context context = {})
{
    On1xSDK_Program *raw_program = nullptr;
    check(prepare(&raw_program), "program preparation", name);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    On1xSDK_Registry *raw_registry = nullptr;
    check(on1x_sdk_registry_create(&raw_registry), "registry creation", name);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    check(registrar(registry.get(), &context), "registration", name);
    On1xSDK_Artifact *raw_artifact = nullptr;
    const auto status = sdk::run_builtin_backend(registry.get(), name, program.get(), &raw_artifact);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    if (context.error) std::rethrow_exception(context.error);
    check(status, "execution", name);
    const std::uint8_t *bytes = nullptr;
    std::size_t size = 0;
    check(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size), "artifact retrieval", name);
    return {size ? std::string(reinterpret_cast<const char *>(bytes), size) : std::string{},
            context.exit_code, std::move(context.diagnostic)};
}
}

std::string builtin_capabilities_json()
{
    std::string out = R"({"schema":"on1x.backend-capabilities/v1",)"
                      R"("scope":"compiler-linked-sdk-builtins","providers":[)";
    bool first = true;
    for (const auto &entry : registrars) {
        if (!first) out += ',';
        first = false;
        Description description{out, entry.key};
        Context context;
        context.describe_provider = describe_provider;
        context.describe_userdata = &description;
        const auto status = entry.registrar(nullptr, &context);
        if (status != ON1X_SDK_OK || !description.provided)
            throw std::logic_error("cannot describe builtin backend: " + std::string(entry.key));
    }
    out += "]}\n";
    return out;
}

Product run_c11(const Unit &unit, std::string_view source, const C11Options &options)
{
    Context context;
    context.c11 = options;
    return run("c11", registrar_for("c11"),
               [&](auto out) { return sdk::make_checked_hir(unit, source, out); }, std::move(context));
}

Product run_mir(const mir::Module &module, const char *backend,
                std::string_view target, bool speculate)
{
    Context context;
    context.mir_target = target;
    context.mir_speculate = speculate;
    return run(backend, registrar_for(backend),
               [&](auto out) { return sdk::make_builtin_mir(module, out); }, std::move(context));
}

Product run_mir_native(const mir::Module &module, std::string_view triple,
                       const NativeOptions &options)
{
    constexpr const char *name = "mir-archon";
    On1xSDK_CallingConvention convention;
    if (triple == "x86_64-unknown-linux-gnu")
        convention = ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64;
    else if (triple == "aarch64-unknown-linux-gnu")
        convention = ON1X_SDK_CALLING_CONVENTION_AAPCS64;
    else if (triple == "riscv64-unknown-linux-gnu")
        convention = ON1X_SDK_CALLING_CONVENTION_RISCV_LP64;
    else
        throw std::runtime_error("unsupported native target: " + std::string(triple));
    const On1xSDK_TargetDescriptor descriptor{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
        triple.data(), triple.size(), nullptr, 0,
        {ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
         ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, 128},
        ON1X_SDK_OBJECT_FORMAT_ELF, convention, nullptr, 0
    };
    On1xSDK_Target *raw_target = nullptr;
    check(on1x_sdk_target_create(&descriptor, &raw_target), "target creation", name);
    const std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>
        target(raw_target, on1x_sdk_target_destroy);
    On1xSDK_Program *raw_program = nullptr;
    check(sdk::make_builtin_mir(module, &raw_program), "program preparation", name);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    Context context;
    context.native_pic = options.pic;
    context.native_emit_c_entrypoint = options.emit_c_entrypoint;
    context.mir_speculate = options.speculate;
    On1xSDK_Registry *raw_registry = nullptr;
    check(on1x_sdk_registry_create(&raw_registry), "registry creation", name);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    check(registrar_for(name)(registry.get(), &context), "registration", name);
    const auto poll = [](void *userdata) -> On1xSDK_Status {
        const auto &opt = *static_cast<const NativeOptions *>(userdata);
        return opt.cancelled && opt.cancelled(opt.cancel_userdata)
            ? ON1X_SDK_CANCELLED : ON1X_SDK_OK;
    };
    const On1xSDK_BackendRequest request{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendRequest),
        program.get(), target.get(), ON1X_SDK_ARTIFACT_ASSEMBLY, module.version,
        name, std::string_view(name).size(), ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC,
        0, options.byte_limit, poll, const_cast<NativeOptions *>(&options)
    };
    On1xSDK_Artifact *raw_artifact = nullptr;
    const auto status = on1x_sdk_backend_produce(registry.get(), &request, &raw_artifact);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    if (context.error) std::rethrow_exception(context.error);
    check(status, "native emission", name);
    const std::uint8_t *bytes = nullptr;
    std::size_t size = 0;
    check(on1x_sdk_artifact_bytes(artifact.get(), &bytes, &size), "artifact retrieval", name);
    return {size ? std::string(reinterpret_cast<const char *>(bytes), size) : std::string{},
            context.exit_code, std::move(context.diagnostic)};
}
}

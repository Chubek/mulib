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

Product run_c11(const Unit &unit, std::string_view source, const C11Options &options)
{
    Context context;
    context.c11 = options;
    return run("c11", registrar_for("c11"),
               [&](auto out) { return sdk::make_checked_hir(unit, source, out); }, std::move(context));
}

Product run_mir(const mir::Module &module, const char *backend)
{
    return run(backend, registrar_for(backend),
               [&](auto out) { return sdk::make_builtin_mir(module, out); });
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

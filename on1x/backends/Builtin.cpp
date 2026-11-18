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
                             (status == ON1X_SDK_LIMIT ? " exceeded its resource limit" : " failed"));
}

template<class Prepare>
Product run(const char *name, Register registrar, Prepare prepare)
{
    On1xSDK_Program *raw_program = nullptr;
    check(prepare(&raw_program), "program preparation", name);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    Context context;
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

Product run_c11(const Unit &unit, std::string_view source)
{
    return run("c11", registrar_for("c11"),
               [&](auto out) { return sdk::make_checked_hir(unit, source, out); });
}

Product run_mir(const mir::Module &module, const char *backend)
{
    return run(backend, registrar_for(backend),
               [&](auto out) { return sdk::make_builtin_mir(module, out); });
}
}

#include "On1xSDK-Backend.h"
#include "FacetInternal.hpp"

namespace on1x::sdk {
On1xSDK_Status register_checked_hir_backend(On1xSDK_Registry *registry,
                                            const On1xSDK_Backend *backend) noexcept {
    return register_facet(registry, Kind::Backend, backend, false, false, true);
}
}

struct On1xSDK_Artifact {
    const std::vector<uint8_t> bytes;
};

namespace on1x::sdk {
On1xSDK_Status run_builtin_backend(const On1xSDK_Registry *registry, const char *name,
                                   const On1xSDK_Program *program, On1xSDK_Artifact **out) noexcept
{
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (!program || (!program->checked_unit && !program->verified_module))
        return ON1X_SDK_INVALID_ARGUMENT;
    // Internal codegen produces large translation units; the public output
    // policy for external plugins is deliberately much smaller.
    constexpr std::size_t limit = 256u * 1024u * 1024u;
    return run_bytes<On1xSDK_Backend>(registry, Kind::Backend, name, program,
                                     limit, limit, [&](auto &&bytes) {
        *out = new On1xSDK_Artifact{std::move(bytes)};
        return ON1X_SDK_OK;
    });
}
}

extern "C" {

On1xSDK_Status on1x_sdk_backend_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Backend *backend) {
    return on1x::sdk::register_facet(registry, on1x::sdk::Kind::Backend, backend, false, true);
}

On1xSDK_Status on1x_sdk_backend_run(const On1xSDK_Registry *registry, const char *name,
                                   const On1xSDK_Program *program,
                                   size_t byte_limit, On1xSDK_Artifact **out) {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (program && (program->checked_unit || program->verified_module))
        return ON1X_SDK_UNSUPPORTED;
    using namespace on1x::sdk;
    return run_bytes<On1xSDK_Backend>(registry, Kind::Backend, name, program,
                                     byte_limit, ON1X_SDK_MAX_ARTIFACT_BYTES, [&](auto &&bytes) {
        *out = new On1xSDK_Artifact{std::move(bytes)};
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_artifact_bytes(const On1xSDK_Artifact *artifact,
                                      const uint8_t **bytes, size_t *size) {
    return on1x::sdk::view_bytes(artifact, bytes, size);
}

void on1x_sdk_artifact_destroy(On1xSDK_Artifact *artifact) {
    delete artifact;
}

} // extern "C"

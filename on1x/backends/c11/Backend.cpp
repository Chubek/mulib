#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"
#include "backend/BackendProviderInternal.hpp"

#include <on1x/ast.hpp>

#include <new>

extern "C" {

static On1xSDK_Status emit_c11(void *userdata, const On1xSDK_Program *program,
                               On1xSDK_OutputSink *artifact) noexcept {
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    std::string_view source_name;
    const auto *unit = on1x::sdk::checked_hir(program, &source_name);
    if (!unit) return ON1X_SDK_UNSUPPORTED;

    try {
        const auto c = on1x::emit_c(*unit, std::string(source_name));
        return on1x_sdk_output_write(artifact, c.data(), c.size());
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        // Let the driver report the original exception, including located diagnostics.
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

static On1xSDK_Status emit_c11_v2(void *userdata,
                                  const On1xSDK_BackendInvocation *invocation,
                                  On1xSDK_OutputSink *artifact) noexcept {
    return invocation ? emit_c11(userdata, invocation->program, artifact)
                      : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status on1x_builtin_c11_register(On1xSDK_Registry *registry,
                                         on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_CHECKED_HIR, 0, 0, ON1X_SDK_ARTIFACT_C11_SOURCE,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0};
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "c11", 3, 0, 0, &capability, 1, emit_c11_v2, context};
    return on1x::sdk::register_builtin_backend_provider(registry, &provider);
}

} // extern "C"

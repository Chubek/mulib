#include "BuiltinMIR.hpp"
#include "BuiltinProvider.hpp"

#include <on1x/backend/mir_c11.hpp>

#include <exception>
#include <new>

extern "C" {

static On1xSDK_Status emit_mir_c11_v2(void *userdata,
                                      const On1xSDK_BackendInvocation *invocation,
                                      On1xSDK_OutputSink *artifact) noexcept {
    if (!invocation) return ON1X_SDK_INVALID_ARGUMENT;
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    try {
        std::string_view target;
        if (const auto status = on1x::backends::selected_mir_target(*invocation, context, target);
            status != ON1X_SDK_OK)
            return status;
        const auto write = [&](const on1x::mir::Module &module) {
            // The emitter sees the portable v4 clauses before specializing.
            const auto c = on1x::mir::emit_directed_c(module, std::nullopt, target, context.mir_speculate);
            return on1x_sdk_output_write(artifact, c.data(), c.size());
        };

        return on1x::backends::consume_portable_mir(invocation->program, write);
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        // Keep located compiler diagnostics intact across the C callback boundary.
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_builtin_mir_c11_register(On1xSDK_Registry *registry,
                                             on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 0, on1x::mir::directive_format_version, ON1X_SDK_ARTIFACT_C11_SOURCE,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0};
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-c11", 7, 0, 0, &capability, 1, emit_mir_c11_v2, context};
    return on1x::backends::register_or_describe(registry, *context, provider);
}

} // extern "C"

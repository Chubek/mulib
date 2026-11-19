#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"
#include "backend/BackendProviderInternal.hpp"

#include <on1x/backend/mir.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <string_view>

extern "C" {

static On1xSDK_Status emit_mir_c11(void *userdata, const On1xSDK_Program *program,
                                   On1xSDK_OutputSink *artifact) noexcept {
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    try {
        const auto write = [artifact](const on1x::mir::Module &module) {
            const auto c = on1x::mir::emit_c(module); // Verifies before emitting.
            return on1x_sdk_output_write(artifact, c.data(), c.size());
        };

        // Preserve the CLI's in-memory path, without imposing a wire-size limit.
        if (const auto *module = on1x::sdk::builtin_mir(program))
            return write(*module);

        const std::uint8_t *bytes = nullptr;
        std::size_t size = 0;
        if (const auto status = on1x_sdk_program_bytes(program, &bytes, &size);
            status != ON1X_SDK_OK)
            return status;
        const auto text = size ? std::string_view(reinterpret_cast<const char *>(bytes), size)
                               : std::string_view{};
        return write(on1x::mir::deserialize(text)); // Also verifies the decoded MIR.
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        // Keep located compiler diagnostics intact across the C callback boundary.
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

static On1xSDK_Status emit_mir_c11_v2(void *userdata,
                                      const On1xSDK_BackendInvocation *invocation,
                                      On1xSDK_OutputSink *artifact) noexcept {
    return invocation ? emit_mir_c11(userdata, invocation->program, artifact)
                      : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status on1x_builtin_mir_c11_register(On1xSDK_Registry *registry,
                                             on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 0, 3, ON1X_SDK_ARTIFACT_C11_SOURCE,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0};
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-c11", 7, 0, 0, &capability, 1, emit_mir_c11_v2, context};
    return on1x::sdk::register_builtin_backend_provider(registry, &provider);
}

} // extern "C"

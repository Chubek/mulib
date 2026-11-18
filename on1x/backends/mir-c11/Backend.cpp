#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"

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

On1xSDK_Status on1x_builtin_mir_c11_register(On1xSDK_Registry *registry,
                                            on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_Backend backend{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend),
                                  "mir-c11", ON1X_SDK_VERIFIED_MIR, emit_mir_c11, context};
    return on1x_sdk_backend_register(registry, &backend);
}

} // extern "C"

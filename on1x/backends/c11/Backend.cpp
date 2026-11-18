#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"

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

On1xSDK_Status on1x_builtin_c11_register(On1xSDK_Registry *registry,
                                        on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_Backend backend{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend),
                                 "c11", ON1X_SDK_CHECKED_HIR, emit_c11, context};
    return on1x::sdk::register_checked_hir_backend(registry, &backend);
}

} // extern "C"

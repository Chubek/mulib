#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"

#include <on1x/backend/mir.hpp>
#include <on1x/diagnostic.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace {

On1xSDK_Status evaluate(const on1x::mir::Module &module, on1x::backends::Context &context,
                        On1xSDK_OutputSink *artifact) {
    for (const auto &f : module.functions) {
        if (f.name == "main" && !f.blocks.empty() &&
            (!f.blocks.front().parameters.empty() ||
             (f.result != on1x::Type::I64 && f.result != on1x::Type::Void)))
            throw on1x::Error(f.where, "ON1X-MIR-EVAL: main must have no parameters and return i64 or void");
    }

    const auto result = on1x::mir::evaluate(module);
    const auto status = on1x_sdk_output_write(artifact, result.output.data(), result.output.size());
    if (status != ON1X_SDK_OK) return status;

    // A program trap is not an SDK failure: the preceding output must survive.
    if (result.status == on1x::mir::Status::Returned) {
        context.exit_code = static_cast<int>(result.value);
    } else {
        using on1x::diagnostic::terminal_text;
        context.diagnostic = terminal_text(result.where.file) + ":" +
            std::to_string(result.where.line) + ":" + std::to_string(result.where.column) +
            ": ON1X-MIR-EVAL: " + terminal_text(result.diagnostic) + "\n";
        context.exit_code = result.status == on1x::mir::Status::Trapped ? 70 : 75;
    }
    return ON1X_SDK_OK;
}

} // namespace

extern "C" {

static On1xSDK_Status run_mir_eval(void *userdata, const On1xSDK_Program *program,
                                  On1xSDK_OutputSink *artifact) noexcept {
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    context.exit_code = 0;
    context.diagnostic.clear();

    try {
        if (const auto *module = on1x::sdk::builtin_mir(program))
            return evaluate(*module, context, artifact);

        const std::uint8_t *bytes = nullptr;
        std::size_t size = 0;
        if (const auto status = on1x_sdk_program_bytes(program, &bytes, &size);
            status != ON1X_SDK_OK)
            return status;
        const auto wire = size ? std::string_view(reinterpret_cast<const char *>(bytes), size)
                               : std::string_view{};
        return evaluate(on1x::mir::deserialize(wire), context, artifact);
    } catch (...) {
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_builtin_mir_eval_register(On1xSDK_Registry *registry,
                                             on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_Backend backend{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Backend),
                                 "mir-eval", ON1X_SDK_VERIFIED_MIR, run_mir_eval, context};
    return on1x_sdk_backend_register(registry, &backend);
}

} // extern "C"

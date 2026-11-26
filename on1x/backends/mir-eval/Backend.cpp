#include "BuiltinMIR.hpp"
#include "BuiltinProvider.hpp"

#include <on1x/diagnostic.hpp>

#include <exception>
#include <new>
#include <string>

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

static On1xSDK_Status run_mir_eval_v2(void *userdata,
                                     const On1xSDK_BackendInvocation *invocation,
                                     On1xSDK_OutputSink *artifact) noexcept {
    if (!invocation) return ON1X_SDK_INVALID_ARGUMENT;
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};
    context.exit_code = 0;
    context.diagnostic.clear();

    try {
        return on1x::backends::consume_specialized_mir(*invocation, context, "mir-eval",
            [&](const auto &module) { return evaluate(module, context, artifact); });
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_builtin_mir_eval_register(On1xSDK_Registry *registry,
                                              on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 0, on1x::mir::directive_format_version, ON1X_SDK_ARTIFACT_PROGRAM_OUTPUT,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0};
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-eval", 8, 0, 0, &capability, 1, run_mir_eval_v2, context};
    return on1x::backends::register_or_describe(registry, *context, provider);
}

} // extern "C"

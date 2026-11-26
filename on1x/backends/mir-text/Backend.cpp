#include "BuiltinMIR.hpp"
#include "BuiltinProvider.hpp"

#include <exception>
#include <new>

extern "C" {

static On1xSDK_Status emit_mir_text(void *userdata, const On1xSDK_Program *program,
                                    On1xSDK_OutputSink *artifact) noexcept {
    auto &context = *static_cast<on1x::backends::Context *>(userdata);
    context.error = {};

    try {
        const auto write = [artifact](const on1x::mir::Module &module) {
            const auto text = on1x::mir::serialize(module);
            return on1x_sdk_output_write(artifact, text.data(), text.size());
        };
        return on1x::backends::consume_portable_mir(program, write);
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        context.error = std::current_exception();
        return ON1X_SDK_ERROR;
    }
}

static On1xSDK_Status emit_mir_text_v2(void *userdata,
                                      const On1xSDK_BackendInvocation *invocation,
                                      On1xSDK_OutputSink *artifact) noexcept {
    return invocation ? emit_mir_text(userdata, invocation->program, artifact)
                      : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status on1x_builtin_mir_text_register(On1xSDK_Registry *registry,
                                              on1x::backends::Context *context) {
    if (!context) return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_BackendCapability capability{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendCapability),
        ON1X_SDK_VERIFIED_MIR, 0, on1x::mir::directive_format_version, ON1X_SDK_ARTIFACT_MIR_TEXT,
        ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC,
        nullptr, 0, nullptr, 0, nullptr, 0};
    const On1xSDK_BackendProvider provider{
        ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION, sizeof(On1xSDK_BackendProvider),
        "mir-text", 8, 0, 0, &capability, 1, emit_mir_text_v2, context};
    return on1x::backends::register_or_describe(registry, *context, provider);
}

} // extern "C"

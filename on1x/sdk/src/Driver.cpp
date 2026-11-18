#include "Driver.hpp"
#include "On1xSDK.h"
#include "ProgramInternal.hpp"
#include "RegistryInternal.hpp"
#ifdef ON1X_WITH_DYNAMIC_SDK_PLUGINS
#include "PluginHost.hpp"
#endif

#include <cstdlib>
#include <memory>

namespace on1x::sdk_driver {
namespace {

struct Report {
    diagnostic::Registry &registry;
    const std::string &input;
    bool error = false;
    bool failed = false;

    void status(On1xSDK_Status status, std::string_view action, std::string_view hook)
    {
        const char *reason = status == ON1X_SDK_LIMIT ? "limit exceeded" :
                             status == ON1X_SDK_UNSUPPORTED ? "unsupported hook or phase" :
                             status == ON1X_SDK_ABI_MISMATCH ? "SDK ABI mismatch" :
                             status == ON1X_SDK_INVALID_ARGUMENT ? "invalid argument" :
                             status == ON1X_SDK_DUPLICATE ? "duplicate registration" :
                             "operation failed";
        (void)registry.emit({
            "ON1X-SDK", std::string(action) + " '" + diagnostic::terminal_text(hook) + "': " + reason,
            "extension-sdk", "verified-mir", "on1x-sdk", {}, {},
            diagnostic::Catalog::embedded().level("error"), {input}, {}
        });
    }
};

bool compiler_diagnostic_id(std::string_view code)
{
    if (code.empty() || code.front() < 'A' || code.front() > 'Z' || code.back() == '-')
        return false;
    char previous = '\0';
    for (char c : code) {
        if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') &&
            !(c == '-' && previous != '-'))
            return false;
        previous = c;
    }
    return true;
}

extern "C" void receive_diagnostic(void *userdata, const On1xSDK_Diagnostic *value) noexcept
{
    auto &report = *static_cast<Report *>(userdata);
    try {
        const bool error = value->severity == ON1X_SDK_DIAGNOSTIC_ERROR;
        report.error |= error;
        const char *severity = error ? "error" : value->severity == ON1X_SDK_WARNING ? "warning" : "note";
        const std::string_view code = value->code;
        const bool native_id = compiler_diagnostic_id(code);
        // Do not map offsets in the serialized MIR to offsets in a source file.
        std::string message;
        if (!native_id)
            message = "[" + std::string(code) + "] ";
        message += diagnostic::terminal_text(value->message);
        if (value->byte_begin != SIZE_MAX)
            message += " [MIR bytes " + std::to_string(value->byte_begin) + ".." +
                       std::to_string(value->byte_end) + ")";
        if (message.size() > 16384) {
            report.failed = true;
            return;
        }
        (void)report.registry.emit({
            native_id ? std::string(code) : "ON1X-SDK-ANALYZER", std::move(message),
            "extension-sdk", "verified-mir", "on1x-sdk", {}, {},
            diagnostic::Catalog::embedded().level(severity), {"<sdk-mir>"}, {}
        });
    } catch (...) {
        report.failed = true;
    }
}

int exit_for(On1xSDK_Status status)
{
    return status == ON1X_SDK_OK ? 0 : status == ON1X_SDK_LIMIT ? 75 : 1;
}

} // namespace

void check_plugin_selection(const std::vector<std::string> &selectors)
{
#ifdef ON1X_WITH_DYNAMIC_SDK_PLUGINS
    (void)selectors;
#else
    const char *path = std::getenv("ON1X_PLUGIN_PATH");
    if (!selectors.empty() || (path && std::string_view(path).find_first_not_of(':') != std::string_view::npos))
        throw std::runtime_error("sandboxed dynamic SDK plugins are unavailable in this build");
#endif
}

Result run(const mir::Module &module, const std::string &input, const std::string &hook,
           bool emit, diagnostic::Registry &diagnostics, const std::vector<std::string> &selectors)
{
    Report report{diagnostics, input};
    auto failure = [&](On1xSDK_Status status, std::string_view action) -> Result {
        const auto name = hook.size() <= ON1X_SDK_NAME_MAX ? std::string_view(hook) : "<overlong>";
        report.status(status, action, name);
        return {exit_for(status), {}};
    };
    if (!sdk::valid_name(hook))
        return failure(ON1X_SDK_INVALID_ARGUMENT, "hook selection");

    const auto bytes = mir::serialize(module); // Verifies before producing canonical bytes.
#ifdef ON1X_WITH_DYNAMIC_SDK_PLUGINS
    if (bytes.size() > ON1X_SDK_MAX_PROGRAM_BYTES)
        return failure(ON1X_SDK_LIMIT, "MIR handoff");
    auto reply = sdk_plugin::dispatch(sdk_plugin::discover(selectors), hook, emit, bytes);
    if (reply) {
        if (reply->status != ON1X_SDK_OK)
            return failure(reply->status, "external plugin");
        for (const auto &value : reply->diagnostics) {
            const On1xSDK_Diagnostic record{value.severity, value.code.c_str(), value.message.c_str(),
                                           value.begin, value.end};
            receive_diagnostic(&report, &record);
        }
        if (report.failed || diagnostics.exhausted())
            return failure(ON1X_SDK_LIMIT, "diagnostic publication");
        return {report.error ? 1 : 0, std::move(reply->artifact)};
    }
    if (!selectors.empty())
        return failure(ON1X_SDK_UNSUPPORTED, "selected external plugin");
#else
    (void)selectors;
#endif
#ifndef ON1X_WITH_LINKED_SDK_EXTENSION
    return failure(ON1X_SDK_UNSUPPORTED, "no external or linked SDK hook");
#else
    On1xSDK_Program *raw_program = nullptr;
    auto status = sdk::make_verified_mir(bytes, &raw_program);
    if (status != ON1X_SDK_OK)
        return failure(status, "MIR handoff");
    std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);

    On1xSDK_Registry *raw_registry = nullptr;
    status = on1x_sdk_registry_create(&raw_registry);
    if (status != ON1X_SDK_OK)
        return failure(status, "registry creation");
    std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    const On1xSDK_Extension *extension = nullptr;
    try {
        extension = on1x_sdk_linked_extension();
    } catch (...) {
        return failure(ON1X_SDK_ERROR, "linked extension entry point");
    }
    status = on1x_sdk_registry_register(registry.get(), extension);
    if (status != ON1X_SDK_OK)
        return failure(status, "linked extension registration");

    if (!emit) {
        const On1xSDK_DiagnosticSink sink{receive_diagnostic, &report};
        status = on1x_sdk_analyzer_run(registry.get(), hook.c_str(), program.get(), &sink);
        if (status != ON1X_SDK_OK)
            return failure(status, "analysis");
        if (report.failed || diagnostics.exhausted())
            return failure(ON1X_SDK_LIMIT, "diagnostic publication");
        return {report.error ? 1 : 0, {}};
    }
    On1xSDK_Artifact *raw_artifact = nullptr;
    status = on1x_sdk_backend_run(registry.get(), hook.c_str(), program.get(),
                                 ON1X_SDK_MAX_ARTIFACT_BYTES, &raw_artifact);
    if (status != ON1X_SDK_OK)
        return failure(status, "emission");
    std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    status = on1x_sdk_artifact_bytes(artifact.get(), &data, &size);
    if (status != ON1X_SDK_OK)
        return failure(status, "artifact retrieval");
    return {0, size ? std::string(reinterpret_cast<const char *>(data), size) : std::string{}};
#endif
}

} // namespace on1x::sdk_driver

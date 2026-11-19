#include "ProjectInternal.hpp"

namespace on1x::sdk::project {
namespace {

bool covered(std::uint32_t size, std::size_t offset, std::size_t field_size) noexcept {
    return size >= offset && field_size <= size - offset;
}

bool environment_name(std::string_view value) noexcept {
    const auto equal = value.find('=');
    if (equal == std::string_view::npos || equal == 0) return false;
    const auto name = value.substr(0, equal);
    if (!((name.front() >= 'A' && name.front() <= 'Z') ||
          (name.front() >= 'a' && name.front() <= 'z') ||
          name.front() == '_'))
        return false;
    return std::all_of(name.begin() + 1, name.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

On1xSDK_Status normalize_request(const On1xSDK_ProcessRequest* input,
                                 On1xSDK_ProcessRequest& output) noexcept {
    constexpr auto minimum = offsetof(On1xSDK_ProcessRequest, environment) +
                             sizeof(On1xSDK_ProcessRequest::environment);
    if (!input) return ON1X_SDK_INVALID_ARGUMENT;
    if (input->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
        return ON1X_SDK_ABI_MISMATCH;
    if (input->struct_size < minimum) return ON1X_SDK_INVALID_ARGUMENT;
    output = {};
    output.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    output.struct_size = sizeof(output);
    output.executable = input->executable;
    output.working_directory = input->working_directory;
    output.argument_count = input->argument_count;
    output.arguments = input->arguments;
    output.environment_count = input->environment_count;
    output.environment = input->environment;
    if (covered(input->struct_size, offsetof(On1xSDK_ProcessRequest, work_units),
                sizeof(input->work_units)))
        output.work_units = input->work_units;
    if (covered(input->struct_size, offsetof(On1xSDK_ProcessRequest, output_bytes),
                sizeof(input->output_bytes)))
        output.output_bytes = input->output_bytes;
    if (covered(input->struct_size,
                offsetof(On1xSDK_ProcessRequest, deadline_milliseconds),
                sizeof(input->deadline_milliseconds)))
        output.deadline_milliseconds = input->deadline_milliseconds;
    if (!valid_view(output.executable) || !valid_view(output.working_directory) ||
        !absolute_path(as_view(output.executable)) ||
        !absolute_path(as_view(output.working_directory)) ||
        !output.argument_count || !output.arguments ||
        output.argument_count > ON1X_SDK_PROCESS_MAX_ARGUMENTS ||
        (output.environment_count && !output.environment) ||
        output.environment_count > ON1X_SDK_PROCESS_MAX_ENVIRONMENT)
        return output.argument_count > ON1X_SDK_PROCESS_MAX_ARGUMENTS ||
                       output.environment_count > ON1X_SDK_PROCESS_MAX_ENVIRONMENT
                   ? ON1X_SDK_LIMIT
                   : ON1X_SDK_INVALID_ARGUMENT;
    std::size_t bytes = 0;
    for (std::size_t i = 0; i < output.argument_count; ++i) {
        if (!valid_view(output.arguments[i]))
            return ON1X_SDK_INVALID_ARGUMENT;
        if (!checked_add(bytes, output.arguments[i].size) ||
            bytes > ON1X_SDK_PROCESS_MAX_ARGUMENT_BYTES)
            return ON1X_SDK_LIMIT;
        if (as_view(output.arguments[i]).find('\0') != std::string_view::npos ||
            !utf8(as_view(output.arguments[i])))
            return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (as_view(output.arguments[0]) != as_view(output.executable))
        return ON1X_SDK_INVALID_ARGUMENT;
    for (std::size_t i = 0; i < output.environment_count; ++i) {
        if (!valid_view(output.environment[i]))
            return ON1X_SDK_INVALID_ARGUMENT;
        if (!checked_add(bytes, output.environment[i].size) ||
            bytes > ON1X_SDK_PROCESS_MAX_ARGUMENT_BYTES)
            return ON1X_SDK_LIMIT;
        if (!utf8(as_view(output.environment[i])) ||
            !environment_name(as_view(output.environment[i])))
            return ON1X_SDK_INVALID_ARGUMENT;
        const auto item = as_view(output.environment[i]);
        const auto name = item.substr(0, item.find('='));
        for (std::size_t j = 0; j < i; ++j) {
            const auto previous = as_view(output.environment[j]);
            if (name == previous.substr(0, previous.find('=')))
                return ON1X_SDK_DUPLICATE;
        }
    }
    return ON1X_SDK_OK;
}

} // namespace
} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_process_request_validate(
    const On1xSDK_ProcessRequest* request) {
    On1xSDK_ProcessRequest normalized{};
    return on1x::sdk::project::normalize_request(request, normalized);
}

extern "C" On1xSDK_Status on1x_sdk_process_dispatch(
    const On1xSDK_ProcessHost* host, const On1xSDK_ProcessRequest* request,
    const On1xSDK_ProcessCancellation* cancellation,
    On1xSDK_ProcessResult* result) {
    using namespace on1x::sdk::project;
    if (!result) return ON1X_SDK_INVALID_ARGUMENT;
    *result = {};
    constexpr auto required = offsetof(On1xSDK_ProcessHost, finish) +
                              sizeof(On1xSDK_ProcessHost::finish);
    if (!host) return ON1X_SDK_INVALID_ARGUMENT;
    if (host->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
        return ON1X_SDK_ABI_MISMATCH;
    if (!descriptor(host, required) || !host->authorize || !host->execute ||
        !host->finish)
        return ON1X_SDK_INVALID_ARGUMENT;
    On1xSDK_ProcessRequest normalized{};
    if (const auto status = normalize_request(request, normalized);
        status != ON1X_SDK_OK)
        return status;
    std::uint64_t authorization = 0;
    On1xSDK_Status status = ON1X_SDK_ERROR;
    try {
        status = callback_status(
            host->authorize(host->context, &normalized, &authorization));
    } catch (...) {
        status = ON1X_SDK_ERROR;
    }
    if (status != ON1X_SDK_OK) return status;
    if (!authorization) {
        On1xSDK_Status cleanup = ON1X_SDK_ERROR;
        try {
            cleanup = callback_status(host->finish(
                host->context, authorization, ON1X_SDK_ERROR));
        } catch (...) {
            cleanup = ON1X_SDK_ERROR;
        }
        return cleanup == ON1X_SDK_OK ? ON1X_SDK_ERROR : cleanup;
    }

    On1xSDK_ProcessResult temporary{};
    temporary.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    temporary.struct_size = sizeof(temporary);
    try {
        status = callback_status(host->execute(
            host->context, authorization, &normalized, cancellation, &temporary));
    } catch (...) {
        status = ON1X_SDK_ERROR;
    }
    On1xSDK_Status cleanup = ON1X_SDK_ERROR;
    try {
        cleanup = callback_status(host->finish(host->context, authorization, status));
    } catch (...) {
        cleanup = ON1X_SDK_ERROR;
    }
    if (cleanup != ON1X_SDK_OK) {
        *result = {};
        return cleanup;
    }
    if (status != ON1X_SDK_OK) {
        *result = {};
        return status;
    }
    if (temporary.abi_version != ON1X_SDK_PROJECT_ABI_VERSION ||
        temporary.struct_size < sizeof(On1xSDK_ProcessResult) ||
        temporary.exit_code < -1 || temporary.signal_number < 0 ||
        (temporary.exit_code >= 0 && temporary.signal_number != 0) ||
        (normalized.output_bytes &&
         temporary.output_bytes > normalized.output_bytes)) {
        *result = {};
        return ON1X_SDK_ERROR;
    }
    *result = temporary;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_formatter_dispatch_source(
    const On1xSDK_ProcessHost* host, On1xSDK_ProjectString compiler,
    On1xSDK_ProjectString input, On1xSDK_ProjectString working_directory,
    const On1xSDK_ProcessCancellation* cancellation,
    On1xSDK_ProcessResult* result) {
    using namespace on1x::sdk::project;
    if (!valid_view(compiler) || !valid_view(input) ||
        !valid_view(working_directory) || !absolute_path(as_view(compiler)) ||
        !absolute_path(as_view(input)) ||
        !absolute_path(as_view(working_directory)))
        return ON1X_SDK_INVALID_ARGUMENT;
    static constexpr char fmt[] = "fmt";
    static constexpr char target[] = "--target";
    static constexpr char plain[] = "plain";
    const std::array arguments{
        compiler, view(std::string_view(fmt, sizeof(fmt) - 1)),
        view(std::string_view(target, sizeof(target) - 1)),
        view(std::string_view(plain, sizeof(plain) - 1)), input};
    On1xSDK_ProcessRequest request{};
    request.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    request.struct_size = sizeof(request);
    request.executable = compiler;
    request.working_directory = working_directory;
    request.argument_count = arguments.size();
    request.arguments = arguments.data();
    request.output_bytes = ON1X_SDK_CODE_ACTION_MAX_OUTPUT;
    return on1x_sdk_process_dispatch(host, &request, cancellation, result);
}

extern "C" On1xSDK_Status on1x_sdk_diagnostic_catalog_dispatch_check(
    const On1xSDK_ProcessHost* host, On1xSDK_ProjectString compiler,
    On1xSDK_ProjectString catalog, On1xSDK_ProjectString source,
    On1xSDK_ProjectString working_directory,
    const On1xSDK_ProcessCancellation* cancellation,
    On1xSDK_ProcessResult* result) {
    using namespace on1x::sdk::project;
    if (!valid_view(compiler) || !valid_view(catalog) ||
        !valid_view(source) || !valid_view(working_directory) ||
        !absolute_path(as_view(compiler)) ||
        !absolute_path(as_view(catalog)) ||
        !absolute_path(as_view(source)) ||
        !absolute_path(as_view(working_directory)))
        return ON1X_SDK_INVALID_ARGUMENT;
    const std::array arguments{
        compiler, view("--check"), view("--diagnostics"), view("json"),
        view("--color"), view("never"), view("--diagnostic-catalog"),
        catalog, source};
    On1xSDK_ProcessRequest request{};
    request.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    request.struct_size = sizeof(request);
    request.executable = compiler;
    request.working_directory = working_directory;
    request.argument_count = arguments.size();
    request.arguments = arguments.data();
    request.output_bytes = 4u * 1024u * 1024u;
    return on1x_sdk_process_dispatch(host, &request, cancellation, result);
}

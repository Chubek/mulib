#include "On1xSDK-CLIOpt.h"
#include "MetadataInternal.hpp"

#include <charconv>

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;

constexpr auto kind = Kind::CLIOpt;
using D = On1xSDK_CLIOpt;
using F = md::Field<D>;

bool valid_option(std::string_view name) noexcept {
    if (name.empty() || !md::lower(name.front())) return false;
    bool hyphen = false;
    for (char c : name) {
        if (md::lower(c) || md::digit(c)) {
            hyphen = false;
        } else if (c == '-' && !hyphen) {
            hyphen = true;
        } else {
            return false;
        }
    }
    return !hyphen;
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_cliopt_register(On1xSDK_Registry* registry, const On1xSDK_CLIOpt* option) {
    if (const auto status = md::check(registry, option); status != ON1X_SDK_OK) return status;
    if (option->kind != ON1X_SDK_CLIOPT_FLAG &&
        option->kind != ON1X_SDK_CLIOPT_I64 && option->kind != ON1X_SDK_CLIOPT_STRING) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    md::Validation validate;
    const std::array fields{
        F{&D::name, validate.field(option->name, ON1X_SDK_NAME_MAX, valid_option)},
        F{&D::help, validate.text(option->help, ON1X_SDK_MAX_CLIOPT_HELP_BYTES)}
    };
    if (validate.status != ON1X_SDK_OK) return validate.status;
    return md::register_fields(registry, kind, *option, fields);
}

extern "C" const On1xSDK_CLIOpt* on1x_sdk_cliopt_find(const On1xSDK_Registry* registry, const char* name) {
    return md::lookup<On1xSDK_CLIOpt>(registry, kind, name);
}

extern "C" size_t on1x_sdk_cliopt_count(const On1xSDK_Registry* registry) {
    return count(registry, kind);
}

extern "C" const On1xSDK_CLIOpt* on1x_sdk_cliopt_at(const On1xSDK_Registry* registry, size_t index) {
    return static_cast<const On1xSDK_CLIOpt*>(at(registry, kind, index));
}

extern "C" On1xSDK_Status on1x_sdk_cliopt_parse(const On1xSDK_Registry* registry,
                                               const char* token, On1xSDK_CLIOptValue* out) {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = {};
    if (!registry) return ON1X_SDK_INVALID_ARGUMENT;

    std::string_view word;
    constexpr auto max_token = 2 + ON1X_SDK_NAME_MAX + 1 + ON1X_SDK_MAX_CLIOPT_VALUE_BYTES;
    if (const auto status = md::bounded(token, max_token, word); status != ON1X_SDK_OK) return status;
    if (!word.starts_with("--")) return ON1X_SDK_INVALID_ARGUMENT;
    word.remove_prefix(2);
    const auto eq = word.find('=');
    const auto name = word.substr(0, eq);
    if (name.size() > ON1X_SDK_NAME_MAX) return ON1X_SDK_LIMIT;
    if (!valid_option(name)) return ON1X_SDK_INVALID_ARGUMENT;
    const auto value = eq == std::string_view::npos ? std::string_view{} : word.substr(eq + 1);
    if (value.size() > ON1X_SDK_MAX_CLIOPT_VALUE_BYTES) return ON1X_SDK_LIMIT;
    const auto* option = static_cast<const On1xSDK_CLIOpt*>(find(registry, kind, name));
    if (!option) return ON1X_SDK_UNSUPPORTED;

    On1xSDK_CLIOptValue parsed{};
    parsed.option = option;
    if (option->kind == ON1X_SDK_CLIOPT_FLAG) {
        if (eq != std::string_view::npos) return ON1X_SDK_INVALID_ARGUMENT;
        parsed.integer = 1;
    } else {
        if (eq == std::string_view::npos) return ON1X_SDK_INVALID_ARGUMENT;
        if (option->kind == ON1X_SDK_CLIOPT_I64) {
            const auto* end = value.data() + value.size();
            const auto result = std::from_chars(value.data(), end, parsed.integer, 10);
            if (result.ec != std::errc{} || result.ptr != end) return ON1X_SDK_INVALID_ARGUMENT;
        } else {
            if (std::any_of(value.begin(), value.end(), [](unsigned char c) {
                    return c < 0x20 || c == 0x7f;
                })) return ON1X_SDK_INVALID_ARGUMENT;
            parsed.text = value.data();
            parsed.text_size = value.size();
        }
    }
    *out = parsed;
    return ON1X_SDK_OK;
}

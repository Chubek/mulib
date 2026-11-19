#include "ProjectInternal.hpp"

namespace {

constexpr std::array<std::string_view, 10> builtins{{
    "build", "release", "init", "exec", "format",
    "fetch", "lock", "info", "inspect", "validate"
}};

} // namespace

extern "C" int on1x_sdk_command_is_builtin(On1xSDK_ProjectString name) {
    using namespace on1x::sdk::project;
    if (!valid_view(name)) return 0;
    const auto value = as_view(name);
    return std::find(builtins.begin(), builtins.end(), value) != builtins.end();
}

extern "C" On1xSDK_Status on1x_sdk_command_validate_extension_name(
    On1xSDK_ProjectString name) {
    using namespace on1x::sdk::project;
    if (!valid_view(name)) return ON1X_SDK_INVALID_ARGUMENT;
    const auto value = as_view(name);
    if (on1x_sdk_command_is_builtin(name)) return ON1X_SDK_DUPLICATE;
    const auto colon = value.find(':');
    if (colon == std::string_view::npos ||
        value.find(':', colon + 1) != std::string_view::npos ||
        !command_component(value.substr(0, colon)) ||
        !command_component(value.substr(colon + 1)))
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

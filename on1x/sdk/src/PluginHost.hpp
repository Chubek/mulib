#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "On1xSDK-Diagnostic.h"

namespace on1x::sdk_plugin {
struct Diagnostic {
    On1xSDK_Severity severity;
    std::string code, message;
    std::size_t begin, end;
};
struct Reply {
    On1xSDK_Status status;
    std::string artifact;
    std::vector<Diagnostic> diagnostics;
};

// Discovery never executes or maps a plugin in the compiler.
std::vector<std::string> discover(const std::vector<std::string> &selectors);
std::optional<Reply> dispatch(const std::vector<std::string> &paths, std::string_view hook,
                              bool emit, std::string_view mir);
}

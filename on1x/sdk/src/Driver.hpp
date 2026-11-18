#pragma once

#include <on1x/backend/mir.hpp>
#include <on1x/diagnostic.hpp>
#include <vector>

namespace on1x::sdk_driver {
struct Result {
    int exit_code;
    std::string artifact;
};

// This is an on1xc implementation detail, not part of the installed SDK.
void check_plugin_selection(const std::vector<std::string> &selectors);
Result run(const mir::Module &, const std::string &input_name,
           const std::string &hook_name, bool emit, diagnostic::Registry &,
           const std::vector<std::string> &selectors);
}

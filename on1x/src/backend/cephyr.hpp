#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace on1x::cephyr {

struct Options {
    bool factir = false, sarif = false, sandslab = false, blackstone = false;
    std::string profile, fuel;
    std::vector<std::string> passes, parameters;
    std::vector<std::filesystem::path> trusted_files;
    std::filesystem::path facts, smtlib;
};

struct Request {
    std::string_view source;
    std::string logical_filename; // Generated C filename for #line, not the On1x source.
    std::string target;
    std::filesystem::path resource_root, archon_root, config;
    std::vector<std::filesystem::path> include_dirs;
    Options options;
};

enum class Status { ok, unsupported_target, invalid_argument, compile_error, internal_error };

struct Result {
    Status status;
    std::string object; // Owned binary data; empty on failure.
    std::string diagnostics;
};

Result compile_object(const Request& request);

}

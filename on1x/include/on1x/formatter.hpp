#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace on1x::format {

enum class Target { plain, latex, troff };
struct Options {
    // An explicit value overrides a source target directive. Otherwise the
    // directive wins, falling back to plain when absent.
    std::optional<Target> target;
    std::vector<std::string> enabled_extensions;
};

// Formats one source file without loading its imports or running preprocessors.
// Uses the nearest .on1xfmt above its path. Returns the entire rendered result
// or throws; the caller is responsible for publishing it.
std::string file(const std::filesystem::path& input, const Options& options = {});

} // namespace on1x::format

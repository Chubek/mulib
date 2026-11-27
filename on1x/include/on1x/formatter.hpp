#pragma once

#include <on1x/loader.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace on1x::format {

enum class Target { plain, latex, troff, html };
struct Options {
    // Explicit value > source directive > .on1xfmt target > plain.
    std::optional<Target> target;
    std::vector<std::string> enabled_extensions;
    // Opt in only for a trusted source path and loader configuration. This
    // discovers operator headers in imports/preludes, without executing them.
    // Leave disengaged for scratch files and untrusted editor buffers.
    std::optional<LoaderOptions> operator_loader;
};

// Formats one source file. By default only that file is parsed; neither mode
// runs preprocessors or declaration extensions. Interfaces (.on1xi) always
// use the declaration-only signature parser, without operator discovery.
// Uses the nearest .on1xfmt above its path. Returns the entire rendered result
// or throws; the caller is responsible for publishing it.
std::string file(const std::filesystem::path& input, const Options& options = {});

} // namespace on1x::format

#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace on1x {
using SourcePrefixes = std::map<std::string, std::string>;

// Match only whole path components; the most specific mapping wins.
inline std::string remap_source_path(const std::string& file, const SourcePrefixes& prefixes) {
    const std::string* replacement = nullptr;
    std::size_t length = 0;
    for (const auto& [from, to] : prefixes) {
        if (from.size() <= length || !file.starts_with(from)) continue;
        if (from != "/" && file.size() != from.size() && file[from.size()] != '/') continue;
        length = from.size();
        replacement = &to;
    }
    if (!replacement) return file;
    auto suffix = file.substr(length);
    if (!suffix.empty() && suffix.front() == '/') suffix.erase(0, 1);
    return suffix.empty() ? *replacement
        : (std::filesystem::path(*replacement) / suffix).lexically_normal().generic_string();
}
}

#pragma once
#include <on1x/ast.hpp>
#include <on1x/extensions.hpp>
#include <filesystem>
#include <set>

namespace on1x {
struct LoaderOptions {
 std::filesystem::path stdlib;
 std::vector<std::filesystem::path> paths;
 std::vector<std::filesystem::path> stdlib_paths;
 bool stdlib_explicit=false;
 bool prelude=true;
 std::vector<std::filesystem::path> preludes;
 std::size_t max_modules=1024,max_depth=64,max_bytes=16*1024*1024;
 // Tooling-only overlays, keyed by canonical paths. The ordinary compiler uses
 // an empty map. All normal parsing, import and byte limits still apply.
  std::map<std::filesystem::path,std::string> source_overrides;
  extensions::Options extensions;
};
struct LoadedUnit {
 Unit unit;
 // Canonical paths in dependency-first order, each included exactly once.
 std::vector<std::filesystem::path> dependencies;
 std::set<std::string> internal_functions;
};
// Reads the new *_PATH lists and legacy ON1X_STDLIB/ON1X_PATH aliases.
 LoaderOptions default_loader_options(const std::string& executable);
std::vector<std::filesystem::path> standard_library_roots(const LoaderOptions&);
// All state belongs to this invocation. Failed loads publish no partial unit.
LoadedUnit load_unit(const std::filesystem::path&,const LoaderOptions&);
}

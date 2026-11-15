#pragma once
#include <on1x/ast.hpp>
#include <filesystem>
#include <set>

namespace on1x {
struct LoaderOptions {
 std::filesystem::path stdlib;
 std::vector<std::filesystem::path> paths;
 bool prelude=true;
 std::vector<std::filesystem::path> preludes;
 std::size_t max_modules=1024,max_depth=64,max_bytes=16*1024*1024;
 // Tooling-only overlays, keyed by canonical paths. The ordinary compiler uses
 // an empty map. All normal parsing, import and byte limits still apply.
 std::map<std::filesystem::path,std::string> source_overrides;
};
struct LoadedUnit {
 Unit unit;
 // Canonical paths in dependency-first order, each included exactly once.
 std::vector<std::filesystem::path> dependencies;
 std::set<std::string> internal_functions;
};
// Reads ON1X_STDLIB and ON1X_PATH; explicit CLI options may override/precede them.
LoaderOptions default_loader_options(const std::string& executable);
// All state belongs to this invocation. Failed loads publish no partial unit.
LoadedUnit load_unit(const std::filesystem::path&,const LoaderOptions&);
}

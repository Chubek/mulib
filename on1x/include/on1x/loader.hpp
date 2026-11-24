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
  bool kernel_c11=false; // Explicit unsafe-only C11 compilation profile.
   bool kernel_stage=false; // Emit unresolved reviewed-profile callers, not an admission decision.
   bool standalone_guest=false; // Untrusted source: no imports or host-side extensions.
 std::vector<std::filesystem::path> preludes;
 std::size_t max_modules=1024,max_depth=64,max_bytes=16*1024*1024;
 // Tooling-only overlays, keyed by canonical paths. The ordinary compiler uses
 // an empty map. All normal parsing, import and byte limits still apply.
  std::map<std::filesystem::path,std::string> source_overrides;
  extensions::Options extensions;
  std::vector<std::filesystem::path> interfaces; // Explicit opt-in, never searched or imported.
};
struct LoadedUnit {
 Unit unit;
 // Canonical paths in dependency-first order, each included exactly once.
 std::vector<std::filesystem::path> dependencies;
 // Exact immutable bytes parsed for each dependency at the same index. This
 // includes tooling overrides and avoids a racy second filesystem read.
 std::vector<std::string> dependency_sources;
 std::set<std::string> internal_functions;
 std::vector<InterfaceObligation> interfaces;
};
// Reads the new *_PATH lists and legacy ON1X_STDLIB/ON1X_PATH aliases.
 LoaderOptions default_loader_options(const std::string& executable);
std::vector<std::filesystem::path> standard_library_roots(const LoaderOptions&);
// All state belongs to this invocation. Failed loads publish no partial unit.
LoadedUnit load_unit(const std::filesystem::path&,const LoaderOptions&);
struct KernelImportRequests {
 std::vector<KernelImport> requests;
 std::map<std::string,std::string> source_digests;
};
// Parse the ordinary module graph without executing extensions or PPX.
// Reports native KernelBind requests only, never portable stdffi imports.
// This does not resolve C declarations or admit calls.
KernelImportRequests inspect_kernel_imports(const std::filesystem::path&,const LoaderOptions&);
}

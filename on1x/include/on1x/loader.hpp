#pragma once
#include <on1x/ast.hpp>
#include <on1x/extensions.hpp>
#include <on1x/unused_binding.hpp>
#include <filesystem>
#include <set>

namespace on1x {
struct LoaderOptions {
 std::filesystem::path stdlib;
 std::vector<std::filesystem::path> paths;
 std::vector<std::filesystem::path> stdlib_paths;
 std::vector<std::filesystem::path> foreign_default_paths; // Used without an explicit MULIB_FOREIGNLIB_PATH.
 bool stdlib_explicit=false;
  bool prelude=true;
  bool kernel_c11=false; // Explicit unsafe-only C11 compilation profile.
   bool kernel_stage=false; // Emit unresolved reviewed-profile callers, not an admission decision.
   bool kernel_load_time=false; // Pin portable stdffi imports for direct C11 load-time resolution.
   bool standalone_guest=false; // Untrusted source: no imports or host-side extensions.
 std::vector<std::filesystem::path> preludes;
 std::size_t max_modules=1024,max_depth=64,max_bytes=16*1024*1024;
 // Tooling-only overlays, keyed by canonical paths. The ordinary compiler uses
 // an empty map. All normal parsing, import and byte limits still apply.
  std::map<std::filesystem::path,std::string> source_overrides;
  extensions::Options extensions;
   std::vector<std::filesystem::path> interfaces; // Explicit opt-in, never searched or imported.
  bool declaration_interfaces=false; // Accept checked declaration-only signatures; never emit this result.
  struct ModuleMember {
   std::filesystem::path package; // Canonical, verified package root.
   std::string declared_name;
   std::vector<std::filesystem::path> fragments,interfaces;
  };
  std::filesystem::path project_package;
  std::map<std::string,ModuleMember> module_members;
  std::map<std::filesystem::path,std::map<std::string,ModuleMember>> scoped_module_members;
  std::map<std::string,std::filesystem::path> program_members;
  bool observe_unused_bindings=false;
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
   std::size_t declaration_interface_members=0;
   // Canonical interface files read by the loader, including sealed standard
   // and project modules. These are not executable source edges.
   std::vector<std::filesystem::path> signature_dependencies;
   bool source_capabilities=false; // Parsed source requested PPX, native or extension capabilities.
  // Direct edges between physical source files, not synthetic module groups.
 std::vector<std::pair<std::filesystem::path,std::filesystem::path>> source_edges;
 std::vector<UnusedBinding> unused_bindings;
};
// Reads the new *_PATH lists and legacy ON1X_STDLIB/ON1X_PATH aliases.
 LoaderOptions default_loader_options(const std::string& executable);
std::vector<std::filesystem::path> standard_library_roots(const LoaderOptions&);
// All state belongs to this invocation. Failed loads publish no partial unit.
LoadedUnit load_unit(const std::filesystem::path&,const LoaderOptions&);
// Parse only the supplied entry snapshot against the canonical operator import
// and prelude environment. Dependency bodies are not parsed or executed;
// no PPX, extension, foreign-admission or checking step runs.
Unit parse_source_with_operator_imports(const std::filesystem::path&,const std::string&,
                                        const LoaderOptions&);
struct KernelImportRequests {
 std::vector<KernelImport> requests;
 std::map<std::string,std::string> source_digests;
};
// Parse the ordinary module graph without executing extensions or PPX.
// Reports native KernelBind requests only, never portable stdffi imports.
// This does not resolve C declarations or admit calls.
KernelImportRequests inspect_kernel_imports(const std::filesystem::path&,const LoaderOptions&);
}

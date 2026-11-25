#pragma once
#include <on1x/ast.hpp>
#include <filesystem>
#include <span>
#include <string_view>

namespace on1x::stdffi {
// Read-only descriptions of the compiled standard registry. The i64-only
// recipe profile has `arity` i64 arguments and returns i64. `symbol` names
// the source C definition; it is provenance, not a runtime link request.
struct Export {
 std::string_view kernel, member, symbol, function, header, sha256;
 std::size_t arity;
};
std::span<const Export> exports();
bool has_kernel(std::string_view kernel);
std::optional<Function> materialize(std::string_view kernel,std::string_view member,const Location&);

// Load-time C11 pins are derived from the checked compiler registry; a package
// or an ffispec on the application's search path cannot set these digests.
struct LoadTimePin {
 unsigned version;
 std::string interface_sha256,behavior_sha256;
};
LoadTimePin load_time_pin(std::string_view member,std::size_t arity,const Location&);

// A selected ffispec can only choose exports from the generated immutable table.
class SelectedLibrary {
 std::filesystem::path path_;
 std::vector<std::size_t> exports_;
public:
 static SelectedLibrary load(const std::filesystem::path&,std::string_view kernel,const Location&);
 const std::filesystem::path& path() const { return path_; }
 const Export* find(std::string_view member) const;
 std::optional<Function> materialize(std::string_view member,const Location&) const;
};
}

#pragma once
#include <on1x/ast.hpp>
#include <span>
#include <string_view>

namespace on1x::stdffi {
// Read-only descriptions of the compiled standard registry. The i64-only
// recipe profile has `arity` i64 arguments and returns i64. `symbol` names
// the reviewed C definition; it is provenance, not a runtime link request.
struct Export {
 std::string_view kernel, member, symbol, function, header, sha256;
 std::size_t arity;
};
std::span<const Export> exports();
bool has_kernel(std::string_view kernel);
std::optional<Function> materialize(std::string_view kernel,std::string_view member,const Location&);
}

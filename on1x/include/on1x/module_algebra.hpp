#pragma once
#include <on1x/module_visibility.hpp>
#include <string_view>
#include <map>
#include <set>
#include <compare>

namespace on1x::oml {
// Only the resolver may supply owner, requester and dependency facts.
ModuleAccess access(VisibilityRule, const ModuleProvenance& owner,
                    const ModuleProvenance& requester, bool dependency_admitted = false);
bool reexport(ModuleAccess original, ModuleAccess forwarding);
// Transaction-local views supplied by trusted adapters. No deserialization path.
enum class Namespace { Value, NominalType, Syntax, Module, Cliche, Operator };
struct Name {
    Namespace space;
    std::string spelling; // operator keys include position/arity/fixity
    auto operator<=>(const Name&) const = default;
};
struct Binding {
    std::string declaration, type, seal;
    ModuleProvenance owner;
    VisibilityRule visibility;
};
using Environment = std::map<Name, Binding>;
// Partial operations throw invalid_argument on a failed premise. Inputs are immutable.
Environment merge(const Environment&, const Environment&);
Environment project(const Environment&, const std::set<Name>&, bool hide = false);
Environment rename(const Environment&, const std::map<Name, Name>&);
Environment select(const Environment& exports, const std::vector<Name>& selectors,
                   const ModuleProvenance& requester, bool admitted);
std::string_view rule_digest();
}

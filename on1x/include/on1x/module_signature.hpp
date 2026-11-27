#pragma once
#include <on1x/ast.hpp>
#include <compare>
#include <filesystem>
#include <string_view>

namespace on1x {
struct AbstractTypeMember {
 Location where;
 std::string name;
 std::size_t arity=0;
};

struct ModuleSignatureMember {
 Location where;
 std::string name;
 Type signature;
 bool procedure=false;
};

// Parsed declaration-only source. An empty module denotes the legacy `.on1xi`
// v1 format, which specifies obligations but does not control exports.
struct ModuleSignature {
 Location where;
 std::optional<std::string> module;
 std::vector<ModuleSignatureMember> members;
 std::vector<AbstractTypeMember> types;
};

// A seal is an identity, not a checked HIR type. It deliberately contains no
// layout, labels or constructors. The loader supplies canonical provenance,
// not a source-spelled filesystem path or the importing module's alias.
struct AbstractTypeIdentity {
 std::filesystem::path implementation;
 std::string module;
 std::string member;
 auto operator<=>(const AbstractTypeIdentity&) const = default;
};

struct SealedModuleSignature {
 std::map<std::string,AbstractTypeIdentity> types;
};

ModuleSignature parse_module_signature(const std::vector<Token>&);

// Verify new-format declarations against exactly one original (unflattened)
// implementation. This does not run the implementation or authorize emission.
// The existing legacy verifier remains responsible for v1.
SealedModuleSignature verify_module_signature(const ModuleSignature&,const Unit& original,
                                              std::string_view declared_module,
                                              const std::filesystem::path& canonical_implementation);
// For a manifest-authorized partial module, the path is the canonical identity
// of the merged provider selected by that manifest, not the path of a client.
SealedModuleSignature verify_module_signature(const ModuleSignature&,
                                              const std::vector<const Unit*>& original_fragments,
                                              std::string_view declared_module,
                                              const std::filesystem::path& canonical_implementation);

// Current checked HIR has no opaque source type representation. Neither the
// concrete layout nor these declaration-only identities may flow to clients.
void require_executable_module_signature(const ModuleSignature&);
}

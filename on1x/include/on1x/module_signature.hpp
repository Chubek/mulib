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
 std::optional<Type> manifest;
};

struct ModuleSignatureMember {
 Location where;
 std::string name;
 Type signature;
 bool procedure=false;
};

struct SignatureReference {
 Location where;
 std::string name;
 std::vector<Type> arguments;
};
struct SignatureSharing {
 Location where;
 std::string member;
 Type equation;
};
struct NamedSignature {
 Location where;
 std::string name;
 std::vector<std::string> parameters;
 std::vector<ModuleSignatureMember> members;
 std::vector<AbstractTypeMember> types;
 std::vector<SignatureReference> includes;
 std::vector<SignatureSharing> sharing;
};
enum class SignatureSeal { Opaque, Transparent };

// Parsed declaration-only source. An empty module denotes the legacy `.on1xi`
// v1 format, which specifies obligations but does not control exports.
struct ModuleSignature {
 Location where;
 std::optional<std::string> module;
 std::vector<ModuleSignatureMember> members;
 std::vector<AbstractTypeMember> types;
 SignatureSeal seal=SignatureSeal::Opaque;
 std::vector<NamedSignature> definitions;
 std::optional<SignatureReference> ascription;
 // Source-only offsets for contextual words. Not used by conformance.
 std::vector<std::size_t> context_keywords;
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

// Transparent ascription preserves a nominal key rather than introducing an
// abstract seal. Do not compare or convert these two identity domains.
struct TransparentTypeIdentity {
 std::filesystem::path implementation;
 std::string module;
 std::string member;
 auto operator<=>(const TransparentTypeIdentity&) const = default;
};

struct SealedModuleSignature {
 std::map<std::string,AbstractTypeIdentity> types;
 std::map<std::string,TransparentTypeIdentity> transparent_types;
 // Source-normalized equations: named leaves refer to types or
 // transparent_types in *this* provider; never use a spelling as global ID.
 std::map<std::string,Type> manifest_types;
};

 ModuleSignature parse_module_signature(const std::vector<Token>&);
 // Instantiate the file-local signature catalog and validate its expansion.
 // Resolve each file independently before merging interface fragments.
 ModuleSignature resolve_module_signature(const ModuleSignature&);

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

// The default executable boundary has no opaque transport. Preserve it for
// callers that cannot install checked ownership and representation gates.
void require_executable_module_signature(const ModuleSignature&);

// Reverify the original fragments and permit exactly the listed closed scalar
// equations. The loader attaches each result to its authenticated declaration;
// clients cannot interpret this spelling map in their own namespace.
std::map<std::string,Type> admit_executable_manifest_signature(
    const ModuleSignature&,const std::vector<const Unit*>& originals,
    std::string_view declared_module,const std::filesystem::path& canonical_implementation);

// Source-side permit for a loader that can attach authenticated opaque owners
// and route them through a checked C11-only boundary. Call after successful
// verification of these same originals; the frontend must still check bodies,
// representation and operations on the resulting nominal values.
std::set<std::string> admit_executable_opaque_signature(
    const ModuleSignature&,const SealedModuleSignature& verified,
    const std::vector<const Unit*>& originals,std::string_view declared_module,
    const std::filesystem::path& canonical_implementation);
}

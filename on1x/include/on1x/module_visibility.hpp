#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace on1x {

enum class DeclarationVisibility { Unspecified, Public, Shared, Private };

// Unspecified records the absence of an annotation, not an explicit public
// annotation. Interface matching needs to distinguish the two.
struct VisibilityRule {
    DeclarationVisibility kind = DeclarationVisibility::Unspecified;
    std::size_t elevation = 0;
};

// The resolver supplies this information. Names, aliases, import spellings,
// and PPX output must never supply or replace canonical provenance. Re-exported
// bindings are checked with the original declaration's rule and owner.
struct ModuleProvenance {
    std::filesystem::path canonical_file;
    // An absolute canonical identity (e.g., the package manifest path), not a
    // package display name. Empty means that package membership is unproven.
    std::filesystem::path package_identity;
    std::vector<std::string> module_components;
};

enum class ModuleVisibilityIssue {
    None, InvalidKind, UnexpectedElevation, MissingModuleScope, ElevationTooHigh
};

enum class ModuleAccess {
    Allowed, InvalidVisibility, Private, SharedPackage, SharedScope, DependencyNotAdmitted
};

namespace module_visibility_detail {

inline ModuleVisibilityIssue rule_issue(VisibilityRule rule) {
    switch (rule.kind) {
    case DeclarationVisibility::Unspecified:
    case DeclarationVisibility::Public:
    case DeclarationVisibility::Private:
        return rule.elevation == 0 ? ModuleVisibilityIssue::None
                                   : ModuleVisibilityIssue::UnexpectedElevation;
    case DeclarationVisibility::Shared:
        return ModuleVisibilityIssue::None;
    }
    return ModuleVisibilityIssue::InvalidKind;
}

inline bool has_module_scope(const ModuleProvenance& source) {
    return !source.module_components.empty() &&
           std::all_of(source.module_components.begin(), source.module_components.end(),
                       [](const std::string& part) {
                           return !part.empty() && part.find_first_of("./\\") == std::string::npos;
                       });
}

inline bool same_identity(const std::filesystem::path& a,
                          const std::filesystem::path& b) {
    // Do not treat missing or relative provenance as a shared identity. No
    // filesystem access here; paths have already been canonicalized upstream.
    return a.is_absolute() && a == b;
}

} // namespace module_visibility_detail

inline ModuleVisibilityIssue validate_module_visibility(VisibilityRule rule,
                                                         const ModuleProvenance& owner) {
    using namespace module_visibility_detail;
    if (auto issue = rule_issue(rule); issue != ModuleVisibilityIssue::None)
        return issue;
    if (rule.kind == DeclarationVisibility::Shared) {
        if (!has_module_scope(owner))
            return ModuleVisibilityIssue::MissingModuleScope;
        if (rule.elevation > owner.module_components.size())
            return ModuleVisibilityIssue::ElevationTooHigh;
    }
    return ModuleVisibilityIssue::None;
}

// Match the corresponding declarations, not an omitted interface member. An
// omitted member's export status is decided by the signature/sealing pass.
// An annotation inherits from either side if the other side is unannotated.
// The returned rule retains Unspecified when neither side is annotated, so a
// subsequent interface merge can still distinguish that case from Public.
// Validate the merged rule against the declaration owner's scope separately.
inline std::optional<VisibilityRule> merge_module_visibility(VisibilityRule implementation_rule,
                                                            VisibilityRule interface_rule) {
    using namespace module_visibility_detail;
    if (rule_issue(implementation_rule) != ModuleVisibilityIssue::None ||
        rule_issue(interface_rule) != ModuleVisibilityIssue::None)
        return std::nullopt;
    if (implementation_rule.kind == DeclarationVisibility::Unspecified)
        return interface_rule;
    if (interface_rule.kind == DeclarationVisibility::Unspecified ||
        (implementation_rule.kind == interface_rule.kind &&
         implementation_rule.elevation == interface_rule.elevation))
        return implementation_rule;
    return std::nullopt;
}

// dependency_admitted represents a verified requester-to-owner dependency (or
// ordinary legacy import resolution). Admission alone cannot open a private or
// shared declaration.
inline ModuleAccess module_visibility_access(VisibilityRule rule,
                                             const ModuleProvenance& owner,
                                             const ModuleProvenance& requester,
                                             bool dependency_admitted = false) {
    using namespace module_visibility_detail;
    if (validate_module_visibility(rule, owner) != ModuleVisibilityIssue::None)
        return ModuleAccess::InvalidVisibility;
    if (same_identity(owner.canonical_file, requester.canonical_file))
        return ModuleAccess::Allowed;
    if (rule.kind == DeclarationVisibility::Private)
        return ModuleAccess::Private;
    const bool same_package = same_identity(owner.package_identity, requester.package_identity);
    if (rule.kind != DeclarationVisibility::Shared)
        return same_package || dependency_admitted ? ModuleAccess::Allowed
                                                   : ModuleAccess::DependencyNotAdmitted;
    if (!same_package)
        return ModuleAccess::SharedPackage;
    // Elevation drops whole components. Dropping all components shares with
    // the whole package, not with other packages defining the same hierarchy.
    const auto scope_depth = owner.module_components.size() - rule.elevation;
    if (scope_depth == 0)
        return ModuleAccess::Allowed;
    if (!has_module_scope(requester) || requester.module_components.size() < scope_depth ||
        !std::equal(owner.module_components.begin(),
                    owner.module_components.begin() + static_cast<std::ptrdiff_t>(scope_depth),
                    requester.module_components.begin()))
        return ModuleAccess::SharedScope;
    return ModuleAccess::Allowed;
}

} // namespace on1x

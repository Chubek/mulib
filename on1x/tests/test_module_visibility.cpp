#include <on1x/module_visibility.hpp>

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace on1x;

namespace {
using Visibility = DeclarationVisibility;
using Access = ModuleAccess;
using Issue = ModuleVisibilityIssue;

void expect(bool condition, const char* scenario) {
    if (!condition) {
        std::cerr << "module visibility: " << scenario << '\n';
        std::exit(1);
    }
}

void expect_rule(std::optional<VisibilityRule> actual, VisibilityRule expected,
                 const char* scenario) {
    expect(actual && actual->kind == expected.kind && actual->elevation == expected.elevation,
           scenario);
}
}

int main() {
    const ModuleProvenance owner{"/work/pkg-a/inc.on1xm", "/work/pkg-a/On1xfile", {"A", "B"}};
    const ModuleProvenance same_file{owner.canonical_file, {}, {"Alias"}};
    const ModuleProvenance peer{"/work/pkg-a/peer.on1xm", owner.package_identity, {"A", "B"}};
    const ModuleProvenance child{"/work/pkg-a/child.on1xm", owner.package_identity,
                                 {"A", "B", "Child"}};
    const ModuleProvenance sibling{"/work/pkg-a/sibling.on1xm", owner.package_identity,
                                   {"A", "Bigger"}};
    const ModuleProvenance wrong_prefix{"/work/pkg-a/prefix.on1xm", owner.package_identity,
                                        {"AB", "B"}};
    const ModuleProvenance ancestor{"/work/pkg-a/parent.on1xm", owner.package_identity, {"A"}};
    const ModuleProvenance unnamed{"/work/pkg-a/entry.on1x", owner.package_identity, {}};
    const ModuleProvenance other_package{"/work/pkg-b/inc.on1xm", "/work/pkg-b/On1xfile",
                                         owner.module_components};
    const ModuleProvenance unknown_package_a{owner.canonical_file, {}, owner.module_components};
    const ModuleProvenance unknown_package_b{other_package.canonical_file, {},
                                             owner.module_components};
    const ModuleProvenance unverified_file{"inc.on1xm", {}, owner.module_components};
    const ModuleProvenance malformed_scope{"/work/pkg-a/malformed.on1xm", owner.package_identity,
                                           {"A", "B.Child"}};

    const VisibilityRule unannotated{};
    const VisibilityRule public_rule{Visibility::Public};
    const VisibilityRule private_rule{Visibility::Private};
    const VisibilityRule shared0{Visibility::Shared, 0};
    const VisibilityRule shared1{Visibility::Shared, 1};
    const VisibilityRule shared2{Visibility::Shared, 2};
    const VisibilityRule shared3{Visibility::Shared, 3};
    const VisibilityRule unknown_kind{static_cast<Visibility>(99)};
    const VisibilityRule invalid_private{Visibility::Private, 1};

    expect(module_visibility_access(private_rule, owner, same_file) == Access::Allowed,
           "canonical file identity permits private access through an alias");
    expect(module_visibility_access(private_rule, owner, peer, true) == Access::Private,
           "another file of the same module cannot access private declarations");
    expect(module_visibility_access(private_rule, owner, other_package, true) == Access::Private,
           "dependency admission cannot expose private declarations");
    expect(module_visibility_access(private_rule, unverified_file, unverified_file) == Access::Private,
           "matching relative paths do not prove canonical file identity");
    expect(module_visibility_access(private_rule, {}, {}) == Access::Private,
           "missing file identities do not compare as the same file");

    expect(module_visibility_access(shared0, owner, peer) == Access::Allowed,
           "shared at elevation zero accepts another file in the same module");
    expect(module_visibility_access(shared0, owner, child) == Access::Allowed,
           "shared scope admits descendants");
    expect(module_visibility_access(shared0, owner, sibling, true) == Access::SharedScope,
           "a string prefix within one component is not a module ancestor");
    expect(module_visibility_access(shared0, owner, ancestor) == Access::SharedScope,
           "the requester must have every component of the shared prefix");
    expect(module_visibility_access(shared0, owner, unnamed) == Access::SharedScope,
           "unnamed requesters cannot claim a matching module prefix");
    expect(module_visibility_access(shared0, owner, malformed_scope) == Access::SharedScope,
           "a qualified spelling is not a single module component");
    expect(module_visibility_access(shared1, owner, sibling) == Access::Allowed,
           "elevation admits siblings by dropping a complete module component");
    expect(module_visibility_access(shared1, owner, ancestor) == Access::Allowed,
           "the elevated ancestor itself is in scope");
    expect(module_visibility_access(shared1, owner, wrong_prefix) == Access::SharedScope,
           "elevation still requires complete components");
    expect(module_visibility_access(shared2, owner, wrong_prefix) == Access::Allowed,
           "elevation to package root admits other same-package modules");
    expect(module_visibility_access(shared2, owner, unnamed) == Access::Allowed,
           "elevation to package root needs package but not module membership");
    expect(module_visibility_access(shared2, owner, other_package, true) == Access::SharedPackage,
           "no elevation crosses package identity, even with identical module names");
    expect(module_visibility_access(shared0, unknown_package_a, unknown_package_b, true) ==
               Access::SharedPackage,
           "two missing package identities do not prove same-package access");

    expect(validate_module_visibility(shared3, owner) == Issue::ElevationTooHigh,
           "elevation past the package root is invalid");
    expect(validate_module_visibility({Visibility::Shared, std::numeric_limits<std::size_t>::max()},
                                      owner) == Issue::ElevationTooHigh,
           "large elevation rejects without arithmetic overflow");
    expect(validate_module_visibility(shared0, unnamed) == Issue::MissingModuleScope,
           "shared declaration requires an established declaring module");
    expect(validate_module_visibility(invalid_private, owner) == Issue::UnexpectedElevation,
           "elevation belongs only to shared declarations");
    expect(validate_module_visibility(unknown_kind, owner) == Issue::InvalidKind,
           "unknown visibility kinds fail closed");
    expect(module_visibility_access(shared3, owner, same_file, true) == Access::InvalidVisibility,
           "same-file identity does not excuse an invalid rule");

    for (const auto rule : {unannotated, public_rule}) {
        expect(module_visibility_access(rule, owner, peer) == Access::Allowed,
               "public declarations are visible within the package without a dependency edge");
        expect(module_visibility_access(rule, owner, other_package) == Access::DependencyNotAdmitted,
               "public declarations do not authorize a new dependency edge");
        expect(module_visibility_access(rule, owner, other_package, true) == Access::Allowed,
               "public declaration is reachable over an admitted dependency");
        expect(module_visibility_access(rule, unknown_package_a, unknown_package_b) ==
                   Access::DependencyNotAdmitted,
               "unproven package membership cannot bypass public dependency admission");
    }

    expect_rule(merge_module_visibility(unannotated, unannotated), unannotated,
                "unannotated declarations keep the default public spelling state");
    for (const auto explicit_rule : {public_rule, private_rule, shared0, shared1}) {
        expect_rule(merge_module_visibility(explicit_rule, unannotated), explicit_rule,
                    "an unannotated interface inherits the implementation annotation");
        expect_rule(merge_module_visibility(unannotated, explicit_rule), explicit_rule,
                    "an unannotated implementation inherits the interface annotation");
        expect_rule(merge_module_visibility(explicit_rule, explicit_rule), explicit_rule,
                    "equal explicit declarations agree");
    }
    expect(!merge_module_visibility(private_rule, public_rule),
           "an explicit interface cannot widen an explicit private implementation");
    expect(!merge_module_visibility(public_rule, private_rule),
           "different explicit declarations conflict in either direction");
    expect(!merge_module_visibility(shared0, shared1),
           "different explicit shared elevations conflict");
    expect(!merge_module_visibility(unannotated, invalid_private),
           "an absent annotation cannot inherit a malformed rule");
}

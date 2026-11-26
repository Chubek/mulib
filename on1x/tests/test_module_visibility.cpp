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
    const ModuleProvenance owner{"/work/pkg-a/inc.on1xm", "/work/pkg-a/On1xfile",
                                 {"foo", "bar", "buzz"}};
    const ModuleProvenance same_file{owner.canonical_file, {}, {"Alias"}};
    const ModuleProvenance peer{"/work/pkg-a/peer.on1xm", owner.package_identity,
                                owner.module_components};
    const ModuleProvenance child{"/work/pkg-a/child.on1xm", owner.package_identity,
                                 {"foo", "bar", "buzz", "child"}};
    const ModuleProvenance sibling{"/work/pkg-a/sibling.on1xm", owner.package_identity,
                                   {"foo", "bar", "buzzing"}};
    const ModuleProvenance cousin{"/work/pkg-a/cousin.on1xm", owner.package_identity,
                                  {"foo", "barrier", "buzz"}};
    const ModuleProvenance wrong_prefix{"/work/pkg-a/prefix.on1xm", owner.package_identity,
                                        {"foobar", "bar", "buzz"}};
    const ModuleProvenance parent{"/work/pkg-a/parent.on1xm", owner.package_identity,
                                  {"foo", "bar"}};
    const ModuleProvenance ancestor{"/work/pkg-a/ancestor.on1xm", owner.package_identity, {"foo"}};
    const ModuleProvenance unnamed{"/work/pkg-a/entry.on1x", owner.package_identity, {}};
    const ModuleProvenance other_package{"/work/pkg-b/inc.on1xm", "/work/pkg-b/On1xfile",
                                         owner.module_components};
    const ModuleProvenance unknown_package_a{owner.canonical_file, {}, owner.module_components};
    const ModuleProvenance unknown_package_b{other_package.canonical_file, {},
                                             owner.module_components};
    const ModuleProvenance unverified_file{"inc.on1xm", {}, owner.module_components};
    const ModuleProvenance malformed_scope{"/work/pkg-a/malformed.on1xm", owner.package_identity,
                                           {"foo", "bar", "buzz.child"}};

    const VisibilityRule unannotated{};
    const VisibilityRule public_rule{Visibility::Public};
    const VisibilityRule private_rule{Visibility::Private};
    const VisibilityRule shared_omitted{Visibility::Shared};
    const VisibilityRule shared0{Visibility::Shared, 0};
    const VisibilityRule shared1{Visibility::Shared, 1};
    const VisibilityRule shared2{Visibility::Shared, 2};
    const VisibilityRule shared3{Visibility::Shared, 3};
    const VisibilityRule shared4{Visibility::Shared, 4};
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

    expect(shared_omitted.elevation == 1, "omitted shared elevation is exactly one");
    expect(module_visibility_access(shared_omitted, owner, peer) == Access::Allowed,
           "omission selects foo.bar.buzz");
    expect(module_visibility_access(shared_omitted, owner, sibling) == Access::SharedScope,
           "omission cannot open foo.bar");
    expect(module_visibility_access(shared1, owner, peer) == Access::Allowed,
           "shared 1 selects foo.bar.buzz");
    expect(module_visibility_access(shared1, owner, child) == Access::Allowed,
           "shared 1 admits descendants of foo.bar.buzz");
    expect(module_visibility_access(shared1, owner, sibling, true) == Access::SharedScope,
           "a string prefix within one component is not a module ancestor");
    expect(module_visibility_access(shared1, owner, parent) == Access::SharedScope,
           "the requester must have every component of the shared prefix");
    expect(module_visibility_access(shared1, owner, malformed_scope) == Access::SharedScope,
           "a qualified spelling is not a single module component");
    expect(module_visibility_access(shared2, owner, sibling) == Access::Allowed,
           "shared 2 admits other modules under foo.bar");
    expect(module_visibility_access(shared2, owner, parent) == Access::Allowed,
           "shared 2 includes foo.bar itself");
    expect(module_visibility_access(shared2, owner, cousin) == Access::SharedScope,
           "shared 2 does not include foo.barrier");
    expect(module_visibility_access(shared2, owner, ancestor) == Access::SharedScope,
           "shared 2 does not include foo itself");
    expect(module_visibility_access(shared3, owner, ancestor) == Access::Allowed,
           "shared 3 includes foo itself");
    expect(module_visibility_access(shared3, owner, cousin) == Access::Allowed,
           "shared 3 includes descendants of foo");
    expect(module_visibility_access(shared3, owner, wrong_prefix) == Access::SharedScope,
           "shared 3 does not grant package-wide access");
    expect(module_visibility_access(shared3, owner, unnamed) == Access::SharedScope,
           "even maximum elevation requires a requester module component");
    expect(module_visibility_access(shared3, owner, other_package, true) == Access::SharedPackage,
           "no elevation crosses package identity, even with identical module names");
    expect(module_visibility_access(shared1, unknown_package_a, unknown_package_b, true) ==
               Access::SharedPackage,
           "two missing package identities do not prove same-package access");

    expect(validate_module_visibility(shared0, owner) == Issue::ElevationTooLow,
           "an explicit zero is invalid");
    expect(module_visibility_access(shared0, owner, same_file) == Access::InvalidVisibility,
           "an explicit zero rejects even when requester and owner are the same file");
    expect(validate_module_visibility(shared4, owner) == Issue::ElevationTooHigh,
           "elevation above the outermost component is invalid");
    expect(validate_module_visibility(shared2, ancestor) == Issue::ElevationTooHigh,
           "a depth-1 module cannot elevate beyond one");
    expect(validate_module_visibility({Visibility::Shared, std::numeric_limits<std::size_t>::max()},
                                      owner) == Issue::ElevationTooHigh,
           "large elevation rejects without arithmetic overflow");
    expect(validate_module_visibility(shared1, unnamed) == Issue::MissingModuleScope,
           "shared declaration requires an established declaring module");
    expect(validate_module_visibility(invalid_private, owner) == Issue::UnexpectedElevation,
           "elevation belongs only to shared declarations");
    expect(validate_module_visibility(unknown_kind, owner) == Issue::InvalidKind,
           "unknown visibility kinds fail closed");
    expect(module_visibility_access(shared4, owner, same_file, true) == Access::InvalidVisibility,
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
    for (const auto explicit_rule : {public_rule, private_rule, shared1, shared2}) {
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
    expect_rule(merge_module_visibility(shared_omitted, shared1), shared1,
                "omitted and explicit shared elevation one agree");
    expect_rule(merge_module_visibility(shared1, shared_omitted), shared1,
                "shared elevation spelling does not matter in either direction");
    expect(!merge_module_visibility(shared1, shared2),
           "different explicit shared elevations conflict");
    expect(!merge_module_visibility(shared_omitted, shared2),
           "omitted shared elevation conflicts with explicit two");
    expect(!merge_module_visibility(shared_omitted, shared0),
           "an explicit zero cannot masquerade as omission");
    expect(!merge_module_visibility(unannotated, invalid_private),
           "an absent annotation cannot inherit a malformed rule");
}

#include <on1x/module_algebra.hpp>
#include "module_rules.hpp"
#include <LibDSL.hpp>

namespace on1x::oml {
namespace {
struct Host : dsl::DSL<Host, dsl::LanguageSynthesizer> {};
const dsl::synth::MetaLanguage& language() {
    static const auto lang = Host::synthesize(
        dsl::synth::Specification::from_text(embedded::rules).to_sexpr());
    return lang;
}
std::string judgment(std::string name, std::initializer_list<bool> facts) {
    std::vector<dsl::SExpr> terms{dsl::SExpr::atom(std::move(name))};
    for (bool fact : facts) terms.push_back(dsl::SExpr::atom(fact ? "true" : "false"));
    auto term = dsl::SExpr::list(std::move(terms));
    language().validate_form(term);
    dsl::synth::MetaEvaluator evaluator;
    return evaluator.evaluate(language().expand(term, {32, 4, 8192}), {32, 8192}).as_syntax().text;
}
using Facts = std::array<bool, 7>;
using Engine = dsl::synth::AttributeEngine<Facts, int>;
const Engine& engine() {
    static const Engine attributes = [] {
        Engine e;
        std::vector<dsl::synth::AttributeKey> deps;
        for (const char* name : {"valid", "file", "private", "shared", "package", "scope", "admitted"}) {
            auto i = deps.size();
            deps.push_back({"request", name});
            e.declare(deps.back(), {}, [i](const auto& c) { return int(c.state()[i]); });
        }
        e.declare({"request", "access"}, deps, [](const auto& c) {
            auto result = judgment("access", {bool(c.at(0)), bool(c.at(1)), bool(c.at(2)),
                bool(c.at(3)), bool(c.at(4)), bool(c.at(5)), bool(c.at(6))});
            for (auto [name, access] : {std::pair{"allowed", ModuleAccess::Allowed},
                    {"invalid-visibility", ModuleAccess::InvalidVisibility}, {"private", ModuleAccess::Private},
                    {"shared-package", ModuleAccess::SharedPackage}, {"shared-scope", ModuleAccess::SharedScope},
                    {"dependency-not-admitted", ModuleAccess::DependencyNotAdmitted}})
                if (result == name) return int(access);
            throw std::logic_error("invalid OML access result");
        });
        return e;
    }();
    return attributes;
}
}
ModuleAccess access(VisibilityRule rule, const ModuleProvenance& owner,
                    const ModuleProvenance& requester, bool dependency_admitted) {
    using namespace module_visibility_detail;
    bool valid = validate_module_visibility(rule, owner) == ModuleVisibilityIssue::None;
    bool shared = rule.kind == DeclarationVisibility::Shared;
    auto depth = valid && shared ? owner.module_components.size() - rule.elevation + 1 : 0;
    Facts facts{valid, same_identity(owner.canonical_file, requester.canonical_file),
        rule.kind == DeclarationVisibility::Private, shared, same_identity(owner.package_identity, requester.package_identity),
        depth && has_module_scope(requester) && requester.module_components.size() >= depth &&
            std::equal(owner.module_components.begin(), owner.module_components.begin() + depth,
                       requester.module_components.begin()), dependency_admitted};
    return static_cast<ModuleAccess>(engine().evaluate(facts, {"request", "access"}, {64, 1}));
}
InsertDecision insert_view(ExistingBinding binding) {
    // The domain has three states. Compile the trusted form once per state,
    // avoiding repeated expansion on the resolver's binding insertion path.
    static const auto decisions = [] {
        std::array<InsertDecision, 3> out;
        for (auto state : {ExistingBinding::Absent, ExistingBinding::Same, ExistingBinding::Different}) {
            auto step = judgment("insert-view", {state != ExistingBinding::Absent, state == ExistingBinding::Same});
            if (step == "insert") out.at(static_cast<size_t>(state)) = InsertDecision::Insert;
            else if (step == "reuse") out.at(static_cast<size_t>(state)) = InsertDecision::Reuse;
            else if (step == "collision") out.at(static_cast<size_t>(state)) = InsertDecision::Collision;
            else throw std::logic_error("invalid OML insert-view result");
        }
        return out;
    }();
    return decisions.at(static_cast<size_t>(binding));
}
bool reexport(ModuleAccess original, ModuleAccess forwarding) {
    return judgment("reexport", {original == ModuleAccess::Allowed, forwarding == ModuleAccess::Allowed}) == "true";
}
namespace {
void bound(std::size_t n) {
    if (n > 4096) throw std::length_error("OML environment budget exceeded");
}
bool identical(const Binding& a, const Binding& b) {
    return a.declaration == b.declaration && a.type == b.type && a.seal == b.seal &&
        a.owner.canonical_file == b.owner.canonical_file && a.owner.package_identity == b.owner.package_identity &&
        a.owner.module_components == b.owner.module_components && a.visibility.kind == b.visibility.kind &&
        a.visibility.elevation == b.visibility.elevation;
}
void validate(const Environment& env) {
    bound(env.size());
    for (const auto& [name, binding] : env)
        if (name.spelling.empty() || binding.declaration.empty() || !binding.owner.canonical_file.is_absolute() ||
            validate_module_visibility(binding.visibility, binding.owner) != ModuleVisibilityIssue::None)
            throw std::invalid_argument("OML invalid binding: " + name.spelling);
}
}
Environment merge(const Environment& left, const Environment& right) {
    validate(left);
    validate(right);
    auto result = left;
    for (const auto& [key, binding] : right) {
        auto old = result.find(key);
        auto step = insert_view(old == result.end() ? ExistingBinding::Absent :
                                identical(old->second, binding) ? ExistingBinding::Same : ExistingBinding::Different);
        if (step == InsertDecision::Reuse) continue;
        if (step != InsertDecision::Insert) throw std::invalid_argument("OML merge collision: " + key.spelling);
        result.emplace(key, binding);
        bound(result.size());
    }
    return result;
}
Environment project(const Environment& env, const std::set<Name>& names, bool hide) {
    validate(env);
    bound(names.size());
    Environment result;
    for (const auto& [key,binding] : env)
        if (judgment("project-member", {names.contains(key), hide}) == "true") result.emplace(key,binding);
    return result;
}
Environment rename(const Environment& env, const std::map<Name,Name>& mapping) {
    validate(env);
    bound(mapping.size());
    for (const auto& [from,to] : mapping)
        if (!env.contains(from) || from.space != to.space || to.spelling.empty())
            throw std::invalid_argument("OML invalid rename: " + from.spelling);
    Environment result;
    for (const auto& [key,binding] : env) {
        const auto& to = mapping.contains(key) ? mapping.at(key) : key;
        if (!result.emplace(to,binding).second) throw std::invalid_argument("OML rename collision: " + to.spelling);
    }
    return result;
}
Environment select(const Environment& exports, const std::vector<Name>& selectors,
                   const ModuleProvenance& requester, bool admitted) {
    validate(exports);
    bound(selectors.size());
    Environment result;
    for (const auto& key : selectors) {
        auto it = exports.find(key);
        bool exists = it != exports.end();
        bool allowed = exists && access(it->second.visibility,it->second.owner,requester,admitted) == ModuleAccess::Allowed;
        auto decision = judgment("select-member", {exists, result.contains(key), allowed});
        if (decision != "selected") throw std::invalid_argument("OML select " + decision + ": " + key.spelling);
        result.emplace(key,it->second);
    }
    return result;
}
std::string_view rule_digest() { return embedded::digest; }
}

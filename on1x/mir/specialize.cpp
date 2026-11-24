#include <on1x/backend/mir_directives.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace on1x::mir {
namespace {
struct Selector {
    std::string_view backend, target;

    bool matches(std::string_view product, std::string_view triple) const {
        return (backend.empty() || backend == product) && (target.empty() || target == triple);
    }
};
enum class Group : unsigned { Arithmetic = 1, Boolean = 2, Slots = 4 };
enum class AtomKind { Capture, Literal };
enum class Proof { Identity, Nonzero, LocalSlot };
struct Activation {
    std::string_view kind, action;
    Selector when;
    Group group;
};
struct Atom {
    AtomKind kind;
    std::int64_t payload; // Capture index, or exact scalar literal.
};
struct Pattern {
    Selector when;
    Group group;
    bool speculative;
    Op op;
    std::size_t arity;
    Atom args[2];
    Atom result;
    Proof proof;
};

#include "mir_policy_db.hpp"

using Constants = std::map<Id, std::int64_t>;
using Aliases = std::map<Id, Id>;
using Rules = std::map<Op, std::vector<const Pattern*>>;

// Every alias points to a verified dominating operand (or to the operand of a
// preceding local store). No CFG edges or parameter definitions are removed.
// Path compression prevents long chains of identities from costing quadratic
// work, including when blocks appear in arbitrary order in the wire form.
Id resolve(Aliases& aliases, Id id) {
    Id result = id;
    for (auto it = aliases.find(result); it != aliases.end(); it = aliases.find(result))
        result = it->second;
    for (auto it = aliases.find(id); it != aliases.end() && it->second != result; it = aliases.find(id)) {
        auto next = it->second;
        it->second = result;
        id = next;
    }
    return result;
}

struct Facts {
    Aliases slots;
    Aliases zero_tests;
    std::set<Id> nonzero;
};

std::optional<Atom> match(const Pattern& p, const Instruction& i,
                         const Constants& constants, const Facts& facts, Id (&captures)[2]) {
    if (i.operands.size() != p.arity) return {};
    for (std::size_t n = 0; n < p.arity; ++n) {
        auto id = i.operands[n];
        auto arg = p.args[n];
        if (arg.kind == AtomKind::Literal) {
            auto known = constants.find(id);
            if (known == constants.end() || known->second != arg.payload) return {};
        } else {
            auto& captured = captures[arg.payload];
            if (captured && captured != id) return {};
            captured = id;
        }
    }
    if (p.proof == Proof::Nonzero) {
        auto id = i.operands[1];
        auto known = constants.find(id);
        if (!facts.nonzero.contains(id) && (known == constants.end() || !known->second)) return {};
    }
    if (p.proof == Proof::LocalSlot) {
        // Slots are initialized/non-addressable in verified MIR. The last
        // same-block store supplies this load's exact scalar value; calls
        // cannot access a caller's frame, and no instruction can alias slots.
        // Deliberately avoid forwarding aggregate copies in this profile.
        if (i.type != Type::I64 && i.type != Type::Bool) return {};
        auto found = facts.slots.find(i.target);
        if (found == facts.slots.end()) return {};
        captures[0] = found->second;
    }
    return p.result;
}

void observe(Facts& facts, const Instruction& i, const Constants& constants) {
    if (i.op == Op::Store) {
        facts.slots[i.target] = i.operands[0];
    } else if (i.op == Op::Ne) {
        for (unsigned n = 0; n < 2; ++n) {
            auto known = constants.find(i.operands[n]);
            if (known != constants.end() && known->second == 0)
                facts.zero_tests.emplace(i.id, i.operands[1 - n]);
        }
    } else if (i.op == Op::Assert) {
        // Kept as an actual trap in the effect chain. Only a completed
        // assertion permits using the fact later in the *same* dynamic block.
        if (auto found = facts.zero_tests.find(i.operands[0]); found != facts.zero_tests.end())
            facts.nonzero.insert(found->second);
    }
}

void rewrite(Function& f, const Rules& rules) {
    if (rules.empty()) return;
    Constants constants;
    Aliases aliases;
    // A constant definition need not occur earlier in storage order. Input
    // verification proves that each operand dominates even such a use.
    for (const auto& b : f.blocks)
        for (const auto& i : b.instructions)
            if (i.op == Op::Constant) constants.emplace(i.id, i.immediate);

    for (auto& b : f.blocks) {
        Facts facts;
        Id token = b.effect;
        std::size_t kept = 0;
        for (std::size_t n = 0; n < b.instructions.size(); ++n) {
            auto& i = b.instructions[n];
            for (auto& operand : i.operands) operand = resolve(aliases, operand);
            bool remove = false;
            if (auto group = rules.find(i.op); group != rules.end()) {
                for (auto p : group->second) {
                    Id captures[2]{};
                    auto replacement = match(*p, i, constants, facts, captures);
                    if (!replacement) continue;
                    if (replacement->kind == AtomKind::Capture) {
                        aliases.emplace(i.id, captures[replacement->payload]);
                        remove = true;
                    } else {
                        i.op = Op::Constant;
                        i.operands.clear();
                        i.immediate = replacement->payload;
                        i.effect_in = i.effect_out = 0;
                        constants.emplace(i.id, i.immediate);
                    }
                    break;
                }
            }
            if (remove) continue;
            observe(facts, i, constants);
            if (effect(i.op) != Effect::Pure) {
                i.effect_in = token;
                token = i.effect_out;
            }
            if (kept != n) b.instructions[kept] = std::move(i);
            ++kept;
        }
        b.instructions.resize(kept);
        b.exit.effect = token;
    }
    // Do this after discovering all aliases, since uses may have been visited
    // before their dominating definitions. Cover every SSA use, not effects,
    // CFG destinations, slots, or call targets.
    for (auto& b : f.blocks) {
        for (auto& i : b.instructions)
            for (auto& operand : i.operands) operand = resolve(aliases, operand);
        b.exit.value = resolve(aliases, b.exit.value);
        for (auto& edge : b.exit.edges)
            for (auto& arg : edge.arguments) arg = resolve(aliases, arg);
    }
}

unsigned executable_version(const Module& module) {
    unsigned needed = legacy_format_version;
    std::set<const std::vector<Type>*> seen;
    auto type = [&](auto&& visit, const Type& t) -> void {
        if (t.is_unsigned() || t == Type::F32) needed = numeric_format_version;
        else if (t == Type::F64) needed = std::max(needed, format_version);
        else if (t.is_tuple()) {
            needed = std::max(needed, format_version);
            if (seen.insert(&t.elements()).second)
                for (const auto& e : t.elements()) visit(visit, e);
        }
    };
    for (const auto& f : module.functions) {
        type(type, f.result);
        for (const auto& s : f.slots) type(type, s.type);
        for (const auto& b : f.blocks) {
            for (const auto& p : b.parameters) type(type, p.type);
            for (const auto& i : b.instructions) {
                type(type, i.type);
                if (i.op == Op::Numeric) return numeric_format_version;
                if (i.op > Op::Assert) needed = std::max(needed, format_version);
            }
        }
        if (needed == numeric_format_version) return needed;
    }
    return needed;
}
}

Module specialize(const Module& module, std::string_view backend,
                  std::string_view target, bool speculate) {
    verify(module);
    Module result = module;
    std::map<Id, unsigned> groups;
    unsigned global = 0;
    for (const auto& d : module.directives) {
        if ((d.speculative && !speculate) || !Selector{d.backend, d.target}.matches(backend, target))
            continue;
        for (const auto& activation : activations) {
            if (activation.kind != d.kind || activation.action != d.action ||
                !activation.when.matches(backend, target)) continue;
            (d.function ? groups[d.function] : global) |= static_cast<unsigned>(activation.group);
        }
    }
    // Hints opt functions into task-specific optimization groups. In particular,
    // a local-memory hint does not enable arithmetic/speculation, nor does a
    // canonicalized hint bypass checked arithmetic or effect proofs.
    std::map<unsigned, Rules> plans;
    for (auto& f : result.functions) {
        auto group = groups.find(f.id);
        unsigned enabled = global | (group == groups.end() ? 0u : group->second);
        if (!enabled) continue;
        auto [plan, fresh] = plans.try_emplace(enabled);
        if (fresh)
            for (const auto& p : patterns)
                if ((enabled & static_cast<unsigned>(p.group)) && (!p.speculative || speculate) &&
                    p.when.matches(backend, target))
                    plan->second[p.op].push_back(&p);
        rewrite(f, plan->second);
    }
    result.directives.clear();
    if (result.version == directive_format_version) result.version = executable_version(result);
    verify(result);
    return result;
}
}

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
enum class Bank : unsigned; // Defined by the generated policy database.
enum class AtomKind { Capture, Literal };
enum class Proof {
    Identity, Nonzero, LocalStore, LocalLoad, DoubleNot, BoolOperands,
    DoubleCheckedNeg, BoolComplement, NonzeroComparison,
    LocalStoreWord, LocalLoadWord, LocalStoreF32, LocalLoadF32,
    LocalStoreF64, LocalLoadF64, LocalStoreTuple, LocalLoadTuple, RedundantStore
};
struct Activation {
    std::string_view kind, action;
    Selector when;
    Bank bank;
};
struct Atom {
    AtomKind kind;
    std::int64_t payload; // Capture index, or exact scalar literal.
};
struct Pattern {
    Selector when;
    Bank bank;
    bool speculative;
    Op op;
    std::size_t arity;
    Atom args[2];
    Atom result;
    Proof proof;
};

#include "mir_policy_db.hpp"
#include "mir_policy_memgraph.hpp"
#include "mir_policy_meta.hpp"

struct Constant {
    std::int64_t value;
    bool boolean;
};
using Constants = std::map<Id, Constant>;
using Aliases = std::map<Id, Id>;
using Rules = std::map<Op, std::vector<const Pattern*>>;

// Every alias points to a verified dominating definition. No CFG edges or
// parameter definitions are removed.
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
    struct SlotValue {
        Id value;
        Proof source;
    };
    std::map<Id, SlotValue> slots;
    Aliases negations;
    Aliases checked_negations;
    Aliases zero_tests;
    std::set<Id> nonzero;
};

bool is_nonzero(Id id, const Constants& constants, const Facts& facts) {
    auto known = constants.find(id);
    return facts.nonzero.contains(id) || (known != constants.end() && known->second.value != 0);
}

// These proofs use a completed store/load of the same verified frame slot.
// Only the local MIR value is forwarded; nothing reads or moves backing storage.
std::optional<Proof> slot_source(Proof proof, const Type& type) {
    switch (proof) {
    case Proof::LocalStore: case Proof::LocalLoad:
        if (type == Type::I64 || type == Type::Bool) return proof;
        break;
    case Proof::LocalStoreWord: case Proof::LocalLoadWord:
        if (type.is_unsigned()) return proof == Proof::LocalStoreWord ? Proof::LocalStore : Proof::LocalLoad;
        break;
    case Proof::LocalStoreF32: case Proof::LocalLoadF32:
        if (type == Type::F32) return proof == Proof::LocalStoreF32 ? Proof::LocalStore : Proof::LocalLoad;
        break;
    case Proof::LocalStoreF64: case Proof::LocalLoadF64:
        if (type == Type::F64) return proof == Proof::LocalStoreF64 ? Proof::LocalStore : Proof::LocalLoad;
        break;
    case Proof::LocalStoreTuple: case Proof::LocalLoadTuple:
        if (type.is_tuple()) return proof == Proof::LocalStoreTuple ? Proof::LocalStore : Proof::LocalLoad;
        break;
    default: break;
    }
    return {};
}

std::optional<Atom> match(const Pattern& p, const Instruction& i,
                         const Constants& constants, const Facts& facts, Id (&captures)[2]) {
    if (i.operands.size() != p.arity) return {};
    for (std::size_t n = 0; n < p.arity; ++n) {
        auto id = i.operands[n];
        auto arg = p.args[n];
        if (arg.kind == AtomKind::Literal) {
            auto known = constants.find(id);
            if (known == constants.end() || known->second.value != arg.payload ||
                (p.proof == Proof::BoolOperands && !known->second.boolean)) return {};
        } else {
            auto& captured = captures[arg.payload];
            if (captured && captured != id) return {};
            captured = id;
        }
    }
    if (p.proof == Proof::Nonzero && !is_nonzero(i.operands[1], constants, facts)) return {};
    if (p.proof == Proof::NonzeroComparison) {
        const auto literal = p.args[0].kind == AtomKind::Literal ? 0u : 1u;
        if (constants.at(i.operands[literal]).boolean ||
            !is_nonzero(i.operands[1 - literal], constants, facts)) return {};
    }
    if (i.op == Op::Load) {
        // The last observed same-block store or load supplies this load's
        // exact value. Calls cannot access a caller's frame, and no instruction
        // can alias slots. A newer same-slot store always replaces the fact.
        const auto source = slot_source(p.proof, i.type);
        if (!source) return {};
        auto found = facts.slots.find(i.target);
        if (found == facts.slots.end() || found->second.source != *source) return {};
        captures[p.result.payload] = found->second.value;
    }
    if (p.proof == Proof::RedundantStore) {
        // Never drop initialization. A retained same-block store must already
        // have written this exact immutable SSA value to this very slot.
        auto found = facts.slots.find(i.target);
        if (found == facts.slots.end() || found->second.source != Proof::LocalStore ||
            found->second.value != i.operands[0]) return {};
    }
    if (p.proof == Proof::DoubleNot || p.proof == Proof::DoubleCheckedNeg) {
        // A completed checked negation's result cannot be INT64_MIN. The
        // first check stays in the block; only its trap-free inverse goes.
        const auto& seen = p.proof == Proof::DoubleNot ? facts.negations : facts.checked_negations;
        auto found = seen.find(i.operands[0]);
        if (found == seen.end()) return {};
        captures[p.result.payload] = found->second;
    }
    if (p.proof == Proof::BoolComplement) {
        auto complements = [&](Id a, Id b) {
            auto it = facts.negations.find(a);
            return it != facts.negations.end() && it->second == b;
        };
        if (!complements(i.operands[0], i.operands[1]) &&
            !complements(i.operands[1], i.operands[0])) return {};
    }
    return p.result;
}

void observe(Facts& facts, const Instruction& i, const Constants& constants) {
    if (i.op == Op::Store) {
        facts.slots[i.target] = {i.operands[0], Proof::LocalStore};
    } else if (i.op == Op::Load) {
        facts.slots[i.target] = {i.id, Proof::LocalLoad};
    } else if (i.op == Op::Not) {
        facts.negations.emplace(i.id, i.operands[0]);
    } else if (i.op == Op::Neg) {
        facts.checked_negations.emplace(i.id, i.operands[0]);
    } else if (i.op == Op::Ne) {
        for (unsigned n = 0; n < 2; ++n) {
            auto known = constants.find(i.operands[n]);
            if (known != constants.end() && !known->second.boolean && known->second.value == 0)
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
            if (i.op == Op::Constant) constants.emplace(i.id, Constant{i.immediate, i.type == Type::Bool});

    for (auto& b : f.blocks) {
        Facts facts;
        Id token = b.effect;
        std::size_t kept = 0;
        for (std::size_t n = 0; n < b.instructions.size(); ++n) {
            auto& i = b.instructions[n];
            for (auto& operand : i.operands) operand = resolve(aliases, operand);
            bool remove = false;
            if (auto candidates = rules.find(i.op); candidates != rules.end()) {
                for (auto p : candidates->second) {
                    Id captures[2]{};
                    auto replacement = match(*p, i, constants, facts, captures);
                    if (!replacement) continue;
                    if (replacement->kind == AtomKind::Capture) {
                        // A redundant store has no SSA result to substitute.
                        if (p->proof != Proof::RedundantStore)
                            aliases.emplace(i.id, captures[replacement->payload]);
                        remove = true;
                    } else {
                        i.op = Op::Constant;
                        i.operands.clear();
                        i.immediate = replacement->payload;
                        i.effect_in = i.effect_out = 0;
                        constants.emplace(i.id, Constant{i.immediate, i.type == Type::Bool});
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
    // Legacy banks keep their existing activation/host-opt-in semantics.
    // The new actions require a matching directive of the pattern's mode for
    // that exact function; a safe request never silently unlocks speculation.
    struct Scope { unsigned safe = 0, speculative = 0; };
    std::map<Id, Scope> scoped_banks;
    unsigned global = 0;
    for (const auto& d : module.directives) {
        if ((d.speculative && !speculate) || !Selector{d.backend, d.target}.matches(backend, target))
            continue;
        if (d.kind == "rewrite" && (d.action == memgraph_local_action || d.action == meta_canonical_action)) {
            auto& scope = scoped_banks[d.function]; // Verification requires a nonzero, existing ID.
            const auto bank = d.action == memgraph_local_action ? Bank::MemgraphLocal : Bank::MetaCanonical;
            (d.speculative ? scope.speculative : scope.safe) |= static_cast<unsigned>(bank);
            continue;
        }
        for (const auto& activation : activations) {
            if (activation.kind != d.kind || activation.action != d.action ||
                !activation.when.matches(backend, target)) continue;
            (d.function ? scoped_banks[d.function].safe : global) |= static_cast<unsigned>(activation.bank);
        }
    }
    // Hints opt functions into task-specific rewrite banks. In particular,
    // a local-memory hint does not enable arithmetic/speculation, nor does a
    // canonicalized hint bypass checked arithmetic or effect proofs.
    std::map<std::pair<unsigned, unsigned>, Rules> plans;
    for (auto& f : result.functions) {
        auto scope = scoped_banks.find(f.id);
        unsigned safe = global | (scope == scoped_banks.end() ? 0u : scope->second.safe);
        unsigned speculative = scope == scoped_banks.end() ? 0u : scope->second.speculative;
        if (!(safe | speculative)) continue;
        auto [plan, fresh] = plans.try_emplace(std::pair{safe, speculative});
        if (fresh) {
            auto add = [&](const Pattern& p, unsigned enabled) {
                if ((enabled & static_cast<unsigned>(p.bank)) && p.when.matches(backend, target))
                    plan->second[p.op].push_back(&p);
            };
            for (const auto& p : patterns)
                if (!p.speculative || speculate) add(p, safe);
            for (const auto& p : MemgraphLocalPatterns) add(p, p.speculative ? speculative : safe);
            for (const auto& p : MetaCanonicalPatterns) add(p, p.speculative ? speculative : safe);
        }
        rewrite(f, plan->second);
    }
    result.directives.clear();
    if (result.version == directive_format_version) result.version = executable_version(result);
    verify(result);
    return result;
}
}

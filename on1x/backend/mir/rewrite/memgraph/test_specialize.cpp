#include <on1x/backend/mir_c11.hpp>
#include <on1x/backend/mir_directives.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace m = on1x::mir;
using on1x::Type;
using enum m::Op;
constexpr auto unit = Type::Void;
constexpr auto boolean = Type::Bool;
constexpr char backend[] = "mir-c11";
constexpr char target[] = "x86_64-unknown-linux-gnu";
const on1x::Location at{"memgraph.mir", 0, 1, 1};

void require(bool ok, const char* test, const char* message) {
    if (!ok) throw std::runtime_error(std::string(test) + ": " + message);
}

struct Fixture {
    m::Module module;
    m::Id next = 1;
    m::Id token = 0;

    Fixture() { module.version = m::numeric_format_version; }

    m::Id id() { return ++next; }
    m::Function& function() { return module.functions.back(); }
    m::Block& block() { return function().blocks.back(); }

    m::Id add_block(std::initializer_list<Type> args) {
        auto block_id = id();
        token = id();
        function().blocks.push_back({block_id, token, at, {}, {}, {}});
        for (const auto& arg : args) block().parameters.push_back({id(), arg, at});
        return block_id;
    }

    m::Id add_function(const char* name, const Type& type, std::initializer_list<Type> args) {
        auto function_id = id();
        module.functions.push_back({function_id, name, type, at, {}, {}});
        add_block(args);
        return function_id;
    }

    m::Id emit(m::Op op, const Type& type, std::vector<m::Id> args = {}, m::Id target = 0) {
        auto instruction_id = id();
        auto where = at;
        where.line = instruction_id;
        m::Instruction i{instruction_id, type, op, std::move(args), 0, target, 0, 0, where};
        if (m::effect(op) != m::Effect::Pure) {
            i.effect_in = token;
            token = i.effect_out = id();
        }
        block().instructions.push_back(std::move(i));
        return instruction_id;
    }
};

struct Case {
    enum Mode { None, Safe, Speculative };
    const char* name;
    Type type;
    Mode mode;
    m::RuntimeValue stored, updated;
};

struct Matches {
    m::Id start, first, different_slot, forwarded, repeated, updated;
};

Matches exercise(Fixture& f, const Case& test, m::Id callee) {
    const auto& type = test.type;
    auto result = Type::tuple({type, type, type, type});
    f.add_function(test.name, result, {type, type, boolean});
    auto x = f.block().parameters[0].id;
    auto y = f.block().parameters[1].id;
    auto should_return = f.block().parameters[2].id;
    auto primary = f.id(), other = f.id();
    f.function().slots = {{primary, type, at}, {other, type, at}};
    f.emit(Store, unit, {x}, primary);
    f.emit(Store, unit, {y}, other);
    auto forwarded = f.emit(Load, type, {}, primary);
    auto entry_token = f.token;

    auto body = f.add_block({type});
    f.function().blocks.front().exit = {f.id(), m::Exit::Jump, 0, entry_token, {{body, {forwarded}}}, at};
    auto from_edge = f.block().parameters[0].id;
    // The predecessor's stores do not establish local facts. In particular a
    // fact for primary cannot stand in for a load of the equally typed other.
    auto first = f.emit(Load, type, {}, primary);
    auto different_slot = f.emit(Load, type, {}, other);
    f.emit(Store, unit, {y}, other); // Different-slot write does not kill primary.
    auto condition = f.emit(Call, boolean, {should_return}, callee);
    f.emit(Assert, unit, {condition});
    auto repeated = f.emit(Load, type, {}, primary);
    f.emit(Store, unit, {y}, primary); // This time the last load is stale.
    auto updated = f.emit(Load, type, {}, primary);
    f.emit(Print, unit, {condition});
    auto packed = f.emit(Tuple, result, {from_edge, repeated, different_slot, updated});
    f.block().exit = {f.id(), m::Exit::Return, packed, f.token, {}, at};
    return {x, first, different_slot, forwarded, repeated, updated};
}

void check_rewrite(const m::Function& before, const m::Function& after, const Matches& ids, bool active) {
    const auto* name = before.name.c_str();
    require(after.blocks.size() == before.blocks.size(), name, "block count changed");
    for (std::size_t b = 0; b < before.blocks.size(); ++b) {
        const auto& old = before.blocks[b];
        const auto& block = after.blocks[b];
        require(block.id == old.id && block.effect == old.effect, name, "block identity changed");
        std::size_t kept = 0;
        auto token = block.effect;
        for (const auto& original : old.instructions) {
            if (active && (original.id == ids.forwarded || original.id == ids.repeated ||
                           original.id == ids.updated))
                continue;
            require(kept < block.instructions.size(), name, "removed a required instruction");
            const auto& i = block.instructions[kept++];
            require(i.id == original.id && i.op == original.op && i.target == original.target &&
                    i.where.line == original.where.line && i.effect_out == original.effect_out,
                    name, "changed or reordered a retained instruction");
            if (m::effect(i.op) != m::Effect::Pure) {
                require(i.effect_in == token, name, "broken effect chain");
                token = i.effect_out;
            }
        }
        require(kept == block.instructions.size() && block.exit.effect == token, name,
                "unexpected instruction or terminator effect");
    }
    require(after.blocks[0].exit.edges[0].arguments ==
                std::vector<m::Id>{active ? ids.start : ids.forwarded},
            name, "store forwarding did not update the edge argument");
    const auto& body = after.blocks[1];
    const auto& operands = body.instructions.back().operands;
    require(operands == std::vector<m::Id>{body.parameters[0].id, active ? ids.first : ids.repeated,
                                          ids.different_slot,
                                          active ? before.blocks[0].parameters[1].id : ids.updated},
            name, "returned wrong SSA value for the slot or intervening store");
}

bool same_value(const m::RuntimeValue& left, const m::RuntimeValue& right) {
    if (left.scalar != right.scalar || left.elements.size() != right.elements.size()) return false;
    for (std::size_t i = 0; i < left.elements.size(); ++i)
        if (!same_value(left.elements[i], right.elements[i])) return false;
    return true;
}

void check_evaluation(const m::Module& before, const m::Module& after, const Case& test) {
    for (const auto flag : {0, 1}) {
        const std::vector<m::RuntimeValue> args{test.stored, test.updated, {flag, {}}};
        auto original = m::evaluate_values(before, test.name, args);
        auto selected = m::evaluate_values(after, test.name, args);
        require(selected.status == original.status && selected.type == original.type &&
                selected.output == original.output && selected.diagnostic == original.diagnostic &&
                same_value(selected.data, original.data),
                test.name, "rewrite changed exact bits, output, or trap");
        if (flag) {
            require(selected.status == m::Status::Returned && selected.output == "true\ntrue\n" &&
                    same_value(selected.data, {0, {test.stored, test.stored, test.updated, test.updated}}),
                    test.name, "wrong memory result or missing effects");
        } else {
            require(selected.status == m::Status::Trapped && selected.output == "false\n" &&
                    selected.where.file == original.where.file && selected.where.offset == original.where.offset &&
                    selected.where.line == original.where.line && selected.where.column == original.where.column,
                    test.name, "assertion moved, was removed, or did not stop subsequent effects");
        }
    }
}

void test_memgraph() {
    using enum Case::Mode;
    constexpr auto f64_neg_zero = std::numeric_limits<std::int64_t>::min();
    constexpr auto f64_nan = INT64_C(0x7ff8000000000000);
    constexpr auto f32_neg_zero = INT64_C(0x80000000);
    constexpr auto f32_nan = INT64_C(0x7fc00000);
    const auto nested = Type::tuple({Type::U64, Type::tuple({Type::F32, Type::F64})});
    const Case cases[] = {
        {"u8_slots", Type::U8, Safe, {255, {}}, {1, {}}},
        {"u16_slots", Type::U16, Safe, {65535, {}}, {2, {}}},
        {"u32_slots", Type::U32, Safe, {INT64_C(0xffffffff), {}}, {3, {}}},
        {"u64_slots", Type::U64, Safe, {-1, {}}, {4, {}}},
        {"f32_slots", Type::F32, Safe, {f32_neg_zero, {}}, {f32_nan, {}}},
        {"f64_slots", Type::F64, Safe, {f64_nan, {}}, {f64_neg_zero, {}}},
        {"tuple_slots", nested, Speculative,
            {0, {{-1, {}}, {0, {{f32_neg_zero, {}}, {f64_nan, {}}}}}},
            {0, {{1, {}}, {0, {{f32_nan, {}}, {f64_neg_zero, {}}}}}}},
        {"i64_slots", Type::I64, None, {-43, {}}, {5, {}}},
        {"bool_slots", Type::Bool, None, {0, {}}, {1, {}}},
        {"unscoped_slots", Type::U16, None, {65535, {}}, {6, {}}},
    };
    Fixture f;
    auto callee = f.add_function("trace", boolean, {boolean});
    auto condition = f.block().parameters[0].id;
    f.emit(Print, unit, {condition});
    f.block().exit = {f.id(), m::Exit::Return, condition, f.token, {}, at};

    std::vector<Matches> matches;
    for (const auto& test : cases) matches.push_back(exercise(f, test, callee));
    f.add_function("main", unit, {});
    f.block().exit = {f.id(), m::Exit::Return, 0, f.token, {}, at};
    const auto& baseline = f.module;
    auto safe_request = baseline, spec_request = baseline;
    safe_request.version = spec_request.version = m::directive_format_version;
    for (std::size_t i = 0; i < std::size(cases) - 1; ++i) {
        auto function = baseline.functions[i + 1].id;
        safe_request.directives.push_back({backend, target, "rewrite", m::memgraph_local_action, function, false});
        spec_request.directives.push_back({backend, target, "rewrite", m::memgraph_local_action, function, true});
    }
    auto both_request = safe_request;
    both_request.directives.insert(both_request.directives.end(), spec_request.directives.begin(),
                                   spec_request.directives.end());
    auto wire = m::serialize(both_request);
    both_request = m::deserialize(wire);
    require(m::serialize(both_request) == wire, "activation", "MIR v4 directives did not round-trip");

    auto safe = m::specialize(safe_request, backend, target);
    auto speculative = m::specialize(spec_request, backend, target, true);
    auto both = m::specialize(both_request, backend, target, true);
    require(m::structurally_equal(m::specialize(safe_request, backend, target, true), safe) &&
            m::structurally_equal(m::specialize(both_request, backend, target, false), safe) &&
            m::structurally_equal(m::specialize(spec_request, backend, target, false), baseline),
            "activation", "directive mode and host opt-in were not independent");
    require(both.directives.empty() && both.version == m::numeric_format_version &&
            m::structurally_equal(m::specialize(baseline, backend, target, true), baseline),
            "activation", "directives not consumed or host opt-in alone opened the bank");

    auto wildcard = both_request;
    for (auto& d : wildcard.directives) {
        d.backend.clear();
        d.target.clear();
    }
    require(m::structurally_equal(m::specialize(wildcard, backend, target, true), both),
            "activation", "wildcard directive should allow a matching exact rule selector");
    for (const auto& [product, triple] : {std::pair{"mir-vm", target}, {"MIR-C11", target},
                                          {backend, ""}, {backend, "x86_64-linux-gnu"}})
        require(m::structurally_equal(m::specialize(both_request, product, triple, true), baseline) &&
                m::structurally_equal(m::specialize(wildcard, product, triple, true), baseline),
                "activation", "directive or rule accepted an inexact selector");

    require(m::emit_directed_c(both_request, {}, target, false) == m::emit_c(safe) &&
            m::emit_directed_c(both_request, {}, target, true) == m::emit_c(both),
            "activation", "C11 emission did not select the requested modes");
    for (std::size_t i = 0; i < std::size(cases); ++i) {
        const auto& original = baseline.functions[i + 1];
        check_rewrite(original, safe.functions[i + 1], matches[i], cases[i].mode == Safe);
        check_rewrite(original, speculative.functions[i + 1], matches[i], cases[i].mode == Speculative);
        check_rewrite(original, both.functions[i + 1], matches[i], cases[i].mode != None);
        check_evaluation(baseline, both, cases[i]);
    }
}

void test_redundant_store() {
    Fixture f;
    const auto word = Type::U64;
    const auto owner = f.add_function("store", word, {word, word, boolean});
    const auto x = f.block().parameters[0].id, y = f.block().parameters[1].id;
    const auto flag = f.block().parameters[2].id;
    const auto slot = f.id(), other = f.id();
    f.function().slots = {{slot, word, at}, {other, word, at}};
    const auto first = f.emit(Store, unit, {x}, slot);
    const auto first_other = f.emit(Store, unit, {x}, other);
    const auto check = f.emit(Assert, unit, {flag});
    f.emit(Store, unit, {x}, slot);
    const auto changed = f.emit(Store, unit, {y}, slot);
    const auto restored = f.emit(Store, unit, {x}, slot);
    const auto value = f.emit(Load, word, {}, slot);
    f.block().exit = {f.id(), m::Exit::Return, value, f.token, {}, at};

    auto requested = f.module;
    requested.version = m::directive_format_version;
    requested.directives.push_back({backend, target, "rewrite", m::memgraph_local_action, owner, true});
    require(m::structurally_equal(m::specialize(requested, backend, target), f.module),
            "store", "redundant store rewrite ran without opt-in");
    auto selected = m::specialize(requested, backend, target, true);
    const auto& block = selected.functions[0].blocks[0];
    auto token = block.effect;
    std::vector<m::Id> kept;
    for (const auto& inst : block.instructions) {
        kept.push_back(inst.id);
        require(inst.effect_in == token, "store", "broken store effect chain");
        token = inst.effect_out;
    }
    require(kept == std::vector<m::Id>{first, first_other, check, changed, restored, value}
            && block.exit.effect == token, "store", "removed a required store or kept the redundant one");
    for (auto condition : {0, 1}) {
        const std::vector<m::RuntimeValue> args{{-1, {}}, {42, {}}, {condition, {}}};
        const auto before = m::evaluate_values(f.module, "store", args);
        const auto after = m::evaluate_values(selected, "store", args);
        require(before.status == after.status && before.where.line == after.where.line
                && same_value(before.data, after.data),
                "store", "rewrite changed the result or assertion trap");
    }
}
}

int main() {
    try {
        test_memgraph();
        test_redundant_store();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#include <on1x/backend/mir_c11.hpp>
#include <on1x/backend/mir_directives.hpp>

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace m = on1x::mir;
using on1x::Type;
using enum m::Op;
constexpr auto i64 = Type::I64;
constexpr auto boolean = Type::Bool;
constexpr auto unit = Type::Void;
constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
constexpr auto target = "x86_64-unknown-linux-gnu";
const on1x::Location at{"meta-canonical.mir", 0, 1, 1};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    m::Module module;
    m::Id next = 1, token = 0;

    m::Id id() { return ++next; }
    m::Function& function() { return module.functions.back(); }
    m::Block& block() { return function().blocks.back(); }

    m::Id start_block(std::initializer_list<Type> parameters = {}) {
        auto label = id();
        token = id();
        function().blocks.push_back({label, token, at, {}, {}, {}});
        for (auto type : parameters) block().parameters.push_back({id(), type, at});
        return label;
    }

    m::Id start_function(const char* name, Type result, std::initializer_list<Type> parameters) {
        auto label = id();
        module.functions.push_back({label, name, result, at, {}, {}});
        start_block(parameters);
        return label;
    }

    m::Id emit(m::Op op, Type type, std::vector<m::Id> operands = {}, std::int64_t immediate = 0,
               m::Id callee = 0) {
        auto label = id();
        auto where = at;
        where.line = label;
        m::Instruction instruction{label, type, op, std::move(operands), immediate, callee, 0, 0, where};
        if (m::effect(op) != m::Effect::Pure) {
            instruction.effect_in = token;
            token = instruction.effect_out = id();
        }
        block().instructions.push_back(std::move(instruction));
        return label;
    }

    void print(m::Id value) { emit(Print, unit, {value}); }
    void ret(m::Id value) { block().exit = {id(), m::Exit::Return, value, token, {}, at}; }

    m::Module request(m::Id function, bool speculative, std::string_view backend) const {
        auto requested = module;
        requested.version = m::directive_format_version;
        requested.directives.push_back({std::string(backend), target, "rewrite",
                                        m::meta_canonical_action, function, speculative});
        return requested;
    }
};

const m::Instruction* instruction(const m::Module& module, m::Id id) {
    for (const auto& f : module.functions)
        for (const auto& b : f.blocks) {
            auto it = std::find_if(b.instructions.begin(), b.instructions.end(),
                                   [id](const auto& i) { return i.id == id; });
            if (it != b.instructions.end()) return &*it;
        }
    return nullptr;
}

void expect(const m::Module& module, m::Id id, m::Op op, std::int64_t immediate = 0) {
    auto i = instruction(module, id);
    if (!i || i->op != op || i->immediate != immediate)
        throw std::runtime_error("unexpected META instruction #" + std::to_string(id) +
                                 "; expected " + std::string(m::op_name(op)));
}

void differential(const m::Module& baseline, const m::Module& rewritten, const char* name,
                  const std::vector<std::int64_t>& arguments) {
    auto before = m::evaluate(baseline, name, arguments);
    auto after = m::evaluate(rewritten, name, arguments);
    require(before.status == after.status && before.value == after.value && before.type == after.type &&
            before.output == after.output && before.diagnostic == after.diagnostic,
            "META changed the result, output, or trap");
    if (before.status == m::Status::Trapped)
        require(before.where.file == after.where.file && before.where.offset == after.where.offset &&
                before.where.line == after.where.line && before.where.column == after.where.column,
                "META changed the first trap location");
}

void modes_selectors_and_scope() {
    Fixture s;
    const auto f = s.start_function("selected", i64, {i64, boolean});
    const auto x = s.block().parameters[0].id, flag = s.block().parameters[1].id;
    const auto zero = s.emit(Constant, i64);
    const auto one = s.emit(Constant, i64, {}, 1);
    const auto low = s.emit(Constant, i64, {}, minimum), high = s.emit(Constant, i64, {}, maximum);
    const auto negative_zero = s.emit(Neg, i64, {zero});
    s.print(negative_zero);
    const auto checked = s.emit(Add, i64, {x, one});
    const auto lt = s.emit(Lt, boolean, {checked, low});
    const auto gt = s.emit(Gt, boolean, {checked, high});
    const auto ge = s.emit(Ge, boolean, {checked, low});
    s.print(lt);
    s.print(gt);
    s.print(ge);
    const auto not_flag = s.emit(Not, boolean, {flag});
    const auto eq = s.emit(Eq, boolean, {flag, not_flag});
    const auto ne = s.emit(Ne, boolean, {not_flag, flag});
    s.print(eq);
    s.print(ne);
    const auto reflexive = s.emit(Eq, boolean, {flag, flag});
    const auto in_range = s.emit(Lt, boolean, {x, high});
    s.print(reflexive);
    s.print(in_range);
    const auto first = s.emit(Neg, i64, {x});
    const auto second = s.emit(Neg, i64, {first});
    s.print(second);
    s.ret(second);

    s.start_function("unrequested", i64, {i64});
    const auto unrelated = s.emit(Neg, i64, {s.emit(Constant, i64)});
    s.print(unrelated);
    const auto first_unrequested = s.emit(Neg, i64, {s.block().parameters[0].id});
    const auto second_unrequested = s.emit(Neg, i64, {first_unrequested});
    s.ret(second_unrequested);

    s.start_function("main", i64, {});
    const auto arg = s.emit(Constant, i64, {}, 42);
    const auto condition = s.emit(Constant, boolean, {}, 1);
    // emit_c requires an executable entry point; this fixture exercises both
    // the requested function and an ordinary call to it.
    s.ret(s.emit(Call, i64, {arg, condition}, 0, f));

    const auto safe_request = s.request(f, false, "mir-c11");
    const auto wire = m::serialize(safe_request);
    require(m::serialize(m::deserialize(wire)) == wire, "META directive did not round-trip in v4");
    auto safe = m::specialize(safe_request, "mir-c11", target);
    require(m::structurally_equal(safe, m::specialize(safe_request, "mir-c11", target, true)),
            "host opt-in widened a safe-only META request");
    expect(safe, negative_zero, Constant, 0);
    expect(safe, lt, Constant, 0);
    expect(safe, gt, Constant, 0);
    expect(safe, ge, Constant, 1);
    expect(safe, eq, Constant, 0);
    expect(safe, ne, Constant, 1);
    expect(safe, checked, Add);
    expect(safe, reflexive, Eq);
    expect(safe, in_range, Lt);
    expect(safe, first, Neg);
    expect(safe, second, Neg);
    expect(safe, unrelated, Neg);

    for (const auto* backend : {"mir-c11", "mir-vm", "mir-archon"}) {
        const auto request = s.request(f, true, backend);
        require(m::structurally_equal(s.module, m::specialize(request, backend, target)),
                "speculative META enabled without host opt-in");
        for (const auto& [product, triple] : {std::pair{"MIR-C11", target}, {backend, ""},
                                              {backend, "x86_64-linux-gnu"}})
            require(m::structurally_equal(s.module, m::specialize(request, product, triple, true)),
                    "META accepted an inexact backend or target selector");
        auto speculative = m::specialize(request, backend, target, true);
        require(speculative.directives.empty() && speculative.version == m::legacy_format_version,
                "lowering did not consume the META request");
        expect(speculative, first, Neg);
        expect(speculative, negative_zero, Neg);
        expect(speculative, lt, Lt);
        expect(speculative, gt, Gt);
        expect(speculative, ge, Ge);
        expect(speculative, eq, Eq);
        expect(speculative, ne, Ne);
        expect(speculative, second_unrequested, Neg);
        require(!instruction(speculative, second) &&
                speculative.functions[0].blocks[0].exit.value == x &&
                speculative.functions[0].blocks[0].instructions.back().operands == std::vector<m::Id>{x},
                "META did not remove just the trap-free inverse and rewrite its uses");
        if (std::string_view(backend) == "mir-c11")
            require(m::emit_directed_c(request, {}, target, true) == m::emit_c(speculative),
                    "C11 emission did not use its selected META specialization");
        for (auto value : {minimum, minimum + 1, std::int64_t{0}, maximum - 1, maximum})
            for (std::int64_t b : {0, 1})
                differential(s.module, speculative, "selected", {value, b});
    }
    for (auto value : {minimum, minimum + 1, std::int64_t{0}, maximum - 1, maximum})
        for (std::int64_t b : {0, 1})
            differential(s.module, safe, "selected", {value, b});
}

void nonzero_proof() {
    Fixture s;
    const auto f = s.start_function("proof", boolean, {i64});
    const auto x = s.block().parameters[0].id;
    const auto zero = s.emit(Constant, i64);
    const auto nonzero_constant = s.emit(Constant, i64, {}, minimum);
    const auto known_eq = s.emit(Eq, boolean, {nonzero_constant, zero});
    const auto known_ne = s.emit(Ne, boolean, {nonzero_constant, zero});
    s.print(known_eq);
    s.print(known_ne);
    const auto false_value = s.emit(Constant, boolean);
    const auto true_value = s.emit(Constant, boolean, {}, 1);
    const auto bool_comparison = s.emit(Eq, boolean, {true_value, false_value});
    s.print(bool_comparison);

    const auto early_eq = s.emit(Eq, boolean, {x, zero});
    s.print(early_eq);
    const auto predicate = s.emit(Ne, boolean, {zero, x});
    const auto assertion = s.emit(Assert, unit, {predicate});
    const auto proved_eq = s.emit(Eq, boolean, {x, zero});
    const auto proved_ne = s.emit(Ne, boolean, {x, zero});
    s.print(proved_eq);
    s.print(proved_ne);

    const auto entry_token = s.token;
    const auto next = s.start_block();
    s.function().blocks[0].exit = {s.id(), m::Exit::Jump, 0, entry_token, {{next, {}}}, at};
    const auto cross_block_eq = s.emit(Eq, boolean, {x, zero});
    s.ret(cross_block_eq);

    const auto request = s.request(f, true, "mir-vm");
    require(m::structurally_equal(s.module, m::specialize(s.request(f, false, "mir-vm"), "mir-vm", target, true)),
            "safe META requested with host opt-in used speculative nonzero proofs");
    const auto selected = m::specialize(request, "mir-vm", target, true);
    expect(selected, known_eq, Constant, 0);
    expect(selected, known_ne, Constant, 1);
    expect(selected, bool_comparison, Eq);
    expect(selected, early_eq, Eq);
    expect(selected, predicate, Ne);
    expect(selected, assertion, Assert);
    expect(selected, proved_eq, Constant, 0);
    expect(selected, proved_ne, Constant, 1);
    expect(selected, cross_block_eq, Eq);
    for (auto x : {minimum, std::int64_t{-1}, std::int64_t{0}, std::int64_t{1}, maximum})
        differential(s.module, selected, "proof", {x});
}
}

int main() {
    try {
        modes_selectors_and_scope();
        nonzero_proof();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#include <on1x/backend/mir_directives.hpp>
#include <on1x/backend/mir_c11.hpp>

#include <algorithm>
#include <iostream>
#include <limits>

namespace {
namespace m = on1x::mir;
using on1x::Type;
using enum m::Op;
constexpr auto i64 = Type::I64;
constexpr auto boolean = Type::Bool;
constexpr auto unit = Type::Void;
const on1x::Location at{"specialize.mir", 0, 1, 1};

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Fixture {
    m::Module module;
    m::Id next = 1;
    std::size_t f = 0, b = 0;
    m::Id token = 0;

    m::Id id() { return ++next; }
    m::Function& function() { return module.functions[f]; }
    m::Block& block() { return function().blocks[b]; }
    m::Id function(const char* name, Type result, std::vector<Type> parameters = {}) {
        f = module.functions.size();
        auto identity = id();
        module.functions.push_back({identity, name, result, at, {}, {}});
        new_block(std::move(parameters));
        return identity;
    }
    m::Id new_block(std::vector<Type> parameters = {}) {
        b = function().blocks.size();
        auto identity = id();
        token = id();
        function().blocks.push_back({identity, token, at, {}, {}, {}});
        for (const auto& t : parameters) block().parameters.push_back({id(), t, at});
        return identity;
    }
    m::Id emit(m::Op op, Type type, std::vector<m::Id> operands = {},
               std::int64_t immediate = 0, m::Id target = 0) {
        auto identity = id();
        auto loc = at;
        loc.line = identity;
        m::Instruction ins{identity, type, op, std::move(operands), immediate, target, 0, 0, loc};
        if (m::effect(op) != m::Effect::Pure) {
            ins.effect_in = token;
            token = ins.effect_out = id();
        }
        block().instructions.push_back(std::move(ins));
        return identity;
    }
    void terminate(m::Exit kind, m::Id result = 0, std::vector<m::Edge> edges = {}) {
        block().exit = {id(), kind, result, token, std::move(edges), at};
    }
    void ret(m::Id result = 0) { terminate(m::Exit::Return, result); }
    void directive(const char* kind, const char* action, m::Id function = 0,
                   const char* backend = "", const char* target = "", bool speculative = false) {
        module.version = m::directive_format_version;
        module.directives.push_back({backend, target, kind, action, function, speculative});
    }
};

std::size_t count(const m::Module& module, std::size_t index, m::Op op) {
    std::size_t n = 0;
    for (const auto& b : module.functions[index].blocks)
        n += std::count_if(b.instructions.begin(), b.instructions.end(), [op](auto& i) { return i.op == op; });
    return n;
}

void differential(const m::Module& baseline, const m::Module& selected, const char* name,
                  const std::vector<std::int64_t>& args = {}) {
    auto before = m::evaluate(baseline, name, args), after = m::evaluate(selected, name, args);
    require(before.status == after.status && before.value == after.value &&
            before.output == after.output && before.diagnostic == after.diagnostic,
            "specialization changed evaluation or effect trace");
    if (before.status == m::Status::Trapped)
        require(before.where.file == after.where.file && before.where.offset == after.where.offset &&
                before.where.line == after.where.line && before.where.column == after.where.column,
                "specialization changed trap location");
}

void gate_and_prove() {
    Fixture s;
    auto callee = s.function("side_effect", i64, {i64});
    auto arg = s.block().parameters[0].id;
    s.emit(Print, unit, {arg});
    s.ret(arg);
    auto scoped = s.function("scoped", i64, {i64});
    arg = s.block().parameters[0].id;
    auto zero = s.emit(Constant, i64);
    auto one = s.emit(Constant, i64, {}, 1);
    auto value = s.emit(Call, i64, {arg}, 0, callee);
    auto identity = s.emit(Add, i64, {value, zero});
    auto multiplied = s.emit(Div, i64, {s.emit(Mul, i64, {one, identity}), one});
    s.emit(Print, unit, {multiplied});
    auto side = s.emit(Mod, i64, {multiplied, one});
    s.emit(Print, unit, {side});
    s.ret(multiplied);
    s.function("unchecked", i64, {i64});
    arg = s.block().parameters[0].id;
    zero = s.emit(Constant, i64);
    s.ret(s.emit(Add, i64, {arg, zero}));
    const auto baseline = s.module;
    const char* triple = "x86_64-unknown-linux-gnu";
    s.directive("rewrite", "fold-identity", scoped, "mir-archon", triple);
    auto wrong_backend = m::specialize(s.module, "mir-vm", triple);
    auto wrong_target = m::specialize(s.module, "mir-archon", "other");
    require(m::structurally_equal(wrong_backend, baseline) && m::structurally_equal(wrong_target, baseline),
            "inactive scope changed MIR");
    auto chosen = m::specialize(s.module, "mir-archon", triple);
    require(chosen.directives.empty() && s.module.directives.size() == 1 &&
            chosen.version == m::legacy_format_version, "directives not cleanly consumed");
    require(count(chosen, 1, Add) == 0 && count(chosen, 1, Mul) == 0 &&
            count(chosen, 1, Div) == 0 && count(chosen, 1, Mod) == 0 && count(chosen, 2, Add) == 1,
            "selected pattern/scoping failed");
    require(count(chosen, 1, Call) == 1 && count(chosen, 1, Print) == 2,
            "rewriting removed an effectful producer or consumer");
    for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t{0},
                       std::numeric_limits<std::int64_t>::max()})
        differential(baseline, chosen, "scoped", {value});

    s.module.directives.clear();
    s.directive("hint", "kernel-hot", scoped);
    auto hot = m::specialize(s.module, "mir-vm", triple);
    require(count(hot, 1, Add) == 0 && count(hot, 1, Mod) == 1,
            "hot hint or independent policy selector did not gate rules");
    s.module.directives[0].speculative = true;
    require(m::structurally_equal(m::specialize(s.module, "mir-vm", triple), baseline) &&
            count(m::specialize(s.module, "mir-vm", triple, true), 1, Add) == 0,
             "speculative directive was not gated");
}

void multiply_by_zero() {
    Fixture s;
    s.function("annihilate", i64, {i64});
    auto x = s.block().parameters[0].id;
    auto zero = s.emit(Constant, i64);
    auto one = s.emit(Constant, i64, {}, 1);
    s.emit(Print, unit, {x});
    auto produced = s.emit(Add, i64, {x, one}); // Still traps for INT64_MAX.
    s.emit(Print, unit, {s.emit(Mul, i64, {zero, produced})});
    s.ret(s.emit(Mul, i64, {produced, zero}));
    const auto baseline = s.module;
    s.directive("hint", "kernel-hot");
    auto rewritten = m::specialize(s.module, "mir-vm", "");
    require(count(rewritten, 0, Mul) == 0 && count(rewritten, 0, Add) == 1,
            "multiplication by zero did not retain its checked operand producer");
    for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t{-1},
                       std::numeric_limits<std::int64_t>::max()})
        differential(baseline, rewritten, "annihilate", {value});
}

void speculation() {
    Fixture s;
    s.function("unguarded", i64, {i64});
    auto x = s.block().parameters[0].id;
    s.emit(Print, unit, {x});
    s.ret(s.emit(Div, i64, {x, x}));
    s.function("guarded", i64, {i64});
    x = s.block().parameters[0].id;
    auto zero = s.emit(Constant, i64);
    auto proof = s.emit(Ne, boolean, {x, zero});
    s.emit(Print, unit, {x});
    s.emit(Assert, unit, {proof});
    s.emit(Print, unit, {s.emit(Div, i64, {zero, x})});
    s.emit(Print, unit, {s.emit(Mod, i64, {zero, x})});
    s.emit(Print, unit, {s.emit(Mod, i64, {x, x})});
    s.ret(s.emit(Div, i64, {x, x}));
    s.function("proved", i64);
    auto minimum = s.emit(Constant, i64, {}, std::numeric_limits<std::int64_t>::min());
    s.ret(s.emit(Div, i64, {minimum, minimum}));
    s.function("overflow", i64);
    minimum = s.emit(Constant, i64, {}, std::numeric_limits<std::int64_t>::min());
    auto negative_one = s.emit(Constant, i64, {}, -1);
    s.ret(s.emit(Div, i64, {minimum, negative_one}));
    s.function("overflow_rem", i64);
    minimum = s.emit(Constant, i64, {}, std::numeric_limits<std::int64_t>::min());
    negative_one = s.emit(Constant, i64, {}, -1);
    s.ret(s.emit(Mod, i64, {minimum, negative_one}));
    s.function("later_check", i64, {i64});
    x = s.block().parameters[0].id;
    zero = s.emit(Constant, i64);
    proof = s.emit(Ne, boolean, {x, zero});
    auto rem = s.emit(Mod, i64, {x, x}); // Nonzero assertion hasn't executed yet.
    s.emit(Assert, unit, {proof});
    s.ret(rem);
    const auto baseline = s.module;
    s.directive("rewrite", "fold-identity");
    auto cautious = m::specialize(s.module, "mir-eval", "");
    require(m::structurally_equal(baseline, cautious), "speculative pattern was enabled by default");
    auto enhanced = m::specialize(s.module, "mir-eval", "", true);
    require(count(enhanced, 0, Div) == 1 && count(enhanced, 1, Div) == 0 &&
            count(enhanced, 1, Mod) == 0 && count(enhanced, 2, Div) == 0 &&
            count(enhanced, 3, Div) == 1 && count(enhanced, 4, Mod) == 1 && count(enhanced, 5, Mod) == 1,
            "nonzero proof dropped a required trap or failed to discharge a proven trap");
    require(count(enhanced, 1, Assert) == 1, "speculation removed its proof-producing assertion");
    for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t{0}, std::int64_t{37}}) {
        differential(baseline, enhanced, "unguarded", {value});
        differential(baseline, enhanced, "guarded", {value});
        differential(baseline, enhanced, "later_check", {value});
    }
    differential(baseline, enhanced, "proved");
    differential(baseline, enhanced, "overflow");
    differential(baseline, enhanced, "overflow_rem");
}

void predicates() {
    Fixture s;
    s.function("predicates", boolean, {i64, boolean});
    auto x = s.block().parameters[0].id;
    auto flag = s.block().parameters[1].id;
    auto zero = s.emit(Constant, i64);
    auto one = s.emit(Constant, i64, {}, 1);
    auto b_false = s.emit(Constant, boolean);
    auto b_true = s.emit(Constant, boolean, {}, 1);
    for (auto op : {Lt, Le, Gt, Ge, Eq, Ne}) s.emit(Print, unit, {s.emit(op, boolean, {x, x})});
    auto negated = s.emit(Not, boolean, {flag});
    auto same = s.emit(Not, boolean, {negated});
    s.emit(Print, unit, {s.emit(Not, boolean, {b_false})});
    s.emit(Print, unit, {s.emit(Not, boolean, {b_true})});
    s.emit(Print, unit, {s.emit(Eq, boolean, {same, b_true})});
    s.emit(Print, unit, {s.emit(Eq, boolean, {b_true, same})});
    s.emit(Print, unit, {s.emit(Ne, boolean, {same, b_false})});
    s.emit(Print, unit, {s.emit(Ne, boolean, {b_false, same})});
    s.emit(Print, unit, {s.emit(Eq, boolean, {x, one})});
    s.emit(Print, unit, {s.emit(Ne, boolean, {x, zero})});
    s.ret(same);

    s.function("nan_comparison", boolean);
    auto nan = s.emit(Constant, Type::F64, {}, INT64_C(0x7ff8000000000000));
    s.ret(s.emit(FloatEq, boolean, {nan, nan}));
    s.module.version = m::format_version;
    const auto baseline = s.module;
    s.directive("hint", "canonicalized");
    auto rewritten = m::specialize(s.module, "mir-eval", "");
    require(count(rewritten, 0, Lt) == 0 && count(rewritten, 0, Le) == 0 &&
            count(rewritten, 0, Gt) == 0 && count(rewritten, 0, Ge) == 0,
            "ordered self-comparisons were not folded");
    require(count(rewritten, 0, Eq) == 1 && count(rewritten, 0, Ne) == 1 &&
            count(rewritten, 1, FloatEq) == 1,
            "the predicate banks missed a fold or changed a non-boolean comparison");
    require(count(rewritten, 0, Not) == 1 && rewritten.functions[0].blocks[0].exit.value == flag,
            "negation bank did not resolve double negation to its original boolean");
    for (std::int64_t input : {-1, 0, 1, 2})
        for (std::int64_t condition : {0, 1})
            differential(baseline, rewritten, "predicates", {input, condition});
    differential(baseline, rewritten, "nan_comparison");
}

void slots_and_dominance() {
    Fixture s;
    s.function("slots_and_dominance", i64, {i64});
    auto x = s.block().parameters[0].id;
    auto slot = s.id();
    s.function().slots.push_back({slot, i64, at});
    auto zero = s.emit(Constant, i64);
    s.emit(Store, unit, {zero}, 0, slot);
    s.emit(Store, unit, {x}, 0, slot);
    auto load = s.emit(Load, i64, {}, 0, slot);
    auto same = s.emit(Eq, boolean, {load, load});
    s.emit(Print, unit, {same});
    auto identity = s.emit(Add, i64, {load, zero});
    auto middle = s.new_block({i64});
    auto middle_param = s.block().parameters[0].id;
    auto another = s.emit(Add, i64, {identity, zero});
    s.emit(Print, unit, {middle_param});
    auto tail = s.new_block();
    auto cross_block_load = s.emit(Load, i64, {}, 0, slot);
    s.emit(Print, unit, {cross_block_load});
    s.ret(another);
    auto& blocks = s.function().blocks;
    blocks[0].exit = {s.id(), m::Exit::Jump, 0, blocks[0].instructions.back().effect_out, {{middle, {identity}}}, at};
    blocks[1].exit = {s.id(), m::Exit::Jump, 0, blocks[1].instructions.back().effect_out, {{tail, {}}}, at};
    std::swap(blocks[1], blocks[2]); // Storage order intentionally disagrees with dominance.
    const auto baseline = s.module;
    s.directive("hint", "local-memory");
    s.directive("hint", "canonicalized");
    auto hinted = m::specialize(s.module, "mir-vm", "");
    require(count(hinted, 0, Load) == 1 && count(hinted, 0, Store) == 2 &&
            count(hinted, 0, Eq) == 0 && count(hinted, 0, Add) == 2,
            "hint did not selectively forward local storage and fold boolean identity");
    s.directive("rewrite", "fold-identity");
    auto rewritten = m::specialize(s.module, "mir-vm", "");
    require(count(rewritten, 0, Add) == 0 &&
            rewritten.functions[0].blocks[0].exit.edges[0].arguments == std::vector<m::Id>{x} &&
            rewritten.functions[0].blocks[1].exit.value == x,
            "SSA aliases were not threaded through edges and non-topological uses");
    differential(baseline, hinted, "slots_and_dominance", {42});
    differential(baseline, rewritten, "slots_and_dominance", {42});
}

void repeated_local_loads() {
    Fixture s;
    s.function("reuse", i64, {i64, i64});
    auto x = s.block().parameters[0].id;
    auto y = s.block().parameters[1].id;
    auto primary = s.id(), other = s.id();
    s.function().slots = {{primary, i64, at}, {other, i64, at}};
    s.emit(Store, unit, {x}, 0, primary);
    auto entry_effect = s.token;
    auto next = s.new_block();
    s.function().blocks[0].exit = {s.id(), m::Exit::Jump, 0, entry_effect, {{next, {}}}, at};
    auto first = s.emit(Load, i64, {}, 0, primary);
    s.emit(Print, unit, {first});
    s.emit(Store, unit, {y}, 0, other); // Different slot leaves the saved load valid.
    auto repeated = s.emit(Load, i64, {}, 0, primary);
    s.emit(Print, unit, {repeated});
    s.emit(Store, unit, {y}, 0, primary); // Same slot invalidates the saved load.
    auto updated = s.emit(Load, i64, {}, 0, primary);
    s.ret(updated);

    auto aggregate = Type::tuple({i64, boolean});
    s.function("aggregate", aggregate, {aggregate});
    auto pair = s.block().parameters[0].id;
    auto slot = s.id();
    s.function().slots.push_back({slot, aggregate, at});
    s.emit(Store, unit, {pair}, 0, slot);
    s.emit(Load, aggregate, {}, 0, slot);
    s.ret(s.emit(Load, aggregate, {}, 0, slot));
    s.module.version = m::format_version;
    const auto baseline = s.module;
    s.directive("hint", "local-memory");
    auto optimized = m::specialize(s.module, "mir-vm", "");
    const auto& block = optimized.functions[0].blocks[1];
    std::vector<m::Id> printed;
    for (const auto& i : block.instructions)
        if (i.op == Print) printed.push_back(i.operands[0]);
    require(count(optimized, 0, Load) == 1 && count(optimized, 0, Store) == 3 &&
            printed == std::vector<m::Id>{first, first} && block.exit.value == y,
            "same-block load reuse crossed a same-slot store or failed to reuse the first load");
    require(count(optimized, 1, Load) == 2, "scalar memory banks rewrote an aggregate load");
    differential(baseline, optimized, "reuse", {11, 22});
}

void scoped_extensions() {
    Fixture s;
    const auto word = Type::U32;
    const auto memory = [&](const char* name) {
        auto function = s.function(name, word, {word});
        auto slot = s.id();
        s.function().slots.push_back({slot, word, at});
        s.emit(Store, unit, {s.block().parameters[0].id}, 0, slot);
        auto load = s.emit(Load, word, {}, 0, slot);
        s.emit(Print, unit, {load});
        s.ret(load);
        return function;
    };
    auto mem = memory("memory");
    memory("untouched_memory");
    auto meta = s.function("meta", i64, {i64});
    auto x = s.block().parameters[0].id;
    s.emit(Print, unit, {x});
    auto first = s.emit(Neg, i64, {x});
    s.ret(s.emit(Neg, i64, {first}));
    auto main = s.function("main", i64);
    // Host opt-in cannot replace a possibly trapping negation on its own.
    s.ret(s.emit(Neg, i64, {s.emit(Constant, i64, {}, std::numeric_limits<std::int64_t>::min())}));
    s.module.version = m::numeric_format_version;
    const auto baseline = s.module;
    const char* backend = "mir-c11";
    const char* target = "x86_64-unknown-linux-gnu";
    s.directive("rewrite", m::memgraph_local_action, mem, backend, target);
    s.directive("rewrite", m::meta_canonical_action, meta, backend, target, true);
    s.directive("rewrite", m::meta_canonical_action, main, backend, target, true);
    const auto wire = m::serialize(s.module);
    const auto portable = m::deserialize(wire);
    require(m::structurally_equal(portable, s.module) && m::serialize(portable) == wire,
            "new scoped actions did not round-trip in v4");

    for (const auto& [product, triple] : {std::pair{"mir-vm", target}, {"MIR-C11", target},
                                          {backend, ""}, {backend, "x86_64-linux-gnu"}})
        require(m::structurally_equal(m::specialize(portable, product, triple, true), baseline),
                "new scoped actions accepted an inexact selector");
    auto safe = m::specialize(portable, backend, target);
    require(count(safe, 0, Load) == 0 && count(safe, 0, Store) == 1 &&
            count(safe, 1, Load) == 1 && count(safe, 2, Neg) == 2,
            "scoped safe rule was inactive, escaped scope, or opened speculation");
    auto selected = m::specialize(portable, backend, target, true);
    require(count(selected, 0, Load) == 0 && count(selected, 2, Neg) == 1 &&
            count(selected, 2, Print) == 1 && count(selected, 3, Neg) == 1,
            "speculative rule missed a checked proof or dropped a required trap");
    auto safe_request = portable;
    safe_request.directives[1].speculative = false;
    require(count(m::specialize(safe_request, backend, target, true), 2, Neg) == 2,
            "host opt-in plus a safe request unlocked a speculative rule");
    auto speculative_request = portable;
    speculative_request.directives[0].speculative = true;
    require(count(m::specialize(speculative_request, backend, target, true), 0, Load) == 1,
            "speculative request activated the safe profile");
    require(m::emit_directed_c(portable, {}, target, false) == m::emit_c(safe) &&
            m::emit_directed_c(portable, {}, target, true) == m::emit_c(selected) &&
            m::emit_directed_c(portable, {}, "", true) == m::emit_c(baseline),
            "C11 emission failed to consume its selected specialization");
    differential(baseline, selected, "memory", {42});
    differential(baseline, selected, "main");
    for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t{0},
                       std::numeric_limits<std::int64_t>::max()})
        differential(baseline, selected, "meta", {value});

    auto wildcard = portable;
    wildcard.directives[0].backend.clear();
    wildcard.directives[0].target.clear();
    require(count(m::specialize(wildcard, backend, target), 0, Load) == 0,
            "wildcard directive did not allow its exact pattern selector");
}

void profile_and_validation() {
    Fixture s;
    s.function("float_only", Type::F64, {Type::F64});
    s.ret(s.block().parameters[0].id);
    s.directive("hint", "local-memory", 0, "unselected");
    require(m::specialize(s.module, "mir-vm", "").version == m::format_version,
            "specialized f64 did not retain MIR version 2");

    auto nested = Type::tuple({Type::F64, Type::tuple({Type::F32, Type::U8})});
    s.function("nested_numeric", nested, {nested});
    s.ret(s.block().parameters[0].id);
    require(m::specialize(s.module, "mir-vm", "").version == m::numeric_format_version,
            "numeric tuple fields did not retain MIR version 3");
    s.module.version = m::numeric_format_version;
    s.module.directives.clear();
    require(m::structurally_equal(m::specialize(s.module, "mir-vm", ""), s.module),
            "specialization changed an unannotated legacy-format module");

    s.directive("hint", "local-memory", 9999, "unselected");
    try {
        (void)m::specialize(s.module, "mir-vm", "");
        throw std::runtime_error("specialization accepted malformed inactive directive");
    } catch (const on1x::Error& error) {
        require(std::string_view(error.what()).find("unknown directive function") != std::string_view::npos,
                "malformed directive rejected for wrong reason");
    }
}
}

int main() {
    try {
        gate_and_prove();
        multiply_by_zero();
        speculation();
        predicates();
        slots_and_dominance();
        repeated_local_loads();
        scoped_extensions();
        profile_and_validation();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

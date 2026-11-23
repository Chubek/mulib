#include "novostone.hpp"
#include "../runtime/on1x_bounds.h"
#include <novostone/CostCC/CostCC.hpp>
#include <novostone/PCC/SMTProof.hpp>
#include <novostone/SpeculativeIR/SpeculativeIR.hpp>
#include <array>
#include <charconv>
#include <limits>
#include <memory>

namespace on1x::hir {
namespace {
namespace ccdb = novostone::ccdb;
namespace cost = novostone::cost;
namespace ir = novostone::ir;
namespace pcc = novostone::pcc;
namespace query = novostone::query;
namespace speculative = novostone::speculative;
using E = Expr::Kind;

// Novostone arithmetic wraps at 32 bits. Admit only closed, effect-free
// expressions whose *every* intermediate is independently proved to be an
// exact signed i32 (or canonical bool) by the On1x checked arithmetic.
struct Word {
    ir::Op op;
    std::int32_t value;
    std::size_t lhs = 0, rhs = 0;
};

class Projection {
    static constexpr std::size_t max_words = 48, max_depth = 24;
    std::vector<Word> words;
    std::uint32_t literals = 0;

    struct Bindings {
        ir::Id assumption;
        std::vector<speculative::Specialization> specializations;
        std::vector<std::uint32_t> arguments;
    };

    std::optional<std::size_t> add(ir::Op op, std::int64_t value,
                                   std::size_t lhs = 0, std::size_t rhs = 0) {
        if (words.size() == max_words || value < std::numeric_limits<std::int32_t>::min() ||
            value > std::numeric_limits<std::int32_t>::max()) return {};
        words.push_back({op, static_cast<std::int32_t>(value), lhs, rhs});
        if (op == ir::Op::Const) ++literals;
        return words.size() - 1;
    }
    std::int64_t value(std::size_t i) const { return words[i].value; }

public:
    std::optional<std::size_t> expression(const Expr& e, std::size_t depth = 0) {
        if (depth == max_depth || words.size() == max_words ||
            (e.type != Type::I64 && e.type != Type::Bool) ||
            e.prototype || e.noreturn || e.move_out || e.unsafe_region ||
            e.borrow != BorrowMode::Value || e.callee || !e.proofs.empty() ||
            !e.body.empty()) return {};
        if (e.children.empty()) {
            if (e.kind == E::Boolean && e.type == Type::Bool && (e.text == "true" || e.text == "false"))
                return add(ir::Op::Const, e.text == "true");
            if (e.kind != E::Integer || e.type != Type::I64) return {};
            std::int64_t n = 0;
            const auto end = e.text.data() + e.text.size();
            const auto parsed = std::from_chars(e.text.data(), end, n);
            if (parsed.ec != std::errc{} || parsed.ptr != end || std::to_string(n) != e.text) return {};
            return add(ir::Op::Const, n);
        }
        if (e.kind == E::Unary && e.children.size() == 1 && e.children[0]->type == e.type) {
            const auto operand = expression(*e.children[0], depth + 1);
            if (!operand) return {};
            if (e.type == Type::Bool && e.text == "!") return negate_bool(*operand);
            if (e.type != Type::I64 || e.text != "-") return {};
            std::int64_t result = 0;
            if (!on1x_try_sub_i64(0, value(*operand), &result)) return {};
            const auto zero = add(ir::Op::Const, 0);
            return zero ? add(ir::Op::Sub, result, *zero, *operand) : std::nullopt;
        }
        if (e.kind != E::Binary || e.children.size() != 2 ||
            e.children[0]->type != e.children[1]->type) return {};
        const auto lhs = expression(*e.children[0], depth + 1);
        if (!lhs) return {};
        const auto rhs = expression(*e.children[1], depth + 1);
        if (!rhs) return {};
        const auto a = value(*lhs), b = value(*rhs);
        if (e.type == Type::Bool && (e.text == "==" || e.text == "!=")) {
            const auto eq = add(ir::Op::Eq, a == b, *lhs, *rhs);
            if (eq && e.text == "!=") return negate_bool(*eq);
            return eq;
        }
        if (e.type != Type::I64 || e.children[0]->type != Type::I64) return {};
        std::int64_t result = 0;
        if (e.text == "+" && on1x_try_add_i64(a, b, &result)) return add(ir::Op::Add, result, *lhs, *rhs);
        if (e.text == "-" && on1x_try_sub_i64(a, b, &result)) return add(ir::Op::Sub, result, *lhs, *rhs);
        if (e.text == "*" && on1x_try_mul_i64(a, b, &result)) return add(ir::Op::Mul, result, *lhs, *rhs);
        return {};
    }

    std::optional<std::size_t> negate_bool(std::size_t operand) {
        const auto zero = add(ir::Op::Const, 0);
        return zero ? add(ir::Op::Eq, !value(operand), operand, *zero) : std::nullopt;
    }

    std::optional<std::int32_t> optimize(processor::NovostoneStats& stats) const {
        auto make_database = [] { return ccdb::Database(ir::program(), [](const auto& s) { ir::verify(s); }); };
        auto generic_db = make_database();
        // Check literal specialization as a guarded parameter refinement first.
        // No guard or alternative body is emitted into On1x runtime code.
        ir::Builder generic(generic_db, literals);
        const auto entry = generic.block();
        auto assumptions = std::make_shared<speculative::Assumptions>();
        Bindings bindings{assumptions->assume("closed checked HIR literals"), {}, {}};
        const auto generic_source = build(generic, entry, &bindings);
        const auto version = speculative::Version::specialize(generic_source, assumptions,
                                                               bindings.specializations);
        const auto fast = version.execute(bindings.arguments, {}, max_words + 1);
        const auto expected = static_cast<std::uint32_t>(words.back().value);
        if (fast.failed_guard || !fast.outcome.trap.empty() || fast.outcome.value != expected) return {};
        ++stats.specialized;

        // QueryBasedIR doesn't accept speculative snapshots. Build a separate,
        // ordinary closed body from the same independently checked projection.
        auto db = make_database();
        ir::Builder builder(db);
        const auto source = build(builder, builder.block());
        const auto candidate = query::optimize(source, {.rewrites=96, .query_work=1'000'000,
                                                        .query_facts=8192, .analysis_work=8192});
        if (candidate.budget_exhausted()) stats.limited = true;
        if (candidate.rewrites().empty()) return {};
        cost::Ranker ranker;
        if (ranker.choose({query::baseline(source), candidate}, {"on1x-c11", "closed-i32", "",
                                                                 "checked-hir-fold", 0}).selected != 1)
            return {};

        const auto before = ir::load(source), after = ir::load(candidate.result());
        const auto& block = after.blocks.at(after.entry).instructions;
        if (after.blocks.size() != 1 || block.size() != 2 || block[0].op != ir::Op::Const ||
            block[1].op != ir::Op::Return || block[1].operands != std::vector{block[0].id} ||
            block[0].immediate != expected) return {};
        auto proof = pcc::emit_smt_proof(before, after, candidate);
        if (proof.obligations.empty()) return {};
        pcc::discharge_smt(proof);
        for (const auto& obligation : proof.obligations) {
            ++stats.obligations;
            if (obligation.status == pcc::ObligationStatus::UnsatProved) ++stats.smt_proved;
            if (obligation.status == pcc::ObligationStatus::SatCounterexample ||
                obligation.status == pcc::ObligationStatus::Unchecked) return {};
        }

        // A closed program has exactly one input. Check it and compare the
        // returned literal against independent checked On1x evaluation even
        // when SMT discharge is unavailable (Unknown is not a proof).
        if (query::validate(before, after, {query::Input{}}, max_words + 1).verdict != query::Verdict::Passed)
            return {};
        (void)query::apply(db, candidate);
        return words.back().value;
    }

private:
    ccdb::Snapshot build(ir::Builder& builder, ir::Id entry, Bindings* bindings = nullptr) const {
        std::array<ir::Id, max_words> ids{};
        for (std::size_t i = 0; i < words.size(); ++i) {
            const auto& word = words[i];
            if (word.op != ir::Op::Const) {
                ids[i] = builder.emit(entry, word.op, {ids[word.lhs], ids[word.rhs]});
                continue;
            }
            const auto bits = static_cast<std::uint32_t>(word.value);
            if (bindings) {
                ids[i] = builder.emit(entry, ir::Op::Param, {},
                                      static_cast<std::uint32_t>(bindings->arguments.size()));
                bindings->arguments.push_back(bits);
                bindings->specializations.push_back({ids[i], bits, bindings->assumption});
            } else {
                ids[i] = builder.emit(entry, ir::Op::Const, {}, bits);
            }
        }
        builder.emit(entry, ir::Op::Return, {ids[words.size() - 1]});
        return builder.finish();
    }
};

struct Fold {
    processor::NovostoneStats stats{.enabled=true};
    static constexpr std::size_t max_visits = 32768, max_attempts = 64;
    std::size_t visits = 0;
    bool tick(std::size_t depth) {
        if (visits == max_visits || stats.attempted == max_attempts || depth > max_syntax_depth) {
            stats.limited = true;
            return false;
        }
        ++visits;
        return true;
    }
    void expression(ExprPtr& expr, std::size_t depth) {
        if (!expr || !tick(depth) || expr->prototype) return;
        if (expr->kind == E::Unary || expr->kind == E::Binary) {
            Projection projection;
            if (projection.expression(*expr)) {
                ++stats.attempted;
                try {
                    if (const auto n = projection.optimize(stats)) {
                        auto literal = std::make_unique<Expr>();
                        literal->kind = expr->type == Type::Bool ? E::Boolean : E::Integer;
                        literal->type = expr->type;
                        literal->where = expr->where;
                        literal->context = expr->context;
                        literal->text = literal->kind == E::Integer ? std::to_string(*n) : *n ? "true" : "false";
                        expr = std::move(literal);
                        ++stats.applied;
                        return;
                    }
                } catch (const novostone::datalog::BudgetExceeded&) {
                    stats.limited = true;
                } catch (const std::length_error&) {
                    // PCC has its own limits on emitted obligations.
                    stats.limited = true;
                }
            }
        }
        expression(expr->callee, depth + 1);
        for (auto& child : expr->children) expression(child, depth + 1);
        block(expr->body, depth + 1);
    }
    void block(std::vector<Stmt>& body, std::size_t depth) {
        for (auto& stmt : body) {
            if (!tick(depth)) return;
            if (stmt.prototype || stmt.kind == Stmt::Kind::Template) continue;
            expression(stmt.value, depth + 1);
            block(stmt.body, depth + 1);
            block(stmt.otherwise, depth + 1);
        }
    }
};
}

processor::NovostoneStats fold_with_novostone(Unit& unit) {
    Fold fold;
    for (auto& fn : unit.functions) {
        if (!fold.tick(0)) break;
        if (!fn.prototype && !fn.external) fold.block(fn.body, 1);
    }
    return fold.stats;
}
}

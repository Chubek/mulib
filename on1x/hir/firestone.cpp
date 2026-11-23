#include "firestone.hpp"
#include "../src/backend/checked.hpp"
#include <librewrite/Term.hpp>
#include <libsema/OverflowCheck.hpp>
#include <charconv>

namespace on1x::hir {
namespace {
using E = Expr::Kind;
using FireOp = firestone::ir::Opcode;
using OnOp = mir::Op;
struct Op {
    FireOp fire;
    OnOp on;
};

std::optional<Op> opcode(const Expr& e) {
    if (e.kind == E::Unary && e.text == "-") return Op{FireOp::Neg, OnOp::Neg};
    if (e.kind != E::Binary) return {};
    if (e.text == "+") return Op{FireOp::Add, OnOp::Add};
    if (e.text == "-") return Op{FireOp::Sub, OnOp::Sub};
    if (e.text == "*") return Op{FireOp::Mul, OnOp::Mul};
    if (e.text == "/") return Op{FireOp::Div, OnOp::Div};
    if (e.text == "%") return Op{FireOp::Mod, OnOp::Mod};
    return {};
}

bool eligible(const Expr& e) {
    return e.type == Type::I64 && !e.prototype && !e.noreturn && !e.move_out &&
        !e.unsafe_region && e.borrow == BorrowMode::Value && !e.callee &&
        e.proofs.empty() && e.body.empty();
}

std::optional<std::int64_t> literal(const Expr& e) {
    if (!eligible(e) || e.kind != E::Integer || !e.children.empty() || e.text.size() > 20)
        return {};
    std::int64_t n = 0;
    const auto end = e.text.data() + e.text.size();
    const auto parsed = std::from_chars(e.text.data(), end, n);
    // In particular, don't reinterpret a leading-zero C spelling as decimal.
    if (parsed.ec != std::errc{} || parsed.ptr != end || std::to_string(n) != e.text) return {};
    return n;
}

bool right_identity(std::int64_t rhs) {
    // Do not project the left operand: it may call, move, or trap. The only
    // admitted rule returns this exact subtree once, in its original position.
    namespace rewrite = firestone::rewrite;
    static const auto pattern = rewrite::parse("(checked-i64-div ?operand 1)");
    return rewrite::match(pattern, {"checked-i64-div", {rewrite::Term("opaque"), rewrite::Term(std::to_string(rhs))}})
        .has_value();
}

struct Fold {
    processor::FirestoneStats stats{.enabled=true};
    static constexpr std::size_t max_visits = 32768;

    bool tick(std::size_t depth) {
        if (stats.visited == max_visits || depth > max_syntax_depth) {
            stats.limited = true;
            return false;
        }
        ++stats.visited;
        return true;
    }

    void expression(ExprPtr& expr, std::size_t depth) {
        if (!expr || !tick(depth) || expr->prototype) return;
        auto& e = *expr;
        expression(e.callee, depth + 1);
        for (auto& child : e.children) expression(child, depth + 1);
        block(e.body, depth + 1);
        if (stats.limited || !eligible(e)) return;
        const auto op = opcode(e);
        if (!op || e.children.size() != (e.kind == E::Unary ? 1u : 2u)) return;
        const auto b = e.kind == E::Unary ? std::optional<std::int64_t>{0} : literal(*e.children[1]);
        if (!b) return;
        const auto a = literal(*e.children[0]);
        if (!a) {
            if (op->on == OnOp::Div && *b == 1 && e.children[0]->type == Type::I64 && right_identity(*b)) {
                auto original = std::move(e.children[0]);
                expr = std::move(original);
                ++stats.divide_one;
            }
            return;
        }
        const auto result = firestone::sema::checked_integer(op->fire, *a, *b);
        // On1x traps on MIN % -1; Firestone returns zero. On1x independently
        // supplies the semantic gate for that and any future disagreements.
        const auto reference = mir::detail::arithmetic(op->on, *a, *b);
        if (!result || reference.trap || *result.value != reference.value) return;
        auto replacement = std::make_unique<Expr>();
        replacement->kind = E::Integer;
        replacement->type = Type::I64;
        replacement->where = e.where;
        replacement->context = e.context;
        replacement->text = std::to_string(*result.value);
        expr = std::move(replacement);
        ++stats.folds;
    }

    void block(std::vector<Stmt>& body, std::size_t depth) {
        for (auto& stmt : body) {
            if (stats.limited || !tick(depth)) return;
            if (stmt.prototype || stmt.kind == Stmt::Kind::Template) continue;
            expression(stmt.value, depth + 1);
            block(stmt.body, depth + 1);
            block(stmt.otherwise, depth + 1);
        }
    }
};
}

processor::FirestoneStats fold_with_firestone(Unit& unit) {
    Fold fold;
    for (auto& fn : unit.functions) {
        if (fold.stats.limited || !fold.tick(0)) break;
        if (!fn.prototype && !fn.external) fold.block(fn.body, 1);
    }
    return fold.stats;
}
}

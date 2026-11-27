#include "AsyncPlan.hpp"

#include <on1x/clone.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/numeric.hpp>
#include <on1x/sema.hpp>
#include <map>
#include <set>

namespace on1x::backends::archon::detail {
namespace {
using E = Expr::Kind;
using S = Stmt::Kind;
using K = Action::Kind;
using Slots = std::map<std::string, std::size_t>;

[[noreturn]] void unsupported(const Location &at, std::string message) {
    throw Error(at, "ON1X-ARCHON-TIMER-UNSUPPORTED: " + std::move(message),
                ARCHON_BACKEND_UNSUPPORTED);
}

bool csp_op(const Expr &e, std::string_view op) {
    return e.kind == E::Csp && e.text == op;
}
const Expr *print_operand(const Expr &e) {
    if (e.kind == E::Call && e.callee && e.callee->reference == Expr::Reference::Intrinsic &&
        e.callee->text == "println" && e.children.size() == 1)
        return e.children.front().get();
    return nullptr;
}

class Projection {
    const Options &options_;
    Unit scalar_;
    TimerProgram result_;
    std::size_t budget_ = 262144;

    void poll(const Location &at) const {
        if (options_.cancelled && options_.cancelled(options_.cancel_userdata))
            throw Error(at, "ON1X-MIR-ARCHON: compilation cancelled", ARCHON_BACKEND_CANCELLED);
    }
    void scalar_inputs(const Expr &e, const Slots &locals, Scalar &scalar,
                       Function &function, std::set<std::size_t> &seen, unsigned depth = 0) {
        poll(e.where);
        if (depth > 128 || e.type != Type::I64)
            unsupported(e.where, "expected bounded pure i64 arithmetic");
        if (e.kind == E::Integer) return;
        if (e.kind == E::Variable && e.reference == Expr::Reference::Local) {
            const auto it = locals.find(e.text);
            if (it == locals.end()) unsupported(e.where, "expected an initialized local i64");
            if (seen.insert(it->second).second) {
                scalar.inputs.push_back(it->second);
                function.params.push_back({e.text, Type::I64, e.where, e.context, {}, BorrowMode::Value});
            }
            return;
        }
        if ((e.kind == E::Unary && e.text == "-") ||
            (e.kind == E::Binary && (e.text == "+" || e.text == "-" || e.text == "*" ||
                                     e.text == "/" || e.text == "%"))) {
            for (const auto &child : e.children)
                scalar_inputs(*child, locals, scalar, function, seen, depth + 1);
            return;
        }
        unsupported(e.where, "only i64 literals, immutable locals and checked arithmetic are admitted");
    }
    Scalar scalar(const Expr &e, const Slots &locals) {
        Scalar out{scalar_.functions.size(), {}};
        Function f;
        f.name = "archon_timer_scalar_" + std::to_string(out.function);
        f.result = Type::I64;
        f.where = e.where;
        std::set<std::size_t> seen;
        scalar_inputs(e, locals, out, f, seen);
        Stmt ret{};
        ret.kind = S::Return;
        ret.where = e.where;
        ret.value = clone_expression(e, budget_);
        f.body.push_back(std::move(ret));
        scalar_.functions.push_back(std::move(f));
        return out;
    }
    static const Expr &flat_value(const Stmt &s) {
        if (s.prototype || !s.value || s.pattern || s.mutable_binding || s.ref_store ||
            s.unsafe_block || !s.body.empty() || !s.otherwise.empty())
            unsupported(s.where, "expected a flat immutable scalar or task statement");
        return *s.value;
    }
    void scalar_let(Actor &actor, Slots &locals, const Stmt &s) {
        const auto operand = scalar(*s.value, locals);
        const auto slot = actor.slots++;
        actor.actions.push_back({K::Let, s.where, operand, slot});
        locals.insert_or_assign(s.name, slot);
    }
    Actor child(const Expr &lambda, bool frame) {
        if (lambda.kind != E::Lambda || !lambda.captures.empty() || !lambda.parameters.empty() ||
            lambda.result != Type::I64 || lambda.body.size() > 128)
            unsupported(lambda.where, "expected a bounded, noncapturing i64 callback");
        Actor actor{lambda.where, {}, 0};
        Slots locals;
        bool returned = false;
        for (const auto &s : lambda.body) {
            const auto &e = flat_value(s);
            if (returned) unsupported(s.where, "expected the final i64 return");
            if (s.kind == S::Let) {
                scalar_let(actor, locals, s);
            } else if (s.kind == S::Return) {
                actor.actions.push_back({K::Return, s.where, scalar(e, locals)});
                returned = true;
            } else if (frame && s.kind == S::Expr && csp_op(e, "frame_sleep")) {
                // csp::verify proves the exact increasing state sequence and
                // initialization of all locals read after each suspension.
                const auto state = integer_literal_bits(e.children[1]->text, e.children[1]->where);
                actor.actions.push_back({K::Sleep, e.where, scalar(*e.children[0], locals),
                                         static_cast<std::size_t>(state)});
            } else if (!frame && s.kind == S::Expr && print_operand(e)) {
                actor.actions.push_back({K::Print, e.where, scalar(*print_operand(e), locals)});
            } else unsupported(s.where, "unsupported native frame or peer statement");
        }
        if (!returned) unsupported(lambda.where, "expected a final i64 result");
        return actor;
    }
    static Action consume(const Expr &value, Slots &handles, bool print, bool bind = false) {
        const auto handle = handles.find(value.children.front()->text);
        if (handle == handles.end()) unsupported(value.where, "expected a live local task");
        Action out{value.text == "join" ? K::Join : K::Cancel, value.where, {},
                   handle->second, print, bind};
        handles.erase(handle);
        return out;
    }

    void owner(const Stmt &group) {
        auto &actor = result_.owner;
        actor.where = group.where;
        if (group.value) result_.within = scalar(*group.value, {});
        if (group.body.size() > 128) unsupported(group.where, "taskgroup statement limit (128) exceeded");
        Slots locals, handles;
        std::size_t next_handle = 0;
        for (const auto &s : group.body) {
            const auto &e = flat_value(s);
            if (s.kind == S::Let && (csp_op(e, "launch_async") || csp_op(e, "launch"))) {
                const bool frame = e.text == "launch_async";
                auto &actors = frame ? result_.frames : result_.peers;
                actor.actions.push_back({frame ? K::LaunchFrame : K::LaunchPeer, e.where,
                                         {}, actors.size()});
                actors.push_back(child(*e.children.front(), frame));
                handles.insert_or_assign(s.name, next_handle++);
                locals.erase(s.name);
            } else if (s.kind == S::Let && csp_op(e, "join")) {
                actor.actions.push_back(consume(e, handles, false, true));
                locals.insert_or_assign(s.name, actor.slots++);
                handles.erase(s.name);
            } else if (s.kind == S::Let) {
                scalar_let(actor, locals, s);
                handles.erase(s.name);
            } else if (s.kind == S::Expr) {
                const auto *printed = print_operand(e);
                const auto &value = printed ? *printed : e;
                if (csp_op(value, "join") || csp_op(value, "cancel")) {
                    actor.actions.push_back(consume(value, handles, printed != nullptr));
                } else if (printed) {
                    actor.actions.push_back({K::Print, e.where, scalar(value, locals)});
                } else unsupported(s.where, "expected println, consuming join, or consuming cancel");
            } else unsupported(s.where, "expected a flat taskgroup statement");
        }
    }
public:
    explicit Projection(const Options &options) : options_(options) {}

    TimerProgram run(const Unit &source) {
        poll({"<archon-timer>"});
        // This projection constructs a new scalar Unit, so fail before it
        // could drop the source's sealed module ABI evidence.
        if (source.direct_c11_type_views || !source.opaque_nominal_owners.empty())
            unsupported({"<archon-timer>"}, "sealed module value ABI requires direct C11 lowering");
        // Reject forged CSP structure before copying or recomputing annotations;
        // then use the same full checked-candidate validation as HIR rewrites.
        csp::verify(source, true);
        auto checked = clone_unit(source, budget_);
        recheck_checked_hir(checked);
        if (checked.functions.size() != 1)
            unsupported({"<archon-timer>"}, "expected a single main function");
        const auto &main = checked.functions.front();
        if (main.name != "main" || main.external || main.prototype ||
            !main.params.empty() || main.result != Type::Void || main.body.size() != 1 ||
            main.body.front().kind != S::CspScope)
            unsupported(main.where, "expected fn main() containing exactly one taskgroup");
        result_.entry_at = main.where;
        owner(main.body.front());
        if (result_.frames.empty()) unsupported(main.where, "expected at least one timer frame");
        result_.scalar = mir::lower(scalar_, options_.verification_limits);
        return std::move(result_);
    }
};
} // namespace

TimerProgram project_timer_program(const Unit &source, const Options &options) {
    return Projection(options).run(source);
}
} // namespace on1x::backends::archon::detail

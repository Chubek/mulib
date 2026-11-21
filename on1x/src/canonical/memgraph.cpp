#include "memgraph.hpp"
#include <sstream>

#ifdef ON1X_WITH_MEMGRAPH
#include "../../runtime/on1x_bounds.h"
#include <on1x_memgraph_rules.hpp>
#include <charconv>
#include <limits>
#endif

namespace on1x::memgraph {
bool available() noexcept {
#ifdef ON1X_WITH_MEMGRAPH
    return true;
#else
    return false;
#endif
}

Bounds::Bounds(const C11Options& options)
    : enabled(options.memgraph_bounds), reporting(options.memgraph_report != nullptr) {
    if (enabled && !available())
        throw std::runtime_error("MEMGraph unavailable: configure with ON1X_WITH_MEMGRAPH=ON and LibStone LLSynth");
#ifdef ON1X_WITH_MEMGRAPH
    nodes = compiled::budget.nodes;
    steps = compiled::budget.steps;
#endif
}

bool Bounds::charge(std::size_t work) {
    if (work > steps) {
        exhausted = true;
        return false;
    }
    steps -= work;
    return true;
}

bool Bounds::record(const Location& where, bool omit, std::string_view reason,
                    std::string_view rule, std::string_view rule_file, std::size_t rule_line,
                    std::string_view diagnostic) {
    if (reporting) {
        ++(omit ? applied : retained);
        if (events.size() < 128)
            events.push_back({where, reason, rule, rule_file, diagnostic, rule_line, omit});
    }
    return omit;
}

namespace {
std::size_t utf8_width(std::string_view suffix) {
    const auto lead = static_cast<unsigned char>(suffix.front());
    const std::size_t width = lead >= 0xc2 && lead <= 0xdf ? 2 :
        lead >= 0xe0 && lead <= 0xef ? 3 : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
    if (!width || suffix.size() < width) return 0;
    for (std::size_t i = 1; i < width; ++i)
        if ((static_cast<unsigned char>(suffix[i]) & 0xc0) != 0x80) return 0;
    const auto second = static_cast<unsigned char>(suffix[1]);
    if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0) ||
        (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90)) return 0;
    return width;
}

void quote(std::ostream& out, std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        const auto width = c >= 128 ? utf8_width(text.substr(i)) : 0;
        if (width) {
            out << text.substr(i, width);
            i += width;
            continue;
        }
        if (c == '\\' || c == '"') out << '\\' << static_cast<char>(c);
        // Escape malformed filename bytes so the report is still valid JSON.
        else if (c < 32 || c >= 127) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << static_cast<char>(c);
        ++i;
    }
    out << '"';
}

#ifdef ON1X_WITH_MEMGRAPH
// V1 admits only flat operators over immutable HIR operands. Capture slots
// correspond to the original Expr child identities in left-to-right order.
struct Shape {
    std::string_view checked, proved;
    std::size_t arity;
};
constexpr Shape index{"CheckedArrayIndex", "ProvedArrayIndex", 2};
constexpr Shape slice{"CheckedArraySlice", "ProvedArraySlice", 3};

bool term_range(std::size_t begin, std::size_t size) {
    return begin <= compiled::terms.size() && size <= compiled::terms.size() - begin;
}

bool same_captures(const compiled::Rule& rule, const Shape& shape) {
    const auto begin = rule.match_begin;
    if (!term_range(begin, rule.match_size) || rule.match_size != shape.arity + 1 ||
        compiled::terms[begin].name != shape.checked || compiled::terms[begin].arity != shape.arity)
        return false;
    for (std::size_t i = 1; i <= shape.arity; ++i) {
        const auto& term = compiled::terms[begin + i];
        if (term.arity || term.name.size() < 2 || term.name.front() != '$') return false;
        for (std::size_t j = 1; j < i; ++j)
            if (term.name == compiled::terms[begin + j].name) return false;
    }
    return true;
}

bool same_replacement(const compiled::Rule& rule, const Shape& shape) {
    const auto begin = rule.replace_begin;
    if (!term_range(begin, rule.replace_size) || rule.replace_size != shape.arity + 1 ||
        compiled::terms[begin].name != shape.proved || compiled::terms[begin].arity != shape.arity)
        return false;
    for (std::size_t i = 1; i <= shape.arity; ++i) {
        const auto& term = compiled::terms[begin + i];
        if (term.arity || term.name != compiled::terms[rule.match_begin + i].name) return false;
    }
    return true;
}

std::optional<std::int64_t> decimal(const Expr& expr) {
    std::string_view text = expr.text;
    // Match the canonical decimal token emitted in INT64_C, not a C octal token.
    if (text.empty() || text.size() > 20) return {};
    auto magnitude = text.starts_with('-') ? text.substr(1) : text;
    if (magnitude.empty() || magnitude.front() < '0' || magnitude.front() > '9' ||
        (magnitude.front() == '0' && magnitude.size() != 1)) return {};
    std::int64_t value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) return {};
    return value;
}

struct Facts {
    bool constant, in_range;

    // This registry is closed. The caller has already established the first
    // four facts via canonical verification, the projected shape and types,
    // and the reference emitter's bounds-check site.
    bool proves(std::string_view name) const {
        if (name == "typed_hir" || name == "fixed_array_layout" ||
            name == "exact_i64_bounds" || name == "emitted_bounds_check") return true;
        if (name == "bounded_checked_i64_evaluation") return constant;
        // Only the bounds call is elided, not any operand. For in-range bounds
        // the reference C11 helper has no effects (including no error/trap).
        if (name == "constant_bounds_in_range" || name == "no_hidden_effect") return in_range;
        return false;
    }
};
#endif
}

bool Bounds::omit_check(const Expr& expr) {
#ifdef ON1X_WITH_MEMGRAPH
    if (!enabled) return false;
    auto keep = [&](std::string_view reason, const compiled::Rule* rule = nullptr) {
        return rule ? record(expr.where, false, reason, rule->name, rule->source, rule->line,
                             rule->diagnostic)
                    : record(expr.where, false, reason);
    };
    if (exhausted || !charge(1)) return keep("budget-exhausted");
    const bool indexing = expr.kind == Expr::Kind::Index;
    const Shape& shape = indexing ? index : slice;
    if ((!indexing && expr.kind != Expr::Kind::Slice) || expr.children.size() != shape.arity ||
        expr.move_out || !expr.text.empty())
        return keep("unsupported-shape");
    if (nodes < shape.arity + 1 || compiled::budget.depth < 2) {
        exhausted = true;
        return keep("budget-exhausted");
    }
    nodes -= shape.arity + 1;

    const auto& base = expr.children[0]->type;
    if (base.kind() != Type::Array) return keep("not-a-fixed-array");
    if (base.elements().size() != 1 || !base.extent() ||
        base.extent() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        base.has_functions() || base.is_move_only())
        return keep("unsupported-layout");
    const auto extent = static_cast<std::int64_t>(base.extent());
    if (indexing ? expr.type != base.elements().front()
                 : (expr.type.kind() != Type::Slice || expr.type.extent() != base.extent() ||
                    expr.type.elements() != base.elements()))
        return keep("unsupported-result");
    for (std::size_t i = 1; i < shape.arity; ++i)
        if (expr.children[i]->type != Type::I64) return keep("non-i64-bound");

    // Bounded evaluation over closed, checked i64 operators only. It proves a
    // value, but never substitutes it for the original runtime operand.
    std::size_t bound_nodes = 64;
    auto evaluate = [&](auto&& self, const Expr& value, std::size_t depth) -> std::optional<std::int64_t> {
        if (depth > 16 || depth > compiled::budget.depth || !bound_nodes) return {};
        if (!nodes) {
            exhausted = true;
            return {};
        }
        if (!charge(1)) return {};
        --bound_nodes;
        --nodes;
        if (value.type != Type::I64 || value.move_out) return {};
        if (value.kind == Expr::Kind::Integer) return decimal(value);
        if (value.kind != Expr::Kind::Unary && value.kind != Expr::Kind::Binary) return {};
        if (value.children.size() != (value.kind == Expr::Kind::Unary ? 1u : 2u)) return {};
        const auto left = self(self, *value.children[0], depth + 1);
        if (!left) return {};
        std::int64_t result = 0;
        if (value.kind == Expr::Kind::Unary)
            return value.text == "-" && on1x_try_sub_i64(0, *left, &result)
                ? std::optional{result} : std::nullopt;
        const auto right = self(self, *value.children[1], depth + 1);
        if (!right) return {};
        bool usable;
        if (value.text == "+") usable = on1x_try_add_i64(*left, *right, &result);
        else if (value.text == "-") usable = on1x_try_sub_i64(*left, *right, &result);
        else if (value.text == "*") usable = on1x_try_mul_i64(*left, *right, &result);
        else if (on1x_valid_divisor_i64(*left, *right)) {
            if (value.text == "/") return *left / *right;
            if (value.text == "%") return *left % *right;
            return {};
        } else return {};
        return usable ? std::optional{result} : std::nullopt;
    };
    const auto lo = evaluate(evaluate, *expr.children[1], 2);
    const auto hi = (indexing || !lo) ? std::nullopt : evaluate(evaluate, *expr.children[2], 2);
    if (exhausted) return keep("budget-exhausted");
    const bool constant = lo && (indexing || hi);
    const Facts facts{constant, constant && (indexing ? on1x_index_in_bounds(*lo, extent)
                                                    : on1x_slice_in_bounds(*lo, *hi, extent))};
    auto obligations = [&](const compiled::Rule& rule) {
        if (!rule.conditions_size || rule.conditions_begin > compiled::conditions.size() ||
            rule.conditions_size > compiled::conditions.size() - rule.conditions_begin)
            return false;
        for (std::size_t i = 0; i < rule.conditions_size; ++i)
            if (!facts.proves(compiled::conditions[rule.conditions_begin + i])) return false;
        return true;
    };
    bool checked = false;
    for (const auto& rule : compiled::rules) {
        if (rule.kind != "check") continue;
        if (!charge(1 + rule.match_size + rule.conditions_size)) return keep("budget-exhausted");
        if (!same_captures(rule, shape)) continue;
        if (!obligations(rule)) return keep("check-not-proved", &rule);
        checked = true;
    }
    if (!checked) return keep("no-check");
    for (const auto& rule : compiled::rules) {
        if (rule.kind != "rewrite") continue;
        if (!charge(1 + rule.match_size + rule.replace_size + rule.conditions_size))
            return keep("budget-exhausted");
        if (!same_captures(rule, shape)) continue;
        // Independently re-establish the complete preservation argument, even
        // if an authored rule omitted a guard: same typed HIR, same operands,
        // same layout and location, and only the provably no-op check is absent.
        if (!constant) return keep("unproved-bound", &rule);
        if (!facts.in_range) return keep("out-of-range", &rule);
        if (!obligations(rule) || !same_replacement(rule, shape))
            return keep("candidate-not-proved", &rule);
        return record(expr.where, true, "in-range", rule.name, rule.source, rule.line);
    }
    return keep("no-rewrite");
#else
    (void)expr;
    return false;
#endif
}

std::string Bounds::report() const {
    std::ostringstream out;
    out << "{\"profile\":\"c11_fixed_array_v1\",\"enabled\":" << (enabled ? "true" : "false")
        << ",\"applied\":" << applied << ",\"retained\":" << retained
        << ",\"exhausted\":" << (exhausted ? "true" : "false")
        << ",\"truncated\":" << (applied + retained > events.size() ? "true" : "false")
        << ",\"events\":[";
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        if (i) out << ',';
        out << "{\"status\":\"" << (event.applied ? "applied" : "retained") << "\",\"reason\":";
        quote(out, event.reason);
        out << ",\"rule\":";
        quote(out, event.rule);
        out << ",\"rule_file\":";
        quote(out, event.rule_file);
        out << ",\"rule_line\":" << event.rule_line << ",\"diagnostic\":";
        quote(out, event.diagnostic);
        out << ",\"file\":";
        quote(out, event.where.file);
        out << ",\"line\":" << event.where.line << ",\"column\":" << event.where.column << '}';
    }
    return out.str() + "]}\n";
}
}

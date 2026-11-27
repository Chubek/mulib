#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x::memgraph {
// Compiler-private, after canonical verification. The engine proves only that
// a fixed-array index/slice check is redundant, not that any operand is dead.
// Each lowering gets a fresh decision budget and queries in its emission order;
// no evidence is attached to HIR or reused for a different unit.
class Bounds {
    struct Event {
        Location where;
        std::string_view reason, rule, rule_file, diagnostic;
        std::size_t rule_line;
        bool applied;
    };
    bool enabled, reporting, exhausted = false;
    std::size_t nodes = 0, steps = 0, applied = 0, retained = 0;
    std::vector<Event> events;

    bool charge(std::size_t work);
    bool record(const Location&, bool omit, std::string_view reason,
                std::string_view rule = {}, std::string_view rule_file = {},
                std::size_t rule_line = 0, std::string_view diagnostic = {});

public:
    Bounds(bool enabled, bool reporting);
    bool omit_check(const Expr&);
    std::string report() const;
};

}

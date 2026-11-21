#pragma once
#include <on1x/c11.hpp>
#include <string_view>

namespace on1x::memgraph {
// Compiler-private, after canonical verification. This engine may change only
// whether the existing C11 bounds call is emitted; HIR and operands are read-only.
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
    explicit Bounds(const C11Options&);
    bool omit_check(const Expr&);
    std::string report() const;
};
}

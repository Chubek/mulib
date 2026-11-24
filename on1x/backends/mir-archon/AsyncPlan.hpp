#pragma once

#include "Bridge.hpp"
#include <optional>

namespace on1x::backends::archon::detail {

// Private, source-only projection of independently rechecked canonical CSP
// HIR. No decoder, foreign symbols, or user-supplied layout/ownership facts.
// Each scalar operand is a function in ordinary verified MIR.
struct Scalar {
    std::size_t function;
    std::vector<std::size_t> inputs;
};
struct Action {
    enum class Kind { Let, Print, Sleep, Return, LaunchFrame, LaunchPeer, Join, Cancel };
    Kind kind;
    Location where;
    std::optional<Scalar> scalar;
    std::size_t index = 0;  // scalar slot, child index, or checked resume state
    bool print = false;    // consume a task and print its result, after cancellation check
    bool bind_result = false; // optionally bind a successful i64 join result to the next scalar slot
};
struct Actor {
    Location where;
    std::vector<Action> actions;
    std::size_t slots = 0;
};
struct TimerProgram {
    mir::Module scalar;
    Actor owner;
    Location entry_at;
    std::optional<Scalar> within;
    std::vector<Actor> frames, peers;
};

TimerProgram project_timer_program(const Unit &, const Options &);

} // namespace on1x::backends::archon::detail

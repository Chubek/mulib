#pragma once
#include <on1x/ast.hpp>
#include <on1x/source_paths.hpp>
#include <map>

namespace on1x {
struct C11Options {
    bool memgraph_bounds = false;
    // Optional compiler report; published only after successful C11 emission.
    std::string* memgraph_report = nullptr;
    // Generated C line checkpoints. An empty file ends the previous range.
    // Locations identify enclosing HIR statements/declarations, not C tokens.
    std::map<std::size_t, Location>* origins = nullptr;
    // Object routes which have not admitted the CSP frame ABI opt out.
    bool allow_stackless_timers = true;
    // Optional logical guest debug paths; does not affect semantic locations.
    const SourcePrefixes* source_prefixes = nullptr;
};

std::string emit_c(const Unit&, const std::string& source_name, const C11Options&);

namespace memgraph {
bool available() noexcept;
}
}

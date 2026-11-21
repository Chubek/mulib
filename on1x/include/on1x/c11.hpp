#pragma once
#include <on1x/ast.hpp>

namespace on1x {
struct C11Options {
    bool memgraph_bounds = false;
    // Optional compiler report; published only after successful C11 emission.
    std::string* memgraph_report = nullptr;
};

std::string emit_c(const Unit&, const std::string& source_name, const C11Options&);

namespace memgraph {
bool available() noexcept;
}
}

#pragma once
#include <on1x/diagnostic.hpp>
#include <optional>

namespace on1x::analysis {
struct Options {
 std::optional<std::string> command_scrutiny;
 std::optional<std::string> project_scrutiny;
 std::size_t work=100000;
 bool repairs=true;
};

struct Statistics {
 bool wetstone_available=false;
 bool complete=true;
 std::size_t functions=0,analyzed_functions=0,skipped_functions=0,work=0;
};

struct Context {
 const diagnostic::Catalog& catalog;
 diagnostic::Registry& diagnostics;
 Options options;
 Statistics statistics;
};

bool wetstone_available() noexcept;
void run(const Unit&,Context&);
}

namespace on1x {
// The ordinary overload remains source-compatible. This overload exposes
// bounded advisory diagnostics to compiler hosts without process-global state.
void check(Unit&,analysis::Context&);
void check_interfaces(Unit,const std::vector<InterfaceObligation>&,analysis::Context&);
}

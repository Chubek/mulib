#include <on1x/c11.hpp>
#include "../src/canonical/memgraph.hpp"
#include <iostream>

namespace {
using namespace on1x;

void require(bool condition, const char* text) {
    if (!condition) throw std::runtime_error(text);
}

void rejected(const Unit& unit, const C11Options& options, std::string& report,
              const char* diagnostic = "ON1X-VALUES-VERIFY") {
    try {
        (void)emit_c(unit, "authority.on1x", options);
    } catch (const Error& error) {
        if (error.diagnostic_id != diagnostic)
            throw std::runtime_error("wrong verifier rejection: " + error.diagnostic_id + ": " + error.what());
        require(report == "unchanged", "published a report for unverified HIR");
        return;
    }
    throw std::runtime_error("C11 admitted corrupted HIR");
}
}

int main() try {
    auto unit = parse(scan("fn main()->i64{let values=array[10,20];return values[1];}", "authority.on1x"));
    check(unit);
    std::string report;
    const C11Options options{true, &report};
    const auto reference = emit_c(unit, "authority.on1x");
    const auto candidate = emit_c(unit, "authority.on1x", options);
    require(reference.find("on1x_bounds_index(") != std::string::npos, "no reference check");
    require(candidate.find("on1x_bounds_index(") == std::string::npos, "proved index check not removed");
    require(report.find("\"applied\":1") != std::string::npos, "missing admission evidence");
    require(emit_c(unit, "authority.on1x") == reference, "candidate mutated the original HIR");

    auto& index = *unit.functions.front().body.back().value;
    require(index.kind == Expr::Kind::Index, "fixture no longer has canonical index expression");
    memgraph::Bounds bounded(options);
    std::size_t proved = 0;
    while (proved < 100000 && bounded.omit_check(index)) ++proved;
    require(proved > 128 && proved < 100000, "bounds decision work budget did not exhaust");
    require(!bounded.omit_check(index), "work resumed after bounds budget exhaustion");
    const auto limited = bounded.report();
    require(limited.find("\"exhausted\":true,\"truncated\":true") != std::string::npos,
            "missing bounds exhaustion or reporting limit");

    report = "unchanged";
    index.type = Type::Bool;
    rejected(unit, options, report);
    index.type = Type::I64;
    index.move_out = true;
    rejected(unit, options, report);
    index.move_out = false;
    index.children[1]->type = Type::U64;
    rejected(unit, options, report);
    index.children[1].reset();
    rejected(unit, options, report, "ON1X-IMPL-HIR");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

#include <on1x/c11.hpp>
#include "../hir/MEMGraph/memgraph.hpp"
#include "../hir/lower_mir.hpp"
#include <algorithm>
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

std::size_t assertions(const mir::Module& module) {
    std::size_t count = 0;
    for (const auto& function : module.functions)
        for (const auto& block : function.blocks)
            count += std::count_if(block.instructions.begin(), block.instructions.end(),
                                  [](const auto& op) { return op.op == mir::Op::Assert; });
    return count;
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

    auto vm = parse(scan("fn get(x:i64)->i64{let a=array[10,20];return a[1]+a[x];}"
                         "fn main(){let a=array[10,20];println(a[2:2][]);println(get(0));}",
                         "vm-bounds.on1x"));
    check(vm);
    const auto checked_vm = mir::lower_for_vm(vm);
    const auto optimized_vm = hir::lower_vm(vm, {}, true);
    mir::verify(optimized_vm);
    require(assertions(checked_vm) == 7 && assertions(optimized_vm) == 2,
            "VM did not remove exactly the proved fixed-array checks");

    auto& index = *unit.functions.front().body.back().value;
    require(index.kind == Expr::Kind::Index, "fixture no longer has canonical index expression");
    memgraph::Bounds bounded(true, true);
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
    try {
        (void)hir::lower_vm(unit, {}, true);
        throw std::runtime_error("VM admitted corrupted HIR");
    } catch (const Error& error) {
        if (error.diagnostic_id != "ON1X-VALUES-VERIFY")
            throw std::runtime_error("wrong VM verifier rejection: " + error.diagnostic_id + ": " + error.what());
    }
    index.children[1].reset();
    rejected(unit, options, report, "ON1X-IMPL-HIR");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

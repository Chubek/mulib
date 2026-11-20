#include <on1x/canonical/values.hpp>
#include <on1x/kernel.hpp>
#include <on1x/loader.hpp>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace on1x;

static Unit checked(const char* fixture) {
    LoaderOptions options;
    options.prelude = false;
    options.kernel_c11 = true;
    auto loaded = load_unit(fixture, options);
    check(loaded.unit);
    return std::move(loaded.unit);
}

template <typename F> static void reject(F action, const char* diagnostic) {
    try {
        action();
    } catch (const Error& error) {
        if (error.diagnostic_id != diagnostic || !error.where.line || !error.where.column) throw;
        return;
    }
    throw std::runtime_error("invalid framed transport HIR admitted");
}

static Expr* find_call(std::vector<Stmt>&, std::string_view);

static Expr* find_call(Expr& expr, std::string_view name) {
    if (expr.kind == Expr::Kind::Call && expr.callee && expr.callee->text == name) return &expr;
    for (auto& child : expr.children)
        if (auto* found = find_call(*child, name)) return found;
    return find_call(expr.body, name);
}

static Expr* find_call(std::vector<Stmt>& body, std::string_view name) {
    for (auto& stmt : body) {
        if (stmt.value)
            if (auto* found = find_call(*stmt.value, name)) return found;
        if (auto* found = find_call(stmt.body, name)) return found;
        if (auto* found = find_call(stmt.otherwise, name)) return found;
    }
    return nullptr;
}

int main(int argc, char** argv) try {
    if (argc != 2) return 2;
    auto unit = checked(argv[1]);
    const auto c11 = emit_c(unit, argv[1]);
    if (c11.find("#include <on1x_kernel_ipc.h>") == std::string::npos)
        throw std::runtime_error("framed transport did not lower through the private header");

    auto adapter = std::find_if(unit.functions.begin(), unit.functions.end(), [](const Function& f) {
        return f.kernel_symbol == ipc_kernel_roundtrip;
    });
    if (adapter == unit.functions.end()) throw std::runtime_error("missing roundtrip declaration");
    Expr* call = nullptr;
    for (auto& f : unit.functions)
        if ((call = find_call(f.body, adapter->name))) break;
    if (!call) throw std::runtime_error("missing roundtrip call");

    adapter->kernel_link_only = true;
    reject([&] { canonical::verify_values(unit); }, "ON1X-KERNEL-ABI");
    adapter->kernel_link_only = false;
    auto closure = adapter->params[3].type;
    adapter->params[3].type = Type::function({Type::I64}, Type::Bool);
    reject([&] { emit_c(unit, argv[1]); }, "ON1X-KERNEL-ABI");
    adapter->params[3].type = closure;

    auto operand = std::move(call->children[0]);
    if (operand->kind != Expr::Kind::Call || operand->callee->text != "move")
        throw std::runtime_error("missing checked transfer");
    operand->children[0]->move_out = false;
    call->children[0] = std::move(operand->children[0]);
    reject([&] { emit_c(unit, argv[1]); }, "ON1X-VALUES-VERIFY");

    const auto session_fixture = (std::filesystem::path(argv[1]).parent_path()/"framed_session.on1x").string();
    auto session = checked(session_fixture.c_str());
    const auto session_c11 = emit_c(session, session_fixture);
    if (session_c11.find("_ipc_loan = ") == std::string::npos)
        throw std::runtime_error("no direct session loan in generated C");
    auto poll = std::find_if(session.functions.begin(), session.functions.end(), [](const Function& f) {
        return f.kernel_symbol == ipc_kernel_poll;
    });
    if (poll == session.functions.end()) throw std::runtime_error("missing poll declaration");
    Expr* poll_call = nullptr;
    for (auto& f : session.functions)
        if ((poll_call = find_call(f.body, poll->name))) break;
    if (!poll_call) throw std::runtime_error("missing poll call");
    poll_call->children[0]->move_out = true;
    reject([&] { emit_c(session, session_fixture); }, "ON1X-REF");
    poll_call->children[0]->move_out = false;
    poll->params[0].borrow = BorrowMode::Shared;
    reject([&] { canonical::verify_values(session); }, "ON1X-KERNEL-ABI");

    std::cout << "framed transport HIR rejects promotion, ABI drift and malformed transfers and loans\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

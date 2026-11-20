#include <on1x/canonical/values.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/loader.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace on1x;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void reject(const Unit& unit, std::string_view diagnostic) {
    try {
        canonical::verify_values(unit);
    } catch (const Error& error) {
        if (error.diagnostic_id != diagnostic || !error.where.line || !error.where.column)
            throw;
        return;
    }
    throw std::runtime_error("malformed shared owner survived HIR verification");
}

static Expr* find(std::vector<Stmt>&, std::string_view);

static Expr* find(Expr& expr, std::string_view name) {
    if (expr.kind == Expr::Kind::Call && expr.callee && expr.callee->text == name)
        return &expr;
    for (auto& child : expr.children)
        if (auto* found = find(*child, name)) return found;
    return find(expr.body, name);
}

static Expr* find(std::vector<Stmt>& body, std::string_view name) {
    for (auto& stmt : body) {
        if (stmt.value)
            if (auto* found = find(*stmt.value, name)) return found;
        if (auto* found = find(stmt.body, name)) return found;
        if (auto* found = find(stmt.otherwise, name)) return found;
    }
    return nullptr;
}

int main(int argc, char** argv) try {
    if (argc != 3) return 2;
    LoaderOptions options;
    options.stdlib = argv[2];
    options.stdlib_explicit = true;
    auto loaded = load_unit(argv[1], options);
    check(loaded.unit);
    auto& unit = loaded.unit;
    auto main = std::find_if(unit.functions.begin(), unit.functions.end(),
                            [](const Function& f) { return f.name == "main"; });
    require(main != unit.functions.end(), "checked main missing");
    const auto c11 = emit_c(unit, argv[1]);
    require(c11.find("#include <on1x_container_shared.h>") != std::string::npos,
            "shared container private ABI header missing");

    auto* count = find(main->body, "@shared_v1_arc_parallel_count");
    require(count != nullptr, "no parallel count intrinsic in checked HIR");
    count->children[0]->move_out = true;
    reject(unit, "ON1X-SHARED");
    count->children[0]->move_out = false;

    auto* clone = find(main->body, "@shared_v1_arc_clone");
    require(clone != nullptr, "no clone intrinsic in checked HIR");
    const auto type = clone->type;
    clone->type = shared_owner(SharedKind::Rc, type.owned().arguments().front());
    reject(unit, "ON1X-SHARED");
    clone->type = type;

    auto* node = find(main->body, "@shared_v1_rc_new");
    require(node && node->children[0]->type.is_own(), "no moved unique payload");
    node->callee->text = "@shared_v1_arc_new";
    reject(unit, "ON1X-SHARED");
    node->callee->text = "@shared_v1_rc_new";

    canonical::verify_values(unit);
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

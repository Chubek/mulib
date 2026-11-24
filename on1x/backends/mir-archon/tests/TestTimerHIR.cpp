#include "Bridge.hpp"

#include <iostream>

namespace {
constexpr auto source = R"(fn main() { taskgroup {
    let f = launch async { let x = 20; await timer(x - 15); await timer(0); x * 2 + 2 };
    println(join f);
} })";
on1x::Unit checked() {
    auto unit = on1x::parse(on1x::scan(source, "timer-hir"));
    on1x::check(unit);
    return unit;
}
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
} // namespace

int main() try {
    namespace archon = on1x::backends::archon;
    const auto unit = checked();
    const auto product = archon::compile_timer_hir(unit);
    require(product.target == "AMD64/SysVAMD64" &&
            product.assembly.find("on1x_archon_timer_resume_0") != std::string::npos,
            "native timer resume callback missing");
    for (int i = 0; i < 7; ++i) {
        auto forged = checked();
        auto &group = forged.functions.front().body.front();
        auto &frame = *group.body.front().value->children.front();
        auto &sleep = *frame.body[1].value;
        auto &delay = *sleep.children.front();
        switch (i) {
        case 0: sleep.children[1]->text = "2"; break;
        case 1: delay.type = on1x::Type::Bool; break;
        case 2: frame.captures.push_back({"outer", on1x::Type::I64}); break;
        case 3: frame.body[0].mutable_binding = true; break;
        case 4:
            delay.kind = on1x::Expr::Kind::Variable;
            delay.reference = on1x::Expr::Reference::Local;
            delay.text = "not_initialized";
            delay.children.clear();
            break;
        case 5: sleep.kind = on1x::Expr::Kind::SourceCsp; break;
        case 6: group.body.pop_back(); break; // unconsumed child
        }
        try {
            (void)archon::compile_timer_hir(forged);
        } catch (const on1x::Error &) {
            continue;
        }
        throw std::runtime_error("forged timer HIR reached native code generation: " + std::to_string(i));
    }
    for (auto status : {ARCHON_BACKEND_CANCELLED, ARCHON_BACKEND_LIMIT, ARCHON_BACKEND_UNSUPPORTED}) {
        archon::Options options;
        if (status == ARCHON_BACKEND_CANCELLED) options.cancelled = +[](void *) { return 1; };
        if (status == ARCHON_BACKEND_LIMIT) options.max_instructions_per_module = 8;
        if (status == ARCHON_BACKEND_UNSUPPORTED) options.emit_c_entrypoint = false;
        try {
            (void)archon::compile_timer_hir(unit, options);
        } catch (const archon::Error &e) {
            require(e.status == status, e.what());
            continue;
        }
        throw std::runtime_error("native timer ignored options");
    }
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}

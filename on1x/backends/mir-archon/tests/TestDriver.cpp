#include "Builtin.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

template<class F> void rejects(F action) {
    try { action(); } catch (const std::exception &) { return; }
    CHECK(false);
}

int main() {
    namespace mir = on1x::mir;
    namespace backend = on1x::backends;
    const on1x::Location where{"native-driver.on1x", 0, 1, 1};
    mir::Module module;
    module.functions.push_back({2, "main", on1x::Type::I64, where, {}, {
        {3, 4, where, {}, {{5, on1x::Type::I64, mir::Op::Constant, {}, 42, 0, 0, 0, where}},
         {6, mir::Exit::Return, 5, 4, {}, where}}
    }});
    constexpr auto target = "x86_64-unknown-linux-gnu";
    auto artifact = backend::run_mir_native(module, target);
    CHECK(artifact.exit_code == 0 && !artifact.bytes.empty());
    CHECK(artifact.bytes == backend::run_mir_native(module, target).bytes);
    rejects([&] { backend::run_mir_native(module, "x86_64-pc-linux-gnu"); });
    rejects([&] { backend::run_mir_native(module, "aarch64-unknown-linux-gnu"); });
    backend::NativeOptions options;
    options.byte_limit = 1;
    rejects([&] { backend::run_mir_native(module, target, options); });
    options = {};
    options.cancelled = [](void *) { return 1; };
    rejects([&] { backend::run_mir_native(module, target, options); });
    int polls = 0;
    options.cancel_userdata = &polls;
    options.cancelled = [](void *state) -> int { return ++*static_cast<int *>(state) >= 4; };
    rejects([&] { backend::run_mir_native(module, target, options); });
    CHECK(polls >= 4);
    options = {};
    options.emit_c_entrypoint = false;
    CHECK(backend::run_mir_native(module, target, options).bytes != artifact.bytes);
    options.pic = true;
    CHECK(!backend::run_mir_native(module, target, options).bytes.empty());
    module.functions[0].blocks[0].instructions[0].type = on1x::Type::Bool;
    rejects([&] { backend::run_mir_native(module, target); });
}

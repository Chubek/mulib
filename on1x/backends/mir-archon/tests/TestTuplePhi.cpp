#include "Bridge.hpp"

#include <iostream>
#include <stdexcept>

namespace {
namespace m = on1x::mir;
using on1x::Type;
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
}
int main() try {
    const on1x::Location at{"tuple-phi.on1x", 0, 1, 1};
    const Type pair = Type::tuple({Type::I64, Type::I64});
    m::Module module;
    module.version = m::format_version;
    m::Function fn;
    fn.id = 2; fn.name = "main"; fn.result = Type::I64; fn.where = at;
    m::Block entry;
    entry.id = 3; entry.effect = 4; entry.where = at;
    entry.instructions = {
        {5, Type::I64, m::Op::Constant, {}, 10, 0, 0, 0, at},
        {6, Type::I64, m::Op::Constant, {}, 20, 0, 0, 0, at},
        {7, Type::Bool, m::Op::Constant, {}, 1, 0, 0, 0, at},
        {8, pair, m::Op::Tuple, {5, 6}, 0, 0, 0, 0, at},
        {9, pair, m::Op::Tuple, {6, 5}, 0, 0, 0, 0, at}
    };
    entry.exit = {10, m::Exit::Jump, 0, 4, {{11, {8, 9, 7}}}, at};

    m::Block loop;
    loop.id = 11; loop.effect = 12; loop.where = at;
    loop.parameters = {{13, pair, at}, {14, pair, at}, {15, Type::Bool, at}};
    loop.instructions = {
        {16, Type::Bool, m::Op::Constant, {}, 0, 0, 0, 0, at}
    };
    loop.exit = {17, m::Exit::Branch, 15, 12,
                 {{11, {14, 13, 16}}, {18, {13}}}, at};

    m::Block done;
    done.id = 18; done.effect = 19; done.where = at;
    done.parameters = {{20, pair, at}};
    done.instructions = {
        {21, Type::I64, m::Op::Extract, {20}, 0, 0, 0, 0, at}
    };
    done.exit = {22, m::Exit::Return, 21, 19, {}, at};

    fn.blocks = {entry, loop, done};
    module.functions.push_back(std::move(fn));
    m::verify(module);
    auto graph = on1x::backends::archon::lower(module);
    const auto &native = graph.view();
    require(native.function_count == 2, "lost entrypoint");
    const auto &body = native.functions[0];
    require(body.block_count == 6, "tuple-carrying edges were not split");
    require(body.blocks[0].term.yes.target >= 3, "entry must transfer via proxy");
    require(body.blocks[1].term.yes.target >= 3 &&
            body.blocks[1].term.no.target >= 3, "branch edges must be independently split");
    for (std::size_t n = 3; n < body.block_count; ++n) {
        require(body.blocks[n].term.kind == ARCHON_MIR_BR, "proxy must use direct branch");
        require(body.blocks[n].inst_count > 0, "proxy must snapshot aggregates");
    }
    const auto emitted = on1x::backends::archon::compile(module);
    require(!emitted.assembly.empty(), "loop/phi assembly missing");
    std::cout << "on1x.archon tuple phi staging, swaps and loops: OK\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "on1x.archon tuple phi: " << e.what() << '\n';
    return 1;
}

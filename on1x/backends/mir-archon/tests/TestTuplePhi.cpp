#include "Bridge.hpp"

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace {
namespace m = on1x::mir;
using on1x::Type;
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
// Track pointer roots to distinguish source payload reads, staging storage,
// and destination phi slots. Counting stores alone cannot detect sequential
// (rather than simultaneous) transfers of two cyclic tuple parameters.
void check_staging(const ArchonMIRBlock &block, std::size_t words) {
    struct Root { bool parameter; std::size_t index; };
    std::map<ArchonValue, Root> roots;
    std::set<std::size_t> destinations;
    for (std::size_t n = 0; n < block.param_count; ++n)
        roots.emplace(block.params[n], Root{true, n});
    for (std::size_t n = 0; n < block.inst_count; ++n) {
        const auto &inst = block.insts[n];
        if (inst.op == ARCHON_MIR_FRAME_ADDR)
            roots.emplace(inst.dst, Root{false, inst.imm});
        else if (inst.op == ARCHON_MIR_PTR_ADD || inst.op == ARCHON_MIR_COPY)
            roots.emplace(inst.dst, roots.at(inst.a));
    }
    for (std::size_t n = 0; n < block.term.yes.arg_count; ++n) {
        const auto found = roots.find(block.term.yes.args[n]);
        if (found != roots.end() && !found->second.parameter)
            destinations.insert(found->second.index);
    }
    require(!destinations.empty(), "proxy does not pass owned phi storage");
    std::size_t source_reads = 0, commits = 0, staging_writes = 0;
    bool committing = false;
    for (std::size_t n = 0; n < block.inst_count; ++n) {
        const auto &inst = block.insts[n];
        if (inst.op == ARCHON_MIR_LOAD && roots.at(inst.a).parameter) {
            require(!committing, "source tuple read after phi storage mutation");
            ++source_reads;
        }
        if (inst.op == ARCHON_MIR_STORE) {
            const auto root = roots.at(inst.a);
            require(!root.parameter, "proxy mutates borrowed source storage");
            if (destinations.contains(root.index)) {
                require(source_reads == destinations.size() * words,
                        "phi commit began before all sources were staged");
                committing = true;
                ++commits;
            } else {
                require(!committing, "staging write after commit began");
                ++staging_writes;
            }
        }
    }
    require(source_reads == destinations.size() * words &&
            commits == source_reads && staging_writes == source_reads,
            "proxy lost or duplicated tuple payload words");
}
void fixture(bool nested, bool same_target, bool choose_first) {
    const on1x::Location at{"tuple-phi.on1x", 0, 1, 1};
    const Type leaf = Type::tuple({Type::I64, Type::I64});
    const Type pair = nested ? Type::tuple({leaf, leaf}) : leaf;
    m::Module module;
    module.version = m::format_version;
    m::Function fn;
    fn.id = 2; fn.name = "main"; fn.result = Type::I64; fn.where = at;
    m::Block entry;
    entry.id = 3; entry.effect = 4; entry.where = at;
    entry.instructions = {
        {5, Type::I64, m::Op::Constant, {}, 10, 0, 0, 0, at},
        {6, Type::I64, m::Op::Constant, {}, 20, 0, 0, 0, at},
        {7, Type::Bool, m::Op::Constant, {}, 1, 0, 0, 0, at}
    };
    if (nested) {
        entry.instructions.push_back({23, leaf, m::Op::Tuple, {5, 6}, 0, 0, 0, 0, at});
        entry.instructions.push_back({24, leaf, m::Op::Tuple, {6, 5}, 0, 0, 0, 0, at});
    }
    entry.instructions.push_back({8, pair, m::Op::Tuple,
                                  nested ? std::vector<m::Id>{23, 24} : std::vector<m::Id>{5, 6},
                                  0, 0, 0, 0, at});
    entry.instructions.push_back({9, pair, m::Op::Tuple,
                                  nested ? std::vector<m::Id>{24, 23} : std::vector<m::Id>{6, 5},
                                  0, 0, 0, 0, at});
    entry.exit = {10, m::Exit::Jump, 0, 4, {{11, {8, 9, 7}}}, at};
    if (same_target) {
        entry.instructions.push_back({26, Type::Bool, m::Op::Constant, {},
                                      choose_first ? 1 : 0, 0, 0, 0, at});
        entry.exit = {10, m::Exit::Branch, 26, 4,
                      {{11, {8, 9, 7}}, {11, {9, 8, 7}}}, at};
    }

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
    done.instructions = {{21, nested ? leaf : Type{Type::I64},
                          m::Op::Extract, {20}, 0, 0, 0, 0, at}};
    if (nested)
        done.instructions.push_back({25, Type::I64, m::Op::Extract, {21}, 0, 0, 0, 0, at});
    done.exit = {22, m::Exit::Return, nested ? 25u : 21u, 19, {}, at};

    fn.blocks = {entry, loop, done};
    module.functions.push_back(std::move(fn));
    m::verify(module);
    const auto result = m::evaluate(module);
    require(result.status == m::Status::Returned &&
            result.value == (same_target && !choose_first ? 10 : 20),
            "tuple reference swap result mismatch");
    auto graph = on1x::backends::archon::lower(module);
    const auto &native = graph.view();
    require(native.function_count == 2, "lost entrypoint");
    const auto &body = native.functions[0];
    require(body.block_count == (same_target ? 7u : 6u), "tuple-carrying edges were not split");
    require(body.blocks[0].term.yes.target >= 3, "entry must transfer via proxy");
    if (same_target)
        require(body.blocks[0].term.yes.target != body.blocks[0].term.no.target,
                "two conditional edges to one destination share a proxy");
    require(body.blocks[1].term.yes.target >= 3 &&
            body.blocks[1].term.no.target >= 3, "branch edges must be independently split");
    for (std::size_t n = 3; n < body.block_count; ++n) {
        require(body.blocks[n].term.kind == ARCHON_MIR_BR, "proxy must use direct branch");
        require(body.blocks[n].inst_count > 0, "proxy must snapshot aggregates");
        check_staging(body.blocks[n], nested ? 4 : 2);
    }
    const auto emitted = on1x::backends::archon::compile(module);
    require(!emitted.assembly.empty(), "loop/phi assembly missing");
}
}
int main() try {
    for (bool nested : {false, true}) {
        fixture(nested, false, true);
        fixture(nested, true, true);
        fixture(nested, true, false);
    }
    std::cout << "on1x.archon tuple phi staging, swaps and loops: OK\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "on1x.archon tuple phi: " << e.what() << '\n';
    return 1;
}

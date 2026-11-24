// A private ffgen helper. This analyzes only the supported callee-value
// projection, never loads a foreign library or runs foreign code.
#include <wetstone/path-analysis/TaintAnalysis.hpp>
#include <charconv>
#include <iostream>
#include <string_view>

int main() {
    namespace ir = wetstone::ir;
    constexpr std::size_t max_sites = 4096;
    // One bounded row per AST-matched call: projected arity, callee parameter.
    for (std::size_t row = 0; ; ++row) {
        char buffer[32]{};
        std::cin.getline(buffer, sizeof(buffer));
        if (std::cin.eof() && std::cin.gcount() == 0) return 0;
        if (!std::cin || std::cin.eof() || row >= max_sites) return 2;
        const std::string_view text(buffer, static_cast<std::size_t>(std::cin.gcount() - 1));
        const auto split = text.find(' ');
        if (split == text.npos) return 2;
        ir::ValueId count{}, parameter{};
        const auto* middle = text.data() + split;
        const auto* end = text.data() + text.size();
        const auto first = std::from_chars(text.data(), middle, count);
        const auto second = std::from_chars(middle + 1, end, parameter);
        if (first.ec != std::errc{} || first.ptr != middle || second.ec != std::errc{} ||
            second.ptr != end || count > 1024 || parameter >= count) return 2;
        // Project just the caller-provided value read by the indirect call
        // target. There is no C semantics for the call or its result here.
        ir::Function function;
        function.entry = 0;
        function.blocks.push_back({0, {
            {ir::Opcode::Input, parameter, {}, {}, {}, {}, 0},
            {ir::Opcode::Copy, count, {ir::Operand::ref(parameter)}, {}, {}, {}, 0},
            {ir::Opcode::Return, {}, {}, {}, {}, {}, 0},
        }});
        function.validate();
        const auto flow = wetstone::path::taint(function, 8);
        if (!flow.converged) return 3;
        std::cout << (flow.out.at(0).values.contains(count) ? "tainted\n" : "not_established\n");
        if (!std::cout) return 4;
    }
}

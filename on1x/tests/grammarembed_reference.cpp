// Independent liblr runtime oracle. GrammarEmbed emits its own On1x engines;
// this executable runs liblr's C++ engines directly on the same language.
#include <liblr/LALR.hpp>
#include <liblr/LexerGenerator.hpp>
#include <liblr/LLTableDriven.hpp>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    namespace lr = firestone::lr;
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    auto grammar = lr::readEBNF("TOP = 'N' ('+' 'N')*;");
    const auto n = *grammar.findTerminal("N"), plus = *grammar.findTerminal("+");
    const auto ws = grammar.terminal("WS");
    const auto lexer = lr::make_lexer({{"[0-9]+", n}, {"\\+", plus}, {"[ \\t\\r\\n]+", ws, true}});
    lr::LRParser parser(grammar, mode == "lr1" ? lr::LRMode::canonicalLR1 : lr::LRMode::lalr1);
    lr::LLTableDriven ll(grammar);
    std::string input;
    while (std::getline(std::cin, input)) {
        std::vector<lr::Symbol> tokens;
        std::size_t offset = 0; bool valid = true;
        for (;;) {
            auto token = lexer.next(input, offset);
            if (token.status == lr::LexStatus::End) break;
            if (token.status == lr::LexStatus::Error) { valid = false; break; }
            tokens.push_back(token.token);
        }
        if (valid) valid = mode == "ll1" ? ll.parse(tokens).has_value() : parser.parse(tokens).has_value();
        std::cout << (valid ? "true\n" : "false\n");
    }
}

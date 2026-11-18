#include "../stdppx/grammarembed/bridge.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    assert(on1x_grammar_compile(
        "grammar Calc :lexer<dfa> :parser<lr1> {"
        "token INT {<[0-9]>+} token WS :skip {\\s+}"
        "rule TOP {<INT> ['+' <INT>]*}}"));
    const auto* plan = on1x_grammar_plan();
    assert(std::strstr(plan, "\"Calc\""));
    assert(std::strstr(plan, "@ACTIONS@"));
    assert(!*on1x_grammar_error());
    assert(!on1x_grammar_compile("grammar X {token T {'x'*} rule TOP {<T>}}"));
    assert(!*on1x_grammar_plan() && *on1x_grammar_error());
    assert(!on1x_grammar_compile(nullptr));
    assert(!*on1x_grammar_plan());
    std::cout << "GrammarEmbed bridge PASS\n";
}

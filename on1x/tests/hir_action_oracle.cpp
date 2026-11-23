#include <novostone/ActionSemantics/ActionSemantics.hpp>
#include <iostream>

// Independent, generated checked-i32 reference for the common arithmetic
// subset. Not linked into on1xc and never runs on behalf of source programs.
int main() {
    namespace action = novostone::action;
    const auto language = action::parse(R"(
      language CheckedWords {
        start Expr;
        syntax Expr = Num(n: integer) | Add(a: Expr,b: Expr) | Sub(a: Expr,b: Expr)
                    | Mul(a: Expr,b: Expr) | Eq(a: Expr,b: Expr);
        rule Num = give n;
        rule Add = (evaluate(a) and then evaluate(b)) then give given(1) + given(2);
        rule Sub = (evaluate(a) and then evaluate(b)) then give given(1) - given(2);
        rule Mul = (evaluate(a) and then evaluate(b)) then give given(1) * given(2);
        rule Eq = (evaluate(a) and then evaluate(b)) then give given(1) == given(2);
      }
    )", "checked-hir-oracle.acsem");
    std::cout << action::generate(language, action::Target::C);
    return std::cout ? 0 : 1;
}

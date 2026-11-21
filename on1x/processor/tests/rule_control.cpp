#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <iostream>
#include <string_view>

// Linked to the real frontend and a production processor.cpp compiled against
// each generated fixture table. The .o1m input is the only behavioral difference.
int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("expected add, subtract or forbid table");
    const bool add = std::string_view(argv[1]) == "add";
    const bool forbid = std::string_view(argv[1]) == "forbid";
    if (!add && !forbid && std::string_view(argv[1]) != "subtract")
      throw std::runtime_error("unknown fixture table");
    auto unit = on1x::parse(on1x::scan(
      "fn identity(x:i64)->i64{return x+0;}"
      "fn main(){println(identity(42));}", "rule-control.on1x"));
    try {
      on1x::check(unit);
    } catch (const on1x::Error& error) {
      if (!forbid) throw;
      const std::string_view message = error.what();
      if (error.diagnostic_id != "ON1X-PROCESSOR-SEMANTIC" ||
          error.where.file != "rule-control.on1x" ||
          message.find("fixture policy forbids integer literals") == std::string_view::npos ||
          message.find("forbid_integer at forbid_zero.o1m:3:1") == std::string_view::npos)
        throw std::runtime_error("authored semantic rejection lost rule/source provenance");
      std::cout << "forbid-integer: checked source rejected by authored semantic constraint\n";
      return 0;
    }
    if (forbid) throw std::runtime_error("authored semantic constraint did not reject source");
    const auto& result = *unit.functions.at(0).body.at(0).value;
    const auto expected = add ? on1x::Expr::Kind::Variable : on1x::Expr::Kind::Binary;
    if (result.kind != expected)
      throw std::runtime_error("authored .o1m pattern did not control checked HIR");
    if (result.where.file != "rule-control.on1x" || result.where.line != 1)
      throw std::runtime_error("rule replacement lost original source provenance");
    const auto evaluated = on1x::mir::evaluate(on1x::mir::lower(unit));
    if (evaluated.output != "42\n")
      throw std::runtime_error("generated rewrite changed source execution");
    std::cout << (add ? "add-zero: Variable" : "subtract-zero: Binary")
              << "; checked source evaluated to 42\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

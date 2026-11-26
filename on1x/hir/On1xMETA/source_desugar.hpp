#pragma once
#include <on1x/operator_lowering.hpp>
#include <array>
#include <cstdint>

namespace on1x::meta::source {
enum class Form {
 OperatorReference, OperatorUnary, OperatorBinary, OperatorTernary,
 OperatorDeclaration, FixityDirective, Count
};
enum class Replacement { BoundOperator, Call, DefineOperator, Consumed };
struct Rule {
 const char* name;
 Form form;
 Replacement replacement;
 std::uint32_t captures;
 std::array<std::uint32_t,3> order;
 const char* file;
 std::uint32_t line,column;
};
struct Budgets { std::uint32_t steps,nodes,depth; };

// The host validates declaration identity, visibility provenance and source
// shape before invoking these constructors; rule code cannot name a callee.
Form form_of(Expr::Kind);
const Rule& select(Form, const Location&);
Budgets limits();

struct ExpressionEdit {
 Expr* target;
 Expr ordinary;
 const Rule* rule;
 void publish() noexcept;
};

struct DefinitionEdit {
 OperatorDeclaration* target;
 // Move the live payload into host-reserved storage without cloning it.
 void publish(std::vector<Function>&) noexcept;
};

class Program {
 std::array<std::size_t,static_cast<std::size_t>(Form::Count)> hits{};
 void hit(const Rule&);
public:
 ExpressionEdit expression(Expr&, const std::string& binding, const Rule&);
 DefinitionEdit definition(OperatorDeclaration&);
 void fixity(const FixityDirective&);
 SourceDesugarReport report(std::size_t definitions) const;
};
}

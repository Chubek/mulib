#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x {
inline bool is_source_operator(Expr::Kind kind){
 switch(kind){
  case Expr::Kind::OperatorReference:case Expr::Kind::OperatorUnary:
  case Expr::Kind::OperatorBinary:case Expr::Kind::OperatorTernary:return true;
  default:return false;
 }
}

namespace meta {
struct SourceDesugarReport {
 struct Hit {
  std::string_view name,file;
  std::size_t count,line,column;
 };
 std::string_view program_sha256;
 std::vector<Hit> rules;
 std::size_t rewrites=0,definitions=0,fixities=0;
 bool verified=false;
};

// Mandatory source phase, after macro and authorized PPX rewriting. Bindings
// must already be assigned by the host. No typing or ownership evidence is
// attached to the resulting ordinary functions, calls and function references.
SourceDesugarReport desugar_source_operators(Unit&);
}
}

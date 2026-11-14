#pragma once
#include <on1x/type.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace on1x {
inline constexpr std::size_t max_source_bytes = 2 * 1024 * 1024;
inline constexpr std::size_t max_syntax_depth = 256;
struct Location { std::string file; std::size_t offset = 0, line = 1, column = 1; };
struct Error : std::runtime_error { Location where; Error(Location at, std::string message) : std::runtime_error(std::move(message)), where(std::move(at)) {} };
struct Token { std::string text; Location where; };
struct Capture { std::string name; Type type; };
struct Pattern {
 Location where;
 std::string name;
 std::size_t context=0; // Syntax context, resolved before inference.
 std::vector<Pattern> elements; // Empty for a name; at least two for a tuple.
 bool is_tuple() const { return !elements.empty(); }
};
struct Param {
 std::string name; Type type; Location where;
 std::size_t context=0;
 // Pattern parameters have unlexable internal names, never emitted as C names.
 std::optional<Pattern> pattern;
};
struct Stmt;
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
  enum class Kind { Integer, Boolean, String, Variable, Unary, Binary, Call, Lambda, Tuple, PolyPack, PolyProjection,
                    MacroCall, SyntaxHole, SyntaxBlock, Extension, SyntaxList, SyntaxVector, SyntaxKeyword,
                    Quote, Splice, Lift, Run } kind;
 enum class Reference { Unresolved, Local, Function, Extern, Intrinsic } reference = Reference::Unresolved;
 Location where;
 std::string text;
 std::size_t context=0;
 std::vector<ExprPtr> children;
 std::vector<ExprPtr> proofs; // Universality checks only; never runtime operands.
 std::size_t projection = 0;
 bool prototype = false;
 std::size_t syntax_repeat = 0;
 ExprPtr syntax_tail; // Dotted syntax, never an ordinary expression operand.
 ExprPtr callee; // Call operands: callee first, followed by children (arguments).
 Type type = Type::Invalid;
 std::vector<Param> parameters;
 Type result = Type::Void;
 std::vector<Stmt> body;
 std::vector<Capture> captures; // Checked immutable value captures, in first-use order.
 std::size_t syntax_depth = 1; // Parser bound includes anonymous-function bodies.
 // The current frontend does not admit escaping json/json_view values.
};
struct Stmt {
  enum class Kind { Let, Assign, Return, If, While, Expr, Block, Template } kind;
 Location where;
 std::string name;
 std::size_t context=0;
 std::optional<Pattern> pattern; // Local tuple destructuring only; names use name.
 Type declared = Type::Invalid;
 bool mutable_binding = false;
 bool prototype = false; // Type-checked generic instance, never emitted.
 std::vector<std::unique_ptr<Stmt>> instances; // Stable storage during inference.
 ExprPtr value;
 std::vector<Stmt> body, otherwise;
};
struct Function {
 std::string name;
 Location where;
 std::vector<Param> params;
 Type result = Type::Void;
 bool external = false;
 bool prototype = false; // Checked generalized declaration, not emitted.
 std::vector<Stmt> body;
 std::vector<ExprPtr> attributes;
};
struct MacroParameter {
 enum class Kind { Capture, Literal, Wildcard, List, Vector, Tuple, Call, Unary, Binary } kind=Kind::Capture;
 Location where;std::string name,category;bool repeated=false;
 ExprPtr literal; // Non-null for a binding-sensitive literal pattern.
 std::vector<MacroParameter> elements;
 std::unique_ptr<MacroParameter> tail;
};
struct MacroRule {
 Location where;std::vector<MacroParameter> parameters;
 bool block=false;ExprPtr expression;std::vector<Stmt> body;
 std::unique_ptr<MacroParameter> tail;
};
struct Macro {Location where;std::string name;std::vector<MacroRule> rules;std::string ellipsis="...";std::vector<std::string> literals;};
struct PpxRegistration {Location where;std::string label,program;};
struct Unit {
 std::vector<std::string> imports; std::vector<Function> functions;
 std::map<Type,std::vector<Type>> forall_layouts; // Internal whole-unit demand ABI.
 bool forall_elaborated=false;
 bool stages_lowered=false;
 std::vector<Macro> macros;
 std::vector<PpxRegistration> ppx;
};
std::string read_source_file(const std::string& filename);
std::vector<Token> scan(const std::string& source, const std::string& filename);
Unit parse(const std::vector<Token>& tokens);
bool is_intrinsic_name(const std::string& name);
std::vector<Type> extern_parameter_types(const Function& function);
void check(Unit& unit);
std::string emit_c(const Unit& unit, const std::string& source_name);
}

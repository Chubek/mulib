#pragma once
#include <on1x/type.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
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
struct Param { std::string name; Type type; Location where; };
struct Capture { std::string name; Type type; };
struct Stmt;
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
 enum class Kind { Integer, Boolean, String, Variable, Unary, Binary, Call, Lambda } kind;
 enum class Reference { Unresolved, Local, Function, Extern, Intrinsic } reference = Reference::Unresolved;
 Location where;
 std::string text;
 std::vector<ExprPtr> children;
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
 enum class Kind { Let, Assign, Return, If, While, Expr, Block } kind;
 Location where;
 std::string name;
 Type declared = Type::Invalid;
 bool mutable_binding = false;
 ExprPtr value;
 std::vector<Stmt> body, otherwise;
};
struct Function {
 std::string name;
 Location where;
 std::vector<Param> params;
 Type result = Type::Void;
 bool external = false;
 std::vector<Stmt> body;
};
struct Unit { std::vector<std::string> imports; std::vector<Function> functions; };
std::string read_source_file(const std::string& filename);
std::vector<Token> scan(const std::string& source, const std::string& filename);
Unit parse(const std::vector<Token>& tokens);
bool is_intrinsic_name(const std::string& name);
void check(Unit& unit);
std::string emit_c(const Unit& unit, const std::string& source_name);
}

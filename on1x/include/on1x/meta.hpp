#pragma once
#include <on1x/ast.hpp>
#include <map>
#include <string_view>
namespace on1x::meta {
inline constexpr std::size_t max_wire_bytes=4*1024*1024;
inline constexpr std::size_t max_wire_nodes=262144;
struct SExpr {
 enum class Kind { Symbol, String, List } kind=Kind::List;
 std::string text;std::vector<SExpr> items;Location where;
};
SExpr symbol(std::string text);
SExpr string(std::string text);
SExpr list(std::vector<SExpr> items);
SExpr read_sexpr(std::string_view input,const std::string& file);
std::string write_sexpr(const SExpr& node);
struct SchemaNode {std::string category,owner;std::vector<std::pair<std::string,std::string>> fields;};
struct Schema {
 std::map<std::string,std::string> parents;
 std::map<std::string,SchemaNode> nodes;
 bool subtype(std::string child,const std::string& parent)const;
 void validate(const SExpr& tree,const std::string& expected="Unit",const std::string& path="$")const;
 void extend(std::string_view json,const std::string& label,const std::string& file);
};
Schema base_schema();
std::string literal_value(const std::string& text);
std::string source_literal(const std::string& value);
SExpr encode(const Unit& unit);
Unit decode(const SExpr& tree);
void require_core(const Unit& unit);
struct PpxOptions {
 std::vector<PpxRegistration> programs;
 std::map<std::string,std::string> schemas;
 bool allow_source=false;
 unsigned timeout_ms=5000;
};
std::string run_transformer(const std::string& program,std::string_view input,unsigned timeout_ms,const std::string& label);
void rewrite(Unit& unit,const PpxOptions& options);
}
namespace on1x {
// A syntax-rules-style, typed AST pass. No transformer executes runtime On1x.
void expand_macros(Unit& unit);
}

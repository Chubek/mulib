#include <on1x/primitives.hpp>
#include <map>
namespace on1x {
std::optional<Type> data_primitive(const std::string& name){
 static const std::map<std::string,Type> primitives={
  {"sx_nil",Type::function({},Type::SExpr)},
  {"sx_symbol",Type::function({Type::String},Type::SExpr)},
  {"sx_string",Type::function({Type::String},Type::SExpr)},
  {"sx_cons",Type::function({Type::SExpr,Type::SExpr},Type::SExpr)},
  {"sx_head",Type::function({Type::SExpr},Type::SExpr)},
  {"sx_tail",Type::function({Type::SExpr},Type::SExpr)},
  {"sx_kind",Type::function({Type::SExpr},Type::I64)},
  {"sx_text",Type::function({Type::SExpr},Type::String)},
  {"data_live_nodes",Type::function({},Type::I64)},
  {"text_len",Type::function({Type::String},Type::I64)},
  {"text_byte",Type::function({Type::String,Type::I64},Type::I64)},
  {"text_slice",Type::function({Type::String,Type::I64,Type::I64},Type::String)},
  {"text_concat",Type::function({Type::String,Type::String},Type::String)},
  {"buffer_new",Type::function({},Type::Buffer)},
  {"buffer_append",Type::function({Type::Buffer,Type::String},Type::Void)},
  {"buffer_byte",Type::function({Type::Buffer,Type::I64},Type::Void)},
  {"buffer_text",Type::function({Type::Buffer},Type::String)},
  {"buffer_len",Type::function({Type::Buffer},Type::I64)},
  {"buffer_clear",Type::function({Type::Buffer},Type::Void)},
  {"io_stdin",Type::function({},Type::String)},
  {"io_stdout",Type::function({Type::String},Type::Void)},
  {"io_stderr",Type::function({Type::String},Type::Void)},
  {"tool_env",Type::function({Type::String},Type::String)},
  {"tool_read_file",Type::function({Type::String},Type::String)},
  {"tool_fail",Type::function({Type::String},Type::Void)}
 };
 auto found=primitives.find(name);return found==primitives.end()?std::nullopt:std::optional<Type>{found->second};
}
}

#include <on1x/primitives.hpp>
#include <map>
namespace on1x {
std::optional<Type> numeric_primitive(const std::string& name){
 static const auto primitives=[]{
  std::map<std::string,Type> out;
  auto add=[&](std::string name,std::vector<Type> args,Type result){out.emplace(std::move(name),Type::function(std::move(args),result));};
  for(auto kind:{Type::F64,Type::F32}){
   Type t(kind);auto n=type_name(t);
   add(n+"_from_i64",{Type::I64},t);add(n+"_to_i64",{t},Type::I64);
   add(n+"_from_bits",{Type::I64},t);add(n+"_to_bits",{t},Type::I64);
   for(auto op:{"is_nan","is_finite","signbit"})add(n+"_"+op,{t},Type::Bool);
  }
  add("f32_from_f64",{Type::F64},Type::F32);add("f64_from_f32",{Type::F32},Type::F64);
  for(auto kind:{Type::U8,Type::U16,Type::U32,Type::U64}){
   Type t(kind);auto n=type_name(t);
   add(n+"_from_i64",{Type::I64},t);add(n+"_to_i64",{t},Type::I64);
   add(n+"_from_bits",{Type::I64},t);add(n+"_to_bits",{t},Type::I64);
   for(auto op:{"wrapping_add","wrapping_sub","wrapping_mul","and","or","xor"})add(n+"_"+op,{t,t},t);
   add(n+"_not",{t},t);add(n+"_shl",{t,Type::I64},t);add(n+"_shr",{t,Type::I64},t);
   for(auto source:{Type::U8,Type::U16,Type::U32,Type::U64})add(n+"_from_"+type_name(source),{source},t);
   for(auto f:{Type::F32,Type::F64}){
    add(n+"_from_"+type_name(f),{f},t);add(type_name(f)+"_from_"+n,{t},f);
   }
  }
  return out;
 }();
 auto found=primitives.find(name);return found==primitives.end()?std::nullopt:std::optional<Type>{found->second};
}
std::optional<Type> data_primitive(const std::string& name){
  if(auto numeric=numeric_primitive(name))return numeric;
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

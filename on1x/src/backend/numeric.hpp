#pragma once
#include <on1x/primitives.hpp>
#include <optional>
#include <string_view>

namespace on1x::mir::detail {
// One descriptor shared by v3 lowering, verification and emission. Public
// numeric intrinsics retain the canonical frontend signatures. Operator names
// are internal MIR descriptors, not additional reserved source functions.
inline std::optional<Type> numeric_signature(const std::string& name){
 if(auto signature=numeric_primitive(name))return signature;
 for(auto kind:{Type::F32,Type::U8,Type::U16,Type::U32,Type::U64}){
  Type t(kind);auto suffix="_"+type_name(t);
  for(auto op:{"add","sub","mul","div","mod","eq","ne","lt","le","gt","ge","neg"}){
   if(name!=std::string(op)+suffix)continue;
   std::string_view operation(op);
   if(operation=="neg")return t==Type::F32?std::optional<Type>{Type::function({t},t)}:std::nullopt;
   if(operation=="mod"&&t.is_float())return {};
   bool compare=operation=="eq"||operation=="ne"||operation=="lt"||operation=="le"||operation=="gt"||operation=="ge";
   return Type::function({t,t},compare?Type{Type::Bool}:t);
  }
 }
 return {};
}
inline bool unsigned_comparison(const std::string& name){
 auto at=name.find('_');if(at==std::string::npos)return false;
 auto op=name.substr(0,at),t=name.substr(at+1);
 return (t=="u8"||t=="u16"||t=="u32"||t=="u64")&&
  (op=="eq"||op=="ne"||op=="lt"||op=="le"||op=="gt"||op=="ge");
}
inline unsigned unsigned_width(const Type& t){
 return t==Type::U8?8:t==Type::U16?16:t==Type::U32?32:64;
}
inline std::uint64_t unsigned_max(const Type& t){
 return t==Type::U64?UINT64_MAX:(UINT64_C(1)<<unsigned_width(t))-1;
}
}

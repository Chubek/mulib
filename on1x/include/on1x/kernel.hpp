#pragma once
#include <on1x/ast.hpp>
#include <on1x/nominal.hpp>

namespace on1x {
inline constexpr std::string_view legacy_scalar_module="mulib.clamp";
inline bool clamp_interval_symbol(std::string_view symbol) {
 return symbol=="mulib_clamp_i64";
}
inline std::size_t clamp_kernel_arity(std::string_view symbol) {
 if(clamp_interval_symbol(symbol))return 3;
 if(symbol=="mulib_clamp_add_i64"||symbol=="mulib_clamp_sub_i64")return 2;
 return 0;
}
inline bool history_kernel_symbol(std::string_view symbol) {
 return symbol=="mulib_histprice_ema"||symbol=="mulib_histprice_holt";
}
inline bool pricing_kernel_symbol(std::string_view symbol) {
 return symbol=="mulib_priceopt_optimize"||symbol=="mulib_priceopt_candidates";
}
inline constexpr char ipc_kernel_open[]="on1x_kernel_ipc_v1_open_pair";
inline constexpr char ipc_kernel_roundtrip[]="on1x_kernel_ipc_v1_roundtrip";
inline constexpr char ipc_kernel_open_session[]="on1x_kernel_ipc_v1_open_session";
inline constexpr char ipc_kernel_register[]="on1x_kernel_ipc_v1_register_callback";
inline constexpr char ipc_kernel_revoke[]="on1x_kernel_ipc_v1_revoke";
inline constexpr char ipc_kernel_send[]="on1x_kernel_ipc_v1_send";
inline constexpr char ipc_kernel_poll[]="on1x_kernel_ipc_v1_poll";
inline constexpr char ipc_kernel_finish[]="on1x_kernel_ipc_v1_finish_send";
// Match exact KernelBind profiles, never synthesize a C symbol from source text.
struct LegacyKernelMember { std::string_view module, member, symbol; };
inline constexpr LegacyKernelMember legacy_kernel_members[]={
 {"mulib.clamp","i64","mulib_clamp_i64"},
 {"mulib.clamp","add_i64","mulib_clamp_add_i64"},
 {"mulib.clamp","sub_i64","mulib_clamp_sub_i64"},
 {"mulib.histprice","ema","mulib_histprice_ema"},
 {"mulib.histprice","holt","mulib_histprice_holt"},
 {"mulib.priceopt","optimize","mulib_priceopt_optimize"},
 {"mulib.priceopt","candidates","mulib_priceopt_candidates"},
 {"mulib.ipc_framed","open_pair",ipc_kernel_open},
 {"mulib.ipc_framed","roundtrip",ipc_kernel_roundtrip},
 {"mulib.ipc_framed","open_session",ipc_kernel_open_session},
 {"mulib.ipc_framed","register_callback",ipc_kernel_register},
 {"mulib.ipc_framed","revoke",ipc_kernel_revoke},
 {"mulib.ipc_framed","send",ipc_kernel_send},
 {"mulib.ipc_framed","poll",ipc_kernel_poll},
 {"mulib.ipc_framed","finish_send",ipc_kernel_finish},
};
inline std::string_view legacy_kernel_member(std::string_view module,std::string_view member){
 for(const auto& entry:legacy_kernel_members)
  if(entry.module==module&&entry.member==member)return entry.symbol;
 return {};
}
// These identities cannot be spelled by the source parser. The explicit
// import alias can name Result and the session's opaque nominal identity.
inline constexpr char ipc_kernel_payload[]="@kernel_ipc_v1_Pair";
inline constexpr char ipc_kernel_session[]="@kernel_ipc_v1_Session";
inline constexpr char ipc_kernel_result_name[]="@kernel_ipc_v1_Result";
inline bool ipc_kernel_session_loan(std::string_view symbol) {
 return symbol==ipc_kernel_register||symbol==ipc_kernel_revoke||symbol==ipc_kernel_send||
        symbol==ipc_kernel_poll||symbol==ipc_kernel_finish;
}
inline bool ipc_kernel_symbol(std::string_view symbol) {
 return symbol==ipc_kernel_open||symbol==ipc_kernel_roundtrip||symbol==ipc_kernel_open_session||
        ipc_kernel_session_loan(symbol);
}
inline bool ipc_kernel_opening(std::string_view symbol) {
 return symbol==ipc_kernel_open||symbol==ipc_kernel_open_session;
}
inline bool ipc_kernel_opaque_nominal(std::string_view nominal) {
 return nominal==ipc_kernel_payload||nominal==ipc_kernel_session;
}
inline Type ipc_kernel_owner() {
 return Type::own(Type::named(ipc_kernel_payload));
}
inline Type ipc_kernel_session_owner() {
 return Type::own(Type::named(ipc_kernel_session));
}
inline bool ipc_kernel_opaque_owner(const Type& type) {
 return type.is_own()&&ipc_kernel_opaque_nominal(type.owned().nominal());
}
inline Type ipc_kernel_result(std::string_view symbol) {
 return Type::named(ipc_kernel_result_name,
                    {symbol==ipc_kernel_open?ipc_kernel_owner():
                     symbol==ipc_kernel_open_session?ipc_kernel_session_owner():Type{Type::I64},
                     Type::I64});
}
inline std::vector<Type> ipc_kernel_parameters(std::string_view symbol) {
 if(ipc_kernel_opening(symbol))return {};
 const auto callback=Type::function({Type::I64},Type::I64);
 if(symbol==ipc_kernel_roundtrip)return {ipc_kernel_owner(),Type::U8,Type::I64,callback};
 if(symbol==ipc_kernel_register)return {ipc_kernel_session_owner(),callback};
 if(symbol==ipc_kernel_send)return {ipc_kernel_session_owner(),Type::U8,Type::I64};
 if(symbol==ipc_kernel_poll)return {ipc_kernel_session_owner(),Type::I64};
 if(ipc_kernel_session_loan(symbol))return {ipc_kernel_session_owner()};
 return {};
}
inline void append_ipc_kernel_nominals(Unit& unit,const Location& at) {
 // The opaque payloads are never materialized as C values in generated code.
 unit.nominals.push_back({at,ipc_kernel_payload,Type::Record,{},{},{}});
 unit.nominals.push_back({at,ipc_kernel_session,Type::Record,{},{},{}});
 unit.nominals.push_back({at,ipc_kernel_result_name,Type::Sum,{"T","E"},
                         {"Ok","Err"},{Type::bound(0),Type::bound(1)}});
}
[[noreturn]] inline void ipc_kernel_opaque_error(const Location& at) {
 throw Error(at,"ON1X-KERNEL-OPAQUE: framed transport payloads cannot be materialized; sessions allow only direct operation loans",
             "ON1X-KERNEL-OPAQUE");
}
inline bool kernel_span_argument(std::string_view symbol,std::size_t i) {
 return (history_kernel_symbol(symbol)&&i==0)||(symbol=="mulib_priceopt_candidates"&&i<2);
}
inline bool kernel_span_type(const Type& type) {
 return type.kind()==Type::Slice&&type.elements()==std::vector<Type>{Type::F64}&&type.extent()<=256;
}
struct KernelArgumentPolicy {
 enum class Mode { Value, BoundedSlice, ConsumeOwner, ExclusiveOwner } mode=Mode::Value;
 std::size_t max_extent=0;
};
inline KernelArgumentPolicy kernel_argument_policy(const Function& f,std::size_t index){
 if(index>=f.params.size()||f.kernel_symbol.empty())return {};
 if(index==0&&f.kernel_symbol==ipc_kernel_roundtrip)return {KernelArgumentPolicy::Mode::ConsumeOwner};
 if(index==0&&ipc_kernel_session_loan(f.kernel_symbol))return {KernelArgumentPolicy::Mode::ExclusiveOwner};
 if(kernel_span_argument(f.kernel_symbol,index))return {KernelArgumentPolicy::Mode::BoundedSlice,256};
 return {};
}
inline bool kernel_argument_type(KernelArgumentPolicy policy,const Type& expected,const Type& actual){
 if(policy.mode==KernelArgumentPolicy::Mode::BoundedSlice)
  return actual.kind()==Type::Slice&&expected.kind()==Type::Slice&&
         actual.elements()==expected.elements()&&actual.extent()<=policy.max_extent;
 if(policy.mode==KernelArgumentPolicy::Mode::ConsumeOwner||policy.mode==KernelArgumentPolicy::Mode::ExclusiveOwner)
  return expected.is_own()&&actual==expected;
 return actual==expected;
}
inline std::vector<Type> pricing_kernel_parameters(std::string_view symbol,const std::string& prefix) {
 std::vector<Type> types;
 if(symbol=="mulib_priceopt_candidates")types.assign(2,Type::data(Type::Slice,{Type::F64}));
 else types.push_back(Type::named(prefix+"DemandModel"));
 types.push_back(Type::named(prefix+"Constraints"));types.push_back(Type::named(prefix+"Objective"));
 return types;
}
inline void append_kernel_nominals(Unit& unit,const std::string& prefix,bool history,const Location& at) {
 const auto empty=Type::data(Type::Record,{});
 auto append=[&](std::string member,Type::Kind kind,std::vector<std::string> labels,std::vector<Type> fields,
                 std::vector<std::string> params={}){
  unit.nominals.push_back({at,prefix+member,kind,std::move(params),std::move(labels),std::move(fields)});
 };
 append("Result",Type::Sum,{"Ok","Err"},{Type::bound(0),Type::bound(1)},{"T","E"});
 append("Error",Type::Sum,{"Rejected"},{empty});
 if(history){
  append("Forecast",Type::Record,{"point","variance"},{Type::F64,Type::F64});
 }else{
  append("DemandKind",Type::Sum,{"Linear","Isoelastic","Logistic"},{empty,empty,empty});
  append("Objective",Type::Sum,{"Profit","Revenue"},{empty,empty});
  append("DemandModel",Type::Record,{"kind","scale","shape","reference"},
         {Type::named(prefix+"DemandKind"),Type::F64,Type::F64,Type::F64});
  append("Constraints",Type::Record,{"min_price","max_price","unit_cost","min_unit_margin",
         "previous_price","max_relative_change","tick"},std::vector<Type>(7,Type::F64));
  append("Decision",Type::Record,{"price","score","expected_demand","evaluations"},
         {Type::F64,Type::F64,Type::F64,Type::U64});
 }
}
inline std::vector<Type> history_kernel_parameters(std::string_view symbol) {
 std::vector<Type> params{Type::data(Type::Slice,{Type::F64}),Type::F64};
 if(symbol=="mulib_histprice_holt")params.insert(params.end(),{Type::F64,Type::F64});
 params.push_back(Type::I64);
 return params;
}
// Verify the compiler-owned scalar projection at checked HIR and C11 ingress.
// This is representability checking, not ExoML provenance or semantic approval.
inline void verify_kernel_shape(const Function& f) {
 if(f.kernel_symbol.empty()&&!f.kernel_link_only)return;
 const auto expected=(f.kernel_link_only?"@kernel_scalar_":"@kernel_")+f.kernel_symbol;
 if(ipc_kernel_symbol(f.kernel_symbol)){
  Unit declarations;
  append_ipc_kernel_nominals(declarations,f.where);
  NominalCatalog catalog(declarations);
  const auto params=ipc_kernel_parameters(f.kernel_symbol);
  if(!f.kernel_link_only&&f.name==expected&&f.external&&!f.prototype&&!f.extension_generated&&
     f.type_parameters.empty()&&f.constraints.empty()&&f.body.empty()&&f.attributes.empty()&&
     f.result==catalog.resolve(ipc_kernel_result(f.kernel_symbol),f.where)&&f.params.size()==params.size()){
   for(std::size_t i=0;i<params.size();++i)
    if(f.params[i].pattern||f.params[i].borrow!=BorrowMode::Value||
       f.params[i].type!=catalog.resolve(params[i],f.where))
     throw Error(f.params[i].where,"ON1X-KERNEL-ABI: invalid framed transport parameter","ON1X-KERNEL-ABI");
   return;
  }
  throw Error(f.where,"ON1X-KERNEL-ABI: invalid internal framed transport interface","ON1X-KERNEL-ABI");
 }
 if(pricing_kernel_symbol(f.kernel_symbol)){
  const auto& result=f.result;
  if(f.kernel_link_only&&f.name==expected&&f.external&&!f.prototype&&f.body.empty()&&f.attributes.empty()&&
     result.nominal().ends_with("kernel_pricing_Result")){
   auto prefix=result.nominal().substr(0,result.nominal().size()-6);
   auto params=pricing_kernel_parameters(f.kernel_symbol,prefix);
   Unit declarations;
   append_kernel_nominals(declarations,prefix,false,f.where);
   NominalCatalog catalog(declarations);
   auto name=[&](const char* member){return Type::named(prefix+member);};
   auto want=catalog.resolve(Type::named(prefix+"Result",{name("Decision"),name("Error")}),f.where);
   if(f.result==want&&f.params.size()==params.size()){
    for(std::size_t i=0;i<params.size();++i)
     if(f.params[i].pattern||f.params[i].type!=catalog.resolve(params[i],f.where))
      throw Error(f.params[i].where,"ON1X-KERNEL-ABI: invalid pricing parameter","ON1X-KERNEL-ABI");
    return;
   }
  }
  throw Error(f.where,"ON1X-KERNEL-ABI: invalid internal pricing interface","ON1X-KERNEL-ABI");
 }
 if(history_kernel_symbol(f.kernel_symbol)){
  const auto params=history_kernel_parameters(f.kernel_symbol);
  const auto forecast=Type::data(Type::Record,{Type::F64,Type::F64},{"point","variance"});
  const auto error=Type::data(Type::Sum,{Type::data(Type::Record,{})},{"Rejected"});
  const auto& result=f.result;
  if(f.kernel_link_only&&f.name==expected&&f.external&&!f.prototype&&f.body.empty()&&f.attributes.empty()&&
     f.params.size()==params.size()&&result.kind()==Type::Sum&&result.labels()==std::vector<std::string>{"Err","Ok"}&&
     result.elements().size()==2&&result.nominal().ends_with("kernel_history_Result")&&
     result.arguments()==std::vector<Type>{result.elements()[1],result.elements()[0]}&&
     result.elements()[1].nominal().ends_with("kernel_history_Forecast")&&
     result.elements()[0].nominal().ends_with("kernel_history_Error")&&
     result.elements()[1]==forecast.with_nominal(result.elements()[1].nominal(),{})&&
     result.elements()[0]==error.with_nominal(result.elements()[0].nominal(),{})){
   for(std::size_t i=0;i<params.size();++i)
    if(f.params[i].type!=params[i]||f.params[i].pattern)
     throw Error(f.params[i].where,"ON1X-KERNEL-ABI: invalid history parameter","ON1X-KERNEL-ABI");
   return;
  }
  throw Error(f.where,"ON1X-KERNEL-ABI: invalid internal history interface","ON1X-KERNEL-ABI");
 }
 const auto arity=clamp_kernel_arity(f.kernel_symbol);
 if(!arity||
     f.name!=expected||!f.external||f.prototype||f.extension_generated||
     !f.type_parameters.empty()||!f.constraints.empty()||!f.body.empty()||!f.attributes.empty()||
     f.result!=Type::I64||f.params.size()!=arity)
  throw Error(f.where,"ON1X-KERNEL-ABI: invalid internal scalar interface","ON1X-KERNEL-ABI");
 for(const auto& p:f.params)
  if(p.type!=Type::I64||p.pattern||p.borrow!=BorrowMode::Value)
   throw Error(p.where,"ON1X-KERNEL-ABI: invalid internal scalar parameter","ON1X-KERNEL-ABI");
}
}

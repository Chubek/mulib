#pragma once
#include <on1x/ast.hpp>
#include <on1x/nominal.hpp>

namespace on1x {
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
inline bool kernel_span_argument(std::string_view symbol,std::size_t i) {
 return (history_kernel_symbol(symbol)&&i==0)||(symbol=="mulib_priceopt_candidates"&&i<2);
}
inline constexpr std::string_view kernel_reviewed_prefix="@kernel_scalar_";
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
     f.name!=expected||!f.external||f.prototype||!f.body.empty()||!f.attributes.empty()||
     f.result!=Type::I64||f.params.size()!=arity)
  throw Error(f.where,"ON1X-KERNEL-ABI: invalid internal scalar interface","ON1X-KERNEL-ABI");
 for(const auto& p:f.params)
  if(p.type!=Type::I64||p.pattern)
   throw Error(p.where,"ON1X-KERNEL-ABI: invalid internal scalar parameter","ON1X-KERNEL-ABI");
}
}

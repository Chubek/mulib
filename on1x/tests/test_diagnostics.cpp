#include <on1x/diagnostics.hpp>
#include <cassert>
#include <iostream>

using namespace moonstone;
using namespace moonstone::syren;
int main(){
 const std::string source="fn main()->i64{return 42}";
 std::optional<on1x::Error> error;
 try{on1x::parse(on1x::scan(source,"test.on1x"));}catch(const on1x::Error& e){error=e;}
 assert(error&&error->diagnostic_id=="ON1X-PARSE-EXPECTED"&&error->expected==";");
 auto report=on1x::diagnostics::describe(*error,source);
 assert(report.repairs.size()==2&&report.repairs.front().edits.front().begin==source.find('}'));
 std::size_t calls=0;
 on1x::diagnostics::validate(report,[&](std::string_view changed){
  ++calls;
  try{auto unit=on1x::parse(on1x::scan(std::string(changed),"test.on1x"));on1x::check(unit);return ProofResult{Status::Proven,"checked",{},1};}
  catch(const on1x::Error&){return ProofResult{Status::Refuted,"still invalid",{},1};}
 });
 assert(calls==2&&report.repairs[0].status==RepairStatus::Validated&&report.repairs[1].status==RepairStatus::Rejected);
 assert(apply_edits(source,report.repairs[0].edits)=="fn main()->i64{return 42;}");
 for(auto status:{Status::Unknown,Status::BudgetExhausted,Status::InvalidInput,Status::Refuted}){
  auto test=on1x::diagnostics::describe(*error,source);
  on1x::diagnostics::validate(test,[&](std::string_view){return ProofResult{status,"not proved",{},0};});
  assert(test.repairs[0].status==((status==Status::Unknown||status==Status::BudgetExhausted)?RepairStatus::Inconclusive:RepairStatus::Rejected));
 }
 auto bad=on1x::diagnostics::describe(*error,source);
 bad.repairs[0].edits={{0,0,"a"},{0,0,"b"}};
 bad.repairs[1].edits={{0,source.size()+1,""}};
 calls=0;
 on1x::diagnostics::validate(bad,[&](std::string_view){++calls;return ProofResult{};});
 assert(calls==0&&bad.repairs[0].status==RepairStatus::Rejected&&bad.repairs[1].status==RepairStatus::Rejected);
 auto negative_cost=on1x::diagnostics::describe(*error,source);
 for(auto& candidate:negative_cost.repairs)candidate.cost=-1;
 on1x::diagnostics::validate(negative_cost,[&](std::string_view){++calls;return ProofResult{};});
 assert(calls==0);
 for(const auto& candidate:negative_cost.repairs){
  assert(candidate.status==RepairStatus::Rejected);
  assert(candidate.reason==negative_cost.presentation.invalid_edits);
 }
 auto throws=on1x::diagnostics::describe(*error,source);
 on1x::diagnostics::validate(throws,[](std::string_view)->ProofResult{throw std::runtime_error("validator failed");});
 assert(throws.repairs[0].status==RepairStatus::Inconclusive);
 auto absent=on1x::diagnostics::describe(*error,source);
 on1x::diagnostics::validate(absent,{});
 assert(absent.repairs[0].status==RepairStatus::Inconclusive);
 assert(on1x::diagnostics::describe(*error,"changed").repairs.empty());
 assert(on1x::diagnostics::describe(*error).repairs.empty());
 auto stale=*error;stale.where.offset=source.size()+1;
 assert(on1x::diagnostics::describe(stale,source).repairs.empty());
 // UTF-16 columns differ from bytes and scalar counts for non-BMP text.
 std::string unicode="fn main()->i64{let s=\"\xf0\x9f\x98\x80\";return 1}";
 try{on1x::parse(on1x::scan(unicode,"a #%.on1x"));assert(false);}catch(const on1x::Error& e){
  auto r=on1x::diagnostics::describe(e,unicode);
  auto sarif=on1x::diagnostics::sarif(r);
  assert(sarif.find("a%20%23%25.on1x")!=std::string::npos);
  assert(sarif.find("\"startColumn\":"+std::to_string(e.where.column-2))!=std::string::npos);
 }
   auto unknown=on1x::diagnostics::describe(on1x::Error({"generated"},"unknown diagnostic","ON1X-CUSTOM"));
  assert(unknown.diagnostic.id==on1x::diagnostic::Catalog::embedded().role(
   on1x::diagnostic::Role::SourceFallback).id&&unknown.diagnostic.message=="unknown diagnostic");
 assert(on1x::diagnostics::sarif(unknown).find("startColumn")==std::string::npos);
 // No invalid UTF-8 is emitted in machine-readable diagnostics.
  auto invalid=on1x::diagnostics::describe(on1x::Error({"x"},std::string("bad ")+char(0xff)));
  assert(on1x::diagnostics::json(invalid).find("\\u00ff")!=std::string::npos);
  auto hostile=on1x::diagnostics::describe(on1x::Error(
   {"bad\n"+std::string(1,'\x1b')+"file"},std::string("message\n")+char(0x1b)+"[31m"));
  auto rendered=on1x::diagnostics::text(hostile);
  assert(rendered.find("bad\\n\\x1bfile")!=std::string::npos);
  assert(rendered.find("message\\n\\x1b[31m")!=std::string::npos);
  assert(rendered.find('\x1b')==std::string::npos);
  // Repeated diagnostics cannot retain an unbounded number of source copies.
   on1x::diagnostic::Registry registry;
   for(std::size_t i=0;i<10;++i){
    auto item=on1x::diagnostic::Catalog::embedded().record(
     on1x::diagnostic::Catalog::embedded().role(on1x::diagnostic::Role::SourceFallback),
     {"snapshot.on1x",0,1,1},{.message="item "+std::to_string(i)});
    assert(registry.emit(std::move(item)));
  }
  const std::string large(on1x::max_source_bytes,' ');
  std::size_t lookups=0;
   auto bounded=on1x::diagnostics::describe(registry,on1x::diagnostic::Catalog::embedded(),[&](const on1x::Location&){
   ++lookups;
   return std::optional<std::string>(large);
  });
  std::size_t retained=0;
  for(const auto& item:bounded.diagnostics)if(item.source)retained+=item.source->size();
  assert(retained==on1x::diagnostic::max_source_snapshot_bytes);
  assert(lookups==on1x::diagnostic::max_source_snapshot_bytes/on1x::max_source_bytes);
  // Deterministic malformed-input exercise; run this under ASan/UBSan too.
 std::uint32_t state=1;
 for(std::size_t iteration=0;iteration<1000;++iteration){
  std::string input="fn main(){";
  for(std::size_t i=0;i<iteration%64;++i){state=state*1664525+1013904223;input+=char(state>>24);}
  input+='}';
  try{on1x::parse(on1x::scan(input,"fuzz.on1x"));}catch(const on1x::Error& e){
   auto r=on1x::diagnostics::describe(e,input);
   assert(!on1x::diagnostics::json(r).empty()&&!on1x::diagnostics::sarif(r).empty());
   for(const auto& c:r.repairs)assert(apply_edits(input,c.edits));
  }
 }
 std::cout<<"diagnostics API OK\n";
}

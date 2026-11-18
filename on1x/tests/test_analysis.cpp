#include <on1x/analysis.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
struct Result {
 std::vector<on1x::diagnostic::Record> records;
 on1x::analysis::Statistics statistics;
 bool errors=false;
};

Result analyze(const std::string& source,on1x::analysis::Options options={}){
 auto unit=on1x::parse(on1x::scan(source,"analysis.on1x"));
 on1x::diagnostic::Registry registry;
  on1x::analysis::Context context{on1x::diagnostic::Catalog::embedded(),registry,std::move(options),{}};
 on1x::check(unit,context);
 return {registry.records(),context.statistics,registry.has_errors()};
}
}

int main(int argc,char** argv){
 assert(argc==2);
 std::ifstream input(argv[1],std::ios::binary);
 std::string catalog_text((std::istreambuf_iterator<char>(input)),{});
 assert(!input.bad()&&!catalog_text.empty());
 std::string hostile="message\n";
 hostile+='\x1b';hostile+="[31mspoof";
 hostile+=std::string("\xc2\x9b")+"31m";
 hostile+=std::string("\xe2\x80\xae")+"txt";
 hostile+=char(0xff);
 auto displayed=on1x::diagnostic::terminal_text(hostile);
 assert(displayed=="message\\n\\x1b[31mspoof\\u{009b}31m\\u{202e}txt\\xff");
 assert(displayed.find('\n')==std::string::npos&&displayed.find('\x1b')==std::string::npos);

 auto catalog=on1x::diagnostic::Catalog::parse(catalog_text,"test-catalog");
 // Lookup indexes must remain owned after copies and moves, not point into a
 // destroyed source catalog.
 auto copied=[&]{
  auto temporary=on1x::diagnostic::Catalog::parse(catalog_text,"temporary");
  auto duplicate=temporary;
  return duplicate;
 }();
 assert(copied.role(on1x::diagnostic::Role::CatalogFailure).id==
        catalog.role(on1x::diagnostic::Role::CatalogFailure).id);
 assert(copied.compiler("ON1X-PARSE-EXPECTED").id==
        catalog.compiler("ON1X-PARSE-EXPECTED").id);
 assert(catalog.size()>4);
 assert(catalog.scrutiny("off").rank==0);
 const auto* division=catalog.wetstone("WET-DIVZERO",on1x::diagnostic::UpstreamLevel::Error);
 assert(division&&catalog.level(catalog.scrutiny("strict"),division->level).failure);
 for(const auto& malformed:{
      catalog_text+" trailing",
      std::string(R"({"schema":"on1x.diagnostic-catalog/v2"})"),
      std::string(catalog_text).replace(catalog_text.find("\"rank\": 1"),9,"\"rank\": 0")}){
  bool rejected=false;
  try{(void)on1x::diagnostic::Catalog::parse(malformed,"bad-catalog");}
  catch(const on1x::Error&){rejected=true;}
  assert(rejected);
 }

 on1x::diagnostic::Registry bounded(1,4096);
 auto first=catalog.record(catalog.role(on1x::diagnostic::Role::SourceFallback),
                           {"test.on1x",0,1,1},{.message="first"});
 assert(bounded.emit(first));
 assert(!bounded.emit(first)&&bounded.dropped()==0);
 auto second=first;second.message="second";
 assert(!bounded.emit(second)&&bounded.dropped()==1&&bounded.exhausted());
 auto invalid_level=first;invalid_level.level.color=108;
 bool invalid_rejected=false;
 try{on1x::diagnostic::Registry checked;(void)checked.emit(invalid_level);}
 catch(const on1x::Error& failure){
  invalid_rejected=failure.diagnostic_id==catalog.role(on1x::diagnostic::Role::ProtocolFailure).id;
 }
 assert(invalid_rejected);

 const std::string divide="fn main()->i64{return 42/0;}";
 auto standard=analyze(divide);
 assert(standard.statistics.wetstone_available==on1x::analysis::wetstone_available());
 if(on1x::analysis::wetstone_available()){
   assert(standard.records.size()==1);
   assert(standard.records[0].id=="ON1X-ANALYSIS-DIVZERO");
   assert(standard.records[0].level.kind==on1x::diagnostic::Severity::Warning);
  assert(!standard.errors);
  assert(standard.statistics.analyzed_functions==1);
  assert(standard.statistics.work>0);
  assert(standard.records[0].notes.size()>1);

  on1x::analysis::Options strict;strict.command_scrutiny="strict";
   auto escalated=analyze(divide,strict);
   assert(escalated.records.size()==1&&escalated.errors);
   assert(escalated.records[0].level.kind==on1x::diagnostic::Severity::Error);

  on1x::analysis::Options disabled;disabled.command_scrutiny="off";
  assert(analyze(divide,disabled).records.empty());

  on1x::analysis::Options project_off;project_off.project_scrutiny="off";
  auto source_strict=analyze("@scrutiny strict;"+divide,project_off);
  assert(source_strict.records.size()==1&&source_strict.errors);

  on1x::analysis::Options command_off;command_off.command_scrutiny="off";
  command_off.project_scrutiny="strict";
  assert(analyze("@scrutiny strict;"+divide,command_off).records.empty());

   on1x::analysis::Options no_repairs;no_repairs.repairs=false;
   auto advisory=analyze(divide,no_repairs);
   assert(advisory.records.size()==1&&advisory.records[0].notes.size()==1);

   // Checked operations trap before later expressions execute. The Wetstone
   // prefix must stop instead of turning an overflowing or dynamic value into
   // a synthetic zero that could produce a false division-by-zero finding.
   for(const auto& source:{
        "fn main()->i64{let x=9223372036854775807+1;return 42/x;}",
        "fn main()->i64{let x=(0-9223372036854775807)-2;return 42/x;}",
        "fn main()->i64{let x=3037000500*3037000500;return 42/x;}",
        "fn main()->i64{let x=(0-9223372036854775807)-1;return 42/(0-x);}",
        "fn main()->i64{return ((0-9223372036854775807)-1)/(0-1);}",
        "fn checked(x:i64)->i64{let y=x+1;return 42/y;}fn main()->i64{return checked(1);}"
       }){
    auto stopped=analyze(source);
    assert(stopped.records.empty());
    assert(!stopped.statistics.complete);
    assert(stopped.statistics.skipped_functions>0);
   }
  }else{
  assert(standard.records.empty()&&!standard.statistics.complete);
 }

 for(auto options:{
      on1x::analysis::Options{.command_scrutiny="unknown",.project_scrutiny=std::nullopt,.work=100000,.repairs=true},
      on1x::analysis::Options{.command_scrutiny=std::nullopt,.project_scrutiny=std::nullopt,.work=0,.repairs=true}
     }){
  bool rejected=false;
  try{(void)analyze(divide,std::move(options));}
  catch(const on1x::Error&){rejected=true;}
  assert(rejected);
 }
 std::cout<<"analysis and catalog API OK\n";
}

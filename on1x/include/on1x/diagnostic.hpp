#pragma once
#include <on1x/ast.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace on1x::diagnostic {
inline constexpr std::size_t max_source_snapshot_bytes=16*1024*1024;

// Numeric catalog kinds select closed implementation mechanics. Their names,
// labels, colors, failure behavior and mappings remain catalog data.
enum class Severity : unsigned char { Note=0,Warning=1,Error=2 };
enum class ProviderKind : unsigned char { Compiler=0,Wetstone=1,Ppx=2 };
enum class RepairKind : unsigned char { None=0,Token=1,PrimitiveType=2,Advisory=3 };
enum class AnalysisAdapter : unsigned char { DivisionSafety=0,AssertionSafety=1,OwnershipSafety=2 };
enum class UpstreamLevel : unsigned char { Warning=0,Error=1 };
enum class Role : unsigned char { None=0,SourceFallback=1,CatalogFailure=2,ScrutinyFailure=3,ProtocolFailure=4,ReportingFailure=5 };
enum class CommandFailure : unsigned char {
 FormatMissing,FormatUnknown,StructuredUnavailable,ColorMissing,ColorDuplicate,ColorUnknown,
 CatalogArgument,ScrutinyArgument,OptionDuplicate,WorkMissing,RepairPolicy,RepairOutput,
 RepairExists,RepairCreate,RepairWrite,RepairOptions,RepairUnavailable,Count
};

// Render untrusted text on one terminal line. Printable UTF-8 is preserved;
// terminal controls, line breaks, bidi controls and malformed bytes are escaped.
std::string terminal_text(std::string_view);

struct TemplateArguments {
 std::string_view message={},expected={},found={},replacement={},count={};
};
std::string expand(std::string_view,const TemplateArguments& = {});

struct Level {
 std::string name,sarif;
 unsigned color=0;
 Severity kind=Severity::Note;
 bool failure=false;
};

struct Provider {
 std::string name,origin;
 ProviderKind kind=ProviderKind::Compiler;
};

struct RepairPolicy {
 std::string name,candidate_description,notice;
 std::string insert_id,replace_id,end_marker,candidate_id,rationale,proposal;
 std::vector<std::string> replacements;
 std::size_t maximum_distance=0;
 RepairKind kind=RepairKind::None;
};

struct Presentation {
 std::array<std::string,4> repair_status;
 std::array<std::string,static_cast<std::size_t>(CommandFailure::Count)> command_errors;
 std::string diagnostic_label,category_label,note_label,help_label,repair_label,bytes_label,arrow_label;
 std::string dropped,invalid_edits,source_limit,missing_validator,validator_failure;
 std::string compiler_validated,source_ppx_unavailable,ppx_client_failure,sarif_tool;
 std::string already_valid,ambiguous_repairs;
};

struct CatalogEntry {
 std::string id,match,message,category,phase,guidance,remediation;
 std::string source_rule,check,minimum_scrutiny,ppx_processor;
 Role role=Role::None;
 Provider provider;
 Level level;
 RepairPolicy repair;
 UpstreamLevel source_level=UpstreamLevel::Warning;
 AnalysisAdapter adapter=AnalysisAdapter::DivisionSafety;
 bool has_upstream=false;
 std::vector<std::string> repair_tokens;
};

struct ScrutinyPolicy {
 std::string name;
 std::size_t rank=0;
 bool enabled=false;
 std::set<std::string> checks;
 std::map<std::string,std::string> severity_overrides;
};

struct Record {
 std::string id,message,category,phase,origin,check,remediation;
 Level level;
 Location where;
 std::vector<std::string> notes;
};

class Catalog {
 std::map<std::string,Level> levels_;
 std::map<std::string,Provider> providers_;
 std::map<std::string,RepairPolicy> repairs_;
 std::map<std::string,CatalogEntry> entries_;
 std::map<std::string,std::string> compiler_;
 std::map<unsigned,std::string> roles_;
 std::map<std::string,ScrutinyPolicy> scrutiny_;
 Presentation presentation_;
 std::string default_scrutiny_;
 static Catalog parse_trusted(std::string_view,const std::string&);
public:
 static Catalog parse(std::string_view,const std::string& origin);
 static Catalog load(const std::filesystem::path&);
 static const Catalog& embedded();

 const CatalogEntry* find(std::string_view id)const;
 const CatalogEntry& role(Role)const;
 const CatalogEntry& compiler(std::string_view legacy_id)const;
 const CatalogEntry* wetstone(std::string_view rule,UpstreamLevel)const;
 const CatalogEntry* ppx(std::string_view id)const;
 std::optional<std::string> ppx_id(std::string_view processor)const;
 const ScrutinyPolicy& scrutiny(std::string_view name,const Location& at={"<diagnostics>"})const;
 const std::string& default_scrutiny()const noexcept{return default_scrutiny_;}
 const Presentation& presentation()const noexcept{return presentation_;}
 const Level& level(std::string_view name)const;
 Level level(const ScrutinyPolicy&,const Level&)const;
 std::set<AnalysisAdapter> adapters(const ScrutinyPolicy&)const;
 bool advisory_repairs(const ScrutinyPolicy&)const;
 bool selected(const ScrutinyPolicy&,const CatalogEntry&)const;
 std::string render(const CatalogEntry&,const TemplateArguments& = {})const;
 Record record(const CatalogEntry&,Location,const TemplateArguments& = {})const;
 Record record(const Error&)const;
 Error failure(Role,Location)const;
 std::string command_failure(CommandFailure,const TemplateArguments& = {})const;

 std::size_t size()const noexcept{return entries_.size();}
 const std::map<std::string,CatalogEntry>& entries()const noexcept{return entries_;}
};

class Registry {
 std::vector<Record> records_;
 std::set<std::string> keys_;
 std::size_t bytes_=0,dropped_=0;
 std::size_t max_count_,max_bytes_;
public:
 explicit Registry(std::size_t max_count=1024,std::size_t max_bytes=1024*1024);
 bool emit(Record);
 const std::vector<Record>& records()const noexcept{return records_;}
 std::size_t retained_bytes()const noexcept{return bytes_;}
 std::size_t dropped()const noexcept{return dropped_;}
 bool exhausted()const noexcept{return dropped_!=0;}
 bool has_errors()const noexcept;
 bool empty()const noexcept{return records_.empty();}
};

std::string text(const Registry&,const Catalog&,bool color=false);
}

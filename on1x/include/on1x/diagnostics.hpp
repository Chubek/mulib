#pragma once
#include <on1x/ast.hpp>
#include <on1x/diagnostic.hpp>
#include <syren/Syren.hpp>
#include <syren/Repair.hpp>
#include <functional>

namespace on1x::diagnostics {
// Immutable source snapshot supplied by the host. Offsets/columns are bytes.
struct Report {
 moonstone::Diagnostic diagnostic;
 std::vector<moonstone::syren::RepairCandidate> repairs;
 std::optional<std::string> source;
 std::size_t offset=0;
 diagnostic::Level level;
 diagnostic::Presentation presentation;
 std::string category,phase,origin,check,remediation;
};
struct Bundle {
 std::vector<Report> diagnostics;
 std::size_t dropped=0;
 diagnostic::Presentation presentation;
};
using SourceLookup=std::function<std::optional<std::string>(const Location&)>;
Report describe(const Error&,std::optional<std::string> source=std::nullopt,
 const diagnostic::Catalog& catalog=diagnostic::Catalog::embedded());
Report describe(const diagnostic::Record&,std::optional<std::string> source=std::nullopt,
 const diagnostic::Catalog& catalog=diagnostic::Catalog::embedded());
Bundle describe(const diagnostic::Registry&,const diagnostic::Catalog& catalog=diagnostic::Catalog::embedded(),
 const SourceLookup& lookup={});
// Checks source edits before invoking Syren's validator. A Proven result means
// only what the host validator establishes, never equivalence to user intent.
void validate(Report&,const moonstone::syren::Validator&);
std::string text(const Report&);
std::string text(const Bundle&,bool color=false);
std::string json(const Report&);
std::string json(const Bundle&);
std::string sarif(const Report&);
std::string sarif(const Bundle&);
std::string status_name(moonstone::syren::RepairStatus,const diagnostic::Presentation&);
}

#pragma once
#include <on1x/ast.hpp>
#include <syren/Syren.hpp>
#include <syren/Repair.hpp>

namespace on1x::diagnostics {
// Immutable source snapshot supplied by the host. Offsets/columns are bytes.
struct Report {
 moonstone::Diagnostic diagnostic;
 std::vector<moonstone::syren::RepairCandidate> repairs;
 std::optional<std::string> source;
 std::size_t offset=0;
};
Report describe(const Error&,std::optional<std::string> source=std::nullopt);
// Checks source edits before invoking Syren's validator. A Proven result means
// only what the host validator establishes, never equivalence to user intent.
void validate(Report&,const moonstone::syren::Validator&);
std::string text(const Report&);
std::string json(const Report&);
std::string sarif(const Report&);
const char* status_name(moonstone::syren::RepairStatus);
}

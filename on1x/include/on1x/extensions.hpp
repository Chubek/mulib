#pragma once
#include <on1x/ast.hpp>
#include <on1x/meta.hpp>
#include <filesystem>
#include <set>

namespace on1x::extensions {
struct Options {
 std::set<std::string> enabled,disabled;
 std::filesystem::path directory;
 bool execute=true;
 unsigned timeout_ms=5000;
};
std::string canonical(const std::string& name,const Location& at);
void select(std::set<std::string>& into,const std::string& names,const Location& at);
std::string describe(const Options& options,const std::string& name={});
void expand(Unit& unit,const Options& options,const std::string& origin);
std::string digest(const std::string& bytes);
// Pure post-check/backend-ingress verification; no execution or cached proof.
void verify_evidence(const Unit& unit);
// The batch must belong to unit.extension_evidence; unit must be checked.
meta::SExpr evidence_snapshot(const Unit& unit,const ExtensionEvidence& evidence);
void admit_evidence_plan(const Unit& unit,const ExtensionEvidence& evidence,const meta::SExpr& plan);
// Native read-only typed feedback, independently admitted by the host.
std::string evidence_plan(const Unit& unit);
}

#pragma once
#include <on1x/analysis.hpp>
#include <on1x/processor_semantic.hpp>

namespace on1x {
// Revalidate a private, already elaborated whole-program candidate. Does not
// expand macros, infer source types or repeat non-idempotent source lowering.
void recheck_checked_hir(Unit&);
// The report is published only after all source and transformed-HIR checks.
void check(Unit&,analysis::Context&,processor::Report&);
void check(Unit&,analysis::Context&,const processor::RuleSelection&,processor::Report* = nullptr);
// Check a library in place without requiring a main or publishing an executable.
void check_library(Unit&,analysis::Context&,const processor::RuleSelection&);
void check_interfaces(Unit,const std::vector<InterfaceObligation>&,analysis::Context&,
                      const processor::RuleSelection&);
}

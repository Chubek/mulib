#pragma once
#include <on1x/analysis.hpp>
#include <on1x/processor_semantic.hpp>

namespace on1x {
// The report is published only after all source and transformed-HIR checks.
void check(Unit&,analysis::Context&,processor::Report&);
}

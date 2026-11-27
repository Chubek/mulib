#pragma once
#include <on1x/diagnostic.hpp>
#include <on1x/unused_binding.hpp>

namespace on1x {

struct Unit;
struct Location;
struct ModuleSignature;
struct LoadedUnit;

// Check only parser-owned source facts, before import resolution or flattening.
// The canonical C++ loader remains the authority for paths and visibility.
// The bootstrap scanner build has no ASDL runtime; its parser and loader retain
// their ordinary checks.
void check_asdl_module(const Unit&);

// Validate typed projections of already-checked module signatures and physical
// graph observations. Neither function changes admission, type equality, or
// the published graph. Legacy declaration-only interfaces do not use this ABI.
void check_asdl_signature(const ModuleSignature&);
void check_asdl_graph(const LoadedUnit&, const Location&);

// Report-only bridge; no source, PPX or user libraries are executed.
std::vector<diagnostic::Record> collect_asdl_unused(
    const std::vector<UnusedBinding>&, const diagnostic::Level&);

} // namespace on1x

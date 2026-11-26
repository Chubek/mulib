#pragma once

namespace on1x {

struct Unit;

// Check only parser-owned source facts, before import resolution or flattening.
// The canonical C++ loader remains the authority for paths and visibility.
// The bootstrap scanner build has no ASDL runtime; its parser and loader retain
// their ordinary checks.
void check_asdl_module(const Unit&);

} // namespace on1x

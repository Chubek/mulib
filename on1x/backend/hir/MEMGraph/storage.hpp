#pragma once
#include <on1x/backend/mir.hpp>
#include <map>

namespace on1x::memgraph {
inline constexpr std::size_t scalar_cell_bytes = 8;
inline constexpr std::size_t max_stack_bytes = 65536;
enum class Placement { Stack, Heap };

// Derived from verified, non-addressable MIR slots and their Load/Store
// edges, not serialized placement claims or SSA type size alone.
std::map<mir::Id, Placement> slot_placement(const mir::Function&, std::size_t stack_bytes);
// Advisory projection for verified straight-line scratch lifetimes. Requires
// both a read and write and no call or I/O between the slot's uses.
bool has_local_scratch(const mir::Function&);
std::string c11_scratch_layout();
std::string c11_heap_cells(const std::string& name);
}

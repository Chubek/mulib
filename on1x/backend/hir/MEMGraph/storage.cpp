#include "storage.hpp"
#include <set>

namespace on1x::memgraph {
std::map<mir::Id, Placement> slot_placement(const mir::Function& function, std::size_t stack_bytes) {
    // The caller has verified the complete CFG, its definite initialization
    // and every use of a slot ID. Slots cannot be captured or passed by
    // address: Load/Store are the entire adjacency relation. The set therefore
    // covers all initialization/use paths, including loops and early returns.
    std::set<mir::Id> uses;
    std::map<mir::Id, Placement> plan;
    for (const auto& block : function.blocks)
        for (const auto& instruction : block.instructions)
            if (instruction.op == mir::Op::Load || instruction.op == mir::Op::Store)
                uses.insert(instruction.target);
    for (const auto& slot : function.slots) {
        if (slot.type != Type::I64 || !uses.contains(slot.id)) continue;
        if (stack_bytes >= scalar_cell_bytes) {
            plan.emplace(slot.id, Placement::Stack);
            stack_bytes -= scalar_cell_bytes;
        } else plan.emplace(slot.id, Placement::Heap);
    }
    return plan;
}

bool has_local_scratch(const mir::Function& function) {
    struct Uses {
        mir::Id block=0;
        std::size_t epoch=0;
        bool load=false, store=false, spans_boundary=false;
    };
    std::map<mir::Id, Uses> uses;
    for (const auto& slot : function.slots)
        if (slot.type==Type::I64) uses.try_emplace(slot.id);
    if (uses.empty()) return false;
    for (const auto& block : function.blocks) {
        std::size_t epoch=0;
        for (const auto& inst : block.instructions) {
            const auto effect=mir::effect(inst.op);
            if (effect==mir::Effect::Call || effect==mir::Effect::IO) ++epoch;
            if (inst.op!=mir::Op::Load && inst.op!=mir::Op::Store) continue;
            const auto it=uses.find(inst.target);
            if (it==uses.end()) continue;
            auto& use=it->second;
            if (use.block==0) { use.block=block.id; use.epoch=epoch; }
            else if (use.block!=block.id || use.epoch!=epoch) use.spans_boundary=true;
            if (inst.op==mir::Op::Load) use.load=true;
            else use.store=true;
        }
    }
    for (const auto& entry : uses) {
        const auto& use=entry.second;
        if (use.load && use.store && !use.spans_boundary) return true;
    }
    return false;
}

std::string c11_scratch_layout() {
    return "_Static_assert(sizeof(int64_t) == 8 && _Alignof(int64_t) <= 8"
           " && _Alignof(int64_t) <= _Alignof(max_align_t), \"unsupported MEMGraph scratch layout\");\n";
}

std::string c11_heap_cells(const std::string& name) {
    // The fat reference and its helpers are private to this translation
    // unit. At a foreign call the wire is a thin pointer to an i64 cell.
    return "#include <stdlib.h>\n"
           "typedef struct { int64_t *data; size_t bytes; } " + name + ";\n"
           "static void " + name + "_alloc(" + name +
           " *cell, int64_t value, const char *file, size_t line, size_t column) {\n"
           "  cell->data = malloc(sizeof(int64_t));\n"
           "  if (!cell->data) { on1x_unique_allocation_fail(file, line, column); abort(); }\n"
           "  cell->bytes = sizeof(int64_t);\n"
           "  *cell->data = value;\n"
           "}\n"
           "static int64_t *" + name + "_ptr(const " + name + " *cell) {\n"
           "  if (!cell->data || cell->bytes != sizeof(int64_t)) abort();\n"
           "  return cell->data;\n"
           "}\n";
}
}

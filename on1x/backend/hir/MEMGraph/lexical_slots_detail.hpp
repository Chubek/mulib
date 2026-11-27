#pragma once
#include "lexical_slots.hpp"
#include <algorithm>
#include <utility>

namespace on1x::memgraph::detail {

// This predicate sees canonical, size-checked types. Admit only layouts that
// are already MIR values and need no retain/drop at a load or store.
inline bool lexical_copy_value(const Type& type) {
    if (type == Type::Bool || type.is_numeric()) return true;
    if (!type.is_tuple()) return false;
    return std::all_of(type.elements().begin(), type.elements().end(), lexical_copy_value);
}

class LexicalSlotBuilder {
    LexicalSlots plan;

public:
    void bind(LexicalSlot slot) {
        if (slot.root_statement) plan.statement_roots.insert(slot.root_statement);
        if (slot.root_parameter) plan.parameter_roots.insert(slot.root_parameter);
        plan.bindings.emplace(slot.binding, std::move(slot));
    }

    void read(const Expr& expression, const Stmt& binding) {
        plan.reads.emplace(&expression, &binding);
    }

    void store(const Stmt& assignment, const Stmt& binding) {
        plan.stores.emplace(&assignment, &binding);
    }

    void exchange(const Expr& call, const Stmt& binding) {
        plan.exchanges.emplace(&call, &binding);
    }

    LexicalSlots finish() && { return std::move(plan); }
};

}

namespace on1x::canonical {
// Only MEMGraph requests the checked facts. Keep the sink off public APIs.
void verify_values_with_lexical_slots(const Unit&, memgraph::detail::LexicalSlotBuilder&);
}

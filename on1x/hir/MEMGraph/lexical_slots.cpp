#include "lexical_slots_detail.hpp"

namespace on1x::memgraph {

const LexicalSlot* LexicalSlots::binding(const Stmt& statement) const {
    const auto found = bindings.find(&statement);
    return found == bindings.end() ? nullptr : &found->second;
}

const LexicalSlot* LexicalSlots::read(const Expr& expression) const {
    const auto found = reads.find(&expression);
    return found == reads.end() ? nullptr : binding(*found->second);
}

const LexicalSlot* LexicalSlots::store(const Stmt& statement) const {
    const auto found = stores.find(&statement);
    return found == stores.end() ? nullptr : binding(*found->second);
}

bool LexicalSlots::is_root(const Stmt& statement) const {
    return statement_roots.contains(&statement);
}

bool LexicalSlots::is_root(const Param& parameter) const {
    return parameter_roots.contains(&parameter);
}

LexicalSlots plan_lexical_slots(const Unit& unit) {
    detail::LexicalSlotBuilder builder;
    canonical::verify_values_with_lexical_slots(unit, builder);
    return std::move(builder).finish();
}

}

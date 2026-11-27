#pragma once
#include <on1x/ast.hpp>
#include <map>
#include <set>

namespace on1x::memgraph {

// A lexical alias is represented by the slot of its checked copy-value root,
// never by a pointer value or a fresh copy. All provenance is original HIR.
struct LexicalSlot {
    const Stmt* binding;
    const Expr* source;
    const Stmt* parent_binding; // Null when source names the root directly.
    const Stmt* root_statement; // Exactly one root is non-null.
    const Param* root_parameter;
    BorrowMode mode;
    Type type;
};

namespace detail { class LexicalSlotBuilder; }

// Compiler-private, tied to the exact, unmodified Unit used to build it.
// Collects facts from canonical value/loan verification; callers must still
// perform the rest of their usual HIR ingress checks and target admission.
class LexicalSlots {
    friend class detail::LexicalSlotBuilder;
    std::map<const Stmt*, LexicalSlot> bindings;
    std::map<const Expr*, const Stmt*> reads;
    std::map<const Stmt*, const Stmt*> stores;
    std::map<const Expr*, const Stmt*> exchanges;
    std::set<const Stmt*> statement_roots;
    std::set<const Param*> parameter_roots;

public:
    // Null means no plan. Calls with borrowed parameters are not projected.
    const LexicalSlot* binding(const Stmt&) const;
    const LexicalSlot* read(const Expr&) const;
    const LexicalSlot* store(const Stmt&) const;
    // Query the intrinsic call itself, not its address-taking first operand.
    const LexicalSlot* exchange(const Expr&) const;
    bool is_root(const Stmt&) const;
    bool is_root(const Param&) const;
};

// Runs canonical value verification and publishes no facts on failure. This
// host projection is independent of the optional LLSynth bounds rules.
LexicalSlots plan_lexical_slots(const Unit&);

}

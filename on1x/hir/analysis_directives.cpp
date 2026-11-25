#include "analysis_directives.hpp"
#include "MEMGraph/storage.hpp"
#include <on1x/backend/mir_directives.hpp>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace on1x::hir {
namespace {
struct Modes {
    bool safe=false, speculative=false;
};

Modes local_candidates(const mir::Function& function) {
    using mir::Op;
    enum class Mode { Safe, Speculative };
    Modes modes;
    if (function.slots.empty()) return modes;
    std::map<mir::Id,Mode> typed_loads;
    for (const auto& slot : function.slots) {
        if (slot.type.is_tuple()) typed_loads.emplace(slot.id,Mode::Speculative);
        else if (slot.type.is_unsigned() || slot.type==Type::F32 || slot.type==Type::F64)
            typed_loads.emplace(slot.id,Mode::Safe);
    }
    for (const auto& block : function.blocks) {
        // Presence records an access; a nonzero value records the last store.
        std::map<mir::Id,mir::Id> accesses;
        for (const auto& inst : block.instructions) {
            if (inst.op!=Op::Load && inst.op!=Op::Store) continue;
            const auto [access,first]=accesses.try_emplace(inst.target,0);
            if (inst.op==Op::Store) {
                const auto value=inst.operands[0];
                modes.speculative|=access->second==value;
                access->second=value;
            } else if (!first) {
                // A prior access is a candidate, not a value or placement
                // fact. The bank checks the reaching store/load itself.
                const auto slot=typed_loads.find(inst.target);
                modes.safe|=slot!=typed_loads.end() && slot->second==Mode::Safe;
                modes.speculative|=slot!=typed_loads.end() && slot->second==Mode::Speculative;
            }
            if (modes.safe && modes.speculative) return modes;
        }
    }
    return modes;
}

Modes canonical_candidates(const mir::Function& function) {
    using mir::Op;
    constexpr auto min=std::numeric_limits<std::int64_t>::min();
    constexpr auto max=std::numeric_limits<std::int64_t>::max();
    Modes modes;
    std::map<mir::Id,std::int64_t> literals;
    for (const auto& block : function.blocks)
        for (const auto& inst : block.instructions)
            if (inst.op==Op::Constant && inst.type==Type::I64 &&
                (inst.immediate==0 || inst.immediate==min || inst.immediate==max))
                literals.emplace(inst.id,inst.immediate);
    const auto is_literal=[&](mir::Id id,std::int64_t value) {
        const auto it=literals.find(id);
        return it!=literals.end() && it->second==value;
    };
    for (const auto& block : function.blocks) {
        std::map<mir::Id,mir::Id> negated;
        std::set<mir::Id> checked_neg;
        const auto complements=[&](mir::Id a,mir::Id b) {
            const auto it=negated.find(a);
            return it!=negated.end() && it->second==b;
        };
        for (const auto& inst : block.instructions) {
            if (inst.op==Op::Not) negated.emplace(inst.id,inst.operands[0]);
            else if (inst.op==Op::Neg) {
                modes.safe|=is_literal(inst.operands[0],0);
                modes.speculative|=checked_neg.contains(inst.operands[0]);
                checked_neg.insert(inst.id);
            } else if (inst.op==Op::Eq || inst.op==Op::Ne) {
                const auto a=inst.operands[0],b=inst.operands[1];
                modes.safe|=complements(a,b) || complements(b,a);
                // A zero comparison is worth checking, but says nothing
                // about whether the other operand is known to be nonzero.
                modes.speculative|=is_literal(b,0);
            } else if (inst.op==Op::Lt || inst.op==Op::Ge) modes.safe|=is_literal(inst.operands[1],min);
            else if (inst.op==Op::Gt) modes.safe|=is_literal(inst.operands[1],max);
            if (modes.safe && modes.speculative) return modes;
        }
    }
    return modes;
}
}

void append_analysis_directives(const Unit& unit,mir::Module& module) {
    std::map<std::string_view,const Function*,std::less<>> rewritten;
    for (const auto& function : unit.functions)
        if (!function.prototype && !function.external && function.meta_canonicalized)
            rewritten.emplace(function.name,&function);

    using Key=std::tuple<std::string,std::string,std::string,std::string,mir::Id,bool>;
    std::set<Key> existing;
    for (const auto& directive : module.directives)
        existing.emplace(directive.backend,directive.target,directive.kind,
                         directive.action,directive.function,directive.speculative);
    auto append=[&](const char* kind,const char* action,mir::Id function,
                    const char* backend="",const char* target="",bool speculative=false) {
        if (!existing.emplace(backend,target,kind,action,function,speculative).second) return;
        module.directives.push_back({backend,target,kind,action,function,speculative});
        module.version=mir::directive_format_version;
    };
    auto request=[&](const char* backend,const char* target,const char* action,
                     mir::Id function,Modes modes) {
        if (modes.safe) append("rewrite",action,function,backend,target,false);
        if (modes.speculative) append("rewrite",action,function,backend,target,true);
    };

    constexpr auto linux_amd64="x86_64-unknown-linux-gnu";
    for (const auto& function : module.functions) {
        if (memgraph::has_local_scratch(function))
            append("hint","local-memory",function.id);
        if (const auto it=rewritten.find(function.name); it!=rewritten.end()) {
            append("hint","canonicalized",function.id);
            if (it->second->meta_folded_identity)
                append("rewrite","fold-identity",function.id);
        }
        request("mir-c11",linux_amd64,mir::memgraph_local_action,function.id,local_candidates(function));
        const auto canonical=canonical_candidates(function);
        request("mir-c11",linux_amd64,mir::meta_canonical_action,function.id,canonical);
        request("mir-archon",linux_amd64,mir::meta_canonical_action,function.id,canonical);
        request("mir-vm","",mir::meta_canonical_action,function.id,canonical);
    }
}
}

#include "analysis_directives.hpp"
#include "MEMGraph/storage.hpp"
#include <map>
#include <set>
#include <tuple>

namespace on1x::hir {
void append_analysis_directives(const Unit& unit,mir::Module& module) {
    std::map<std::string_view,const Function*,std::less<>> rewritten;
    for (const auto& function : unit.functions)
        if (!function.prototype && !function.external && function.meta_canonicalized)
            rewritten.emplace(function.name,&function);

    using Key=std::tuple<std::string,std::string,mir::Id>;
    std::set<Key> existing;
    for (const auto& directive : module.directives)
        if (directive.backend.empty() && directive.target.empty() && !directive.speculative)
            existing.emplace(directive.kind,directive.action,directive.function);
    auto append=[&](const char* kind,const char* action,mir::Id function) {
        if (!existing.emplace(kind,action,function).second) return;
        module.directives.push_back({"","",kind,action,function,false});
        module.version=mir::directive_format_version;
    };

    for (const auto& function : module.functions) {
        if (memgraph::has_local_scratch(function))
            append("hint","local-memory",function.id);
        if (const auto it=rewritten.find(function.name); it!=rewritten.end()) {
            append("hint","canonicalized",function.id);
            if (it->second->meta_folded_identity)
                append("rewrite","fold-identity",function.id);
        }
    }
}
}

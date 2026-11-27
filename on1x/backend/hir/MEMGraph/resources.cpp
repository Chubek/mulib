#include "resource_detail.hpp"
#include <algorithm>

namespace on1x::memgraph {
namespace {
// Ordinary source externs have no lifetime or retention contract. Kernel
// adapters are a separate admission path, never positive resource evidence.
bool unclassified_string_resource(const Function& declaration,const Type& type) {
    return declaration.external && declaration.kernel_symbol.empty() && type==Type::String;
}
}

void detail::ResourceGraph::edge(const Expr& value, const Expr& source) {
    if (incomplete || value.type != Type::String) return;
    if (sources.size() + invalidated.size() == 16384) { incomplete = true; return; }
    sources.emplace(&value, &source);
}

void detail::ResourceGraph::invalidate(const Expr& initializer) {
    if (incomplete) return;
    if (sources.size() + invalidated.size() == 16384) { incomplete = true; return; }
    invalidated.insert(&initializer);
}

void detail::ResourceGraph::call(const Expr& call, const Function& declaration) {
    if (incomplete) return;
    auto add = [&](ResourceIssue issue, const Expr& value) {
        if (boundaries.size() == 4096) { incomplete = true; return; }
        boundaries.push_back({issue, &value});
    };
    if (unclassified_string_resource(declaration, declaration.result))
        add(ResourceIssue::UnknownResultOwner, call);
    for (const auto& argument : call.children) {
        if (incomplete) break;
        if (unclassified_string_resource(declaration, argument->type))
            add(ResourceIssue::UnknownRetention, *argument);
    }
}

std::vector<ResourceFinding> detail::ResourceGraph::finish() const {
    const auto exhausted = [] {
        return std::vector<ResourceFinding>{{ResourceIssue::Incomplete, {"<memgraph>"}}};
    };
    if (incomplete) return exhausted();
    std::size_t work = 131072;
    std::vector<ResourceFinding> findings;
    for (const auto& boundary : boundaries) {
        const Expr* source = boundary.value;
        if (boundary.issue == ResourceIssue::UnknownRetention) {
            // Edges follow a let initializer in an outer lexical binding or
            // a strictly nested region result. Even a malformed cycle cannot
            // escape the work bound. Discard partial diagnostics on exhaustion.
            while (true) {
                if (!work--) return exhausted();
                const auto parent = sources.find(source);
                if (parent == sources.end() || invalidated.contains(parent->second)) break;
                source = parent->second;
            }
            if (source->kind == Expr::Kind::String) continue;
        }
        findings.push_back({boundary.issue, boundary.value->where});
    }
    return findings;
}

std::vector<ResourceFinding> speculate_resources(const Unit& unit) {
    if (std::none_of(unit.functions.begin(), unit.functions.end(), [](const Function& declaration) {
        return unclassified_string_resource(declaration, declaration.result) ||
               std::any_of(declaration.params.begin(), declaration.params.end(), [&](const Param& parameter) {
                   return unclassified_string_resource(declaration, parameter.type);
               });
    })) return {};
    detail::ResourceGraph graph;
    canonical::verify_values_with_resources(unit, graph);
    return graph.finish();
}
}

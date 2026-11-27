#pragma once
#include "resources.hpp"
#include <map>
#include <set>

namespace on1x::memgraph::detail {
class ResourceGraph {
    // Nodes are original canonical expressions; edges point to checked value
    // sources. No edges through mutable cells, unknown calls or aggregate
    // projections: those uses stay unknown, never tentatively static.
    std::map<const Expr*, const Expr*> sources;
    std::set<const Expr*> invalidated;
    struct Boundary { ResourceIssue issue; const Expr* value; };
    std::vector<Boundary> boundaries;
    bool incomplete = false;
public:
    void edge(const Expr& value, const Expr& source);
    void invalidate(const Expr& initializer);
    void call(const Expr&, const Function&);
    std::vector<ResourceFinding> finish() const;
};
}

namespace on1x::canonical {
void verify_values_with_resources(const Unit&, memgraph::detail::ResourceGraph&);
}

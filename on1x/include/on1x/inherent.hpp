#pragma once
#include <on1x/ast.hpp>

namespace on1x::inherent {

struct Selection {
 const InherentImplementation* implementation=nullptr;
 const InherentMember* member=nullptr;
};

// A deterministic, read-only index over the host-retained declarations. Member
// names are unique per canonical nominal identity, so resolution never depends
// on source/import order and emits no runtime lookup.
class Catalog {
 std::map<std::pair<std::string,std::string>,Selection> members;
public:
 explicit Catalog(const Unit&);
 Selection find(const std::string& nominal,const std::string& member,
                const Location&) const;
};

bool present(const Unit&);

// Validate ownership/coherence and append ordinary private functions consumed by
// the existing inference, specialization and C11 paths. This is idempotent.
void prepare(Unit&);

// Checked HIR must not retain an abstract `::` projection.
void verify_elaborated(const Unit&);

} // namespace on1x::inherent

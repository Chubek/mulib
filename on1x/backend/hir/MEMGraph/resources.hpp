#pragma once
#include <on1x/ast.hpp>

namespace on1x::memgraph {
enum class ResourceIssue { UnknownRetention, UnknownResultOwner, Incomplete };
struct ResourceFinding { ResourceIssue issue; Location where; };

// A bounded advisory projection from canonical value verification. An empty
// result is not evidence of foreign safety, ownership, or no retention.
std::vector<ResourceFinding> speculate_resources(const Unit&);
}

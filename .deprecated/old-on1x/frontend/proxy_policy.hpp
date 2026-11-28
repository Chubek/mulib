#pragma once
#include <string>

namespace on1x::proxy {
struct Policy {
 std::string http,https,any;
 bool none=false;
};

// Only the working directory's literal root On1xfile has this authority.
Policy current_project();

// Apply to the CLI's ambient host environment, not to isolated workers.
void apply(const Policy&);
}

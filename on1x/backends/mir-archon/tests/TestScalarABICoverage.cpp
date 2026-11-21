// Query the authoritative MIR signatures, not a test copy of the allowlist.
#include <cstdint>
#include "../../../src/backend/numeric.hpp"
#include <iostream>
#include <string>

int main() {
    std::string name;
    while (std::getline(std::cin, name)) {
        const auto signature = on1x::mir::detail::numeric_signature(name);
        if (!signature) continue;
        std::cout << name << ' ' << on1x::type_name(signature->result());
        for (const auto &argument : signature->parameters())
            std::cout << ' ' << on1x::type_name(argument);
        std::cout << '\n';
    }
}

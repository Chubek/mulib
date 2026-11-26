#include "Builtin.hpp"

#include <iostream>

int main()
{
    std::cout << on1x::backends::builtin_capabilities_json();
    return std::cout ? 0 : 1;
}

#pragma once

#include "BuiltinContext.hpp"
#include <on1x/backend/mir.hpp>

namespace on1x::backends {
struct Product {
    std::string bytes;
    int exit_code;
    std::string diagnostic;
};

Product run_c11(const Unit &, std::string_view source);
Product run_mir(const mir::Module &, const char *backend);
}

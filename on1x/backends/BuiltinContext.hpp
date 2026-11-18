#pragma once

#include <exception>
#include <string>

namespace on1x::backends {
// Private to the compiler and its explicitly linked built-ins.
struct Context {
    std::exception_ptr error;
    int exit_code = 0;
    std::string diagnostic;
};
}

#pragma once
#include <on1x/c11.hpp>

#include <exception>
#include <string>
#include <string_view>

namespace on1x::backends {
// Private to the compiler and its explicitly linked built-ins.
struct Context {
    std::exception_ptr error;
    int exit_code = 0;
    std::string diagnostic;
    C11Options c11;
    bool native_pic = false;
    bool native_emit_c_entrypoint = true;
    std::string_view mir_target;
    bool mir_speculate = false;
};
}

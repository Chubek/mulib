#pragma once

#include "BuiltinContext.hpp"
#include <on1x/backend/mir.hpp>

namespace on1x::backends {
struct Product {
    std::string bytes;
    int exit_code;
    std::string diagnostic;
};

Product run_c11(const Unit &, std::string_view source, const C11Options & = {});
Product run_mir(const mir::Module &, const char *backend,
                std::string_view target = {}, bool speculate = false);

// Describes this binary's compiler-linked SDK adapters, not an input, installed
// toolchain, external provider registry, or qualification run. No backend runs.
std::string builtin_capabilities_json();

// Native output is assembly only. External assembly/linking belongs to the
// explicitly selected build tool, never to a compiler backend callback.
struct NativeOptions {
    bool pic = false;
    bool emit_c_entrypoint = true;
    std::size_t byte_limit = 16u * 1024u * 1024u;
    int (*cancelled)(void *) = nullptr;
    void *cancel_userdata = nullptr;
    bool speculate = false;
};
Product run_mir_native(const mir::Module &, std::string_view target,
                       const NativeOptions & = {});
}

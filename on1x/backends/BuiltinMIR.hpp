#pragma once

#include "BuiltinContext.hpp"
#include "On1xSDK-Backend.h"
#include "ProgramInternal.hpp"

#include <on1x/backend/mir_directives.hpp>

#include <cstdint>

namespace on1x::backends {

template<class Consume>
On1xSDK_Status consume_portable_mir(const On1xSDK_Program *program, Consume consume) {
    // Preserve the synchronous in-memory path and its independent byte limits.
    if (const auto *module = sdk::builtin_mir(program))
        return consume(*module);

    const std::uint8_t *bytes = nullptr;
    std::size_t size = 0;
    if (const auto status = on1x_sdk_program_bytes(program, &bytes, &size);
        status != ON1X_SDK_OK)
        return status;
    const auto wire = size ? std::string_view(reinterpret_cast<const char *>(bytes), size)
                           : std::string_view{};
    return consume(mir::deserialize(wire));
}

inline On1xSDK_Status selected_mir_target(const On1xSDK_BackendInvocation &invocation,
                                          const Context &context, std::string_view &target) {
    target = context.mir_target;
    if (invocation.target) {
        const On1xSDK_TargetDescriptor *descriptor = nullptr;
        if (const auto status = on1x_sdk_target_view(invocation.target, &descriptor);
            status != ON1X_SDK_OK)
            return status;
        target = {descriptor->triple, descriptor->triple_size};
    }
    return ON1X_SDK_OK;
}

// Only compiler-linked providers use this helper, after SDK provider admission.
// The selection comes from the invoked backend and the host, never from MIR.
template<class Consume>
On1xSDK_Status consume_specialized_mir(const On1xSDK_BackendInvocation &invocation,
                                       const Context &context, std::string_view backend,
                                       Consume consume) {
    std::string_view target;
    if (const auto status = selected_mir_target(invocation, context, target);
        status != ON1X_SDK_OK)
        return status;
    return consume_portable_mir(invocation.program, [&](const mir::Module &module) {
        if (module.version != mir::directive_format_version) return consume(module);
        return consume(mir::specialize(module, backend, target, context.mir_speculate));
    });
}

} // namespace on1x::backends

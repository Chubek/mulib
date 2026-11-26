#pragma once

#include "BuiltinContext.hpp"
#include "backend/BackendProviderInternal.hpp"

namespace on1x::backends {

// Single descriptor source for the compiler-linked registration and report.
// The report visitor borrows the descriptor only during this call.
inline On1xSDK_Status register_or_describe(
    On1xSDK_Registry *registry, Context &context, const On1xSDK_BackendProvider &provider,
    On1xSDK_Status prerequisite = ON1X_SDK_OK) {
    if (!registry && context.describe_provider) {
        context.describe_provider(context.describe_userdata, provider, prerequisite == ON1X_SDK_OK);
        return ON1X_SDK_OK;
    }
    if (prerequisite != ON1X_SDK_OK) return prerequisite;
    return sdk::register_builtin_backend_provider(registry, &provider);
}

} // namespace on1x::backends

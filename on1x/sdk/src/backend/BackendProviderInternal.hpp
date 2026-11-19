#ifndef ON1X_SDK_BACKEND_PROVIDER_INTERNAL_HPP
#define ON1X_SDK_BACKEND_PROVIDER_INTERNAL_HPP

#include "On1xSDK-Backend.h"

namespace on1x::sdk {

/* Trusted compiler-linked providers may consume private checked-HIR/MIR views. */
On1xSDK_Status register_builtin_backend_provider(
    On1xSDK_Registry *registry, const On1xSDK_BackendProvider *provider) noexcept;

} // namespace on1x::sdk

#endif

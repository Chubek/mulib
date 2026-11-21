#ifndef ON1X_SDK_BACKEND_PROVIDER_INTERNAL_HPP
#define ON1X_SDK_BACKEND_PROVIDER_INTERNAL_HPP

#include "On1xSDK-Backend.h"

namespace on1x::sdk {

/* Host grant, never exposed through a plugin host table. Trusted compiler-linked
 * providers may consume private checked-HIR/MIR views. Native admission is
 * limited to target-bound assembly from verified MIR, explicitly selected by
 * name at produce time. Providers must run canonical verification and their
 * target's legality checks before writing output; registration is not proof. */
On1xSDK_Status register_builtin_backend_provider(
    On1xSDK_Registry *registry, const On1xSDK_BackendProvider *provider) noexcept;

} // namespace on1x::sdk

#endif

#include "On1xSDK-Toolchain.h"
#include "MetadataInternal.hpp"

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;

constexpr auto kind = Kind::Toolchain;
using D = On1xSDK_Toolchain;
using F = md::Field<D>;

} // namespace

extern "C" On1xSDK_Status on1x_sdk_toolchain_register(On1xSDK_Registry* registry,
                                                     const On1xSDK_Toolchain* toolchain) {
    if (const auto status = md::check(registry, toolchain); status != ON1X_SDK_OK) return status;
    md::Validation validate;
    const std::array fields{
        F{&D::name, validate.name(toolchain->name)},
        F{&D::target, validate.field(toolchain->target, ON1X_SDK_NAME_MAX, md::valid_target)},
        F{&D::driver_id, validate.name(toolchain->driver_id)},
        F{&D::version, validate.text(toolchain->version, ON1X_SDK_NAME_MAX)}
    };
    if (validate.status != ON1X_SDK_OK) return validate.status;
    return md::register_fields(registry, kind, *toolchain, fields);
}

extern "C" const On1xSDK_Toolchain* on1x_sdk_toolchain_find(const On1xSDK_Registry* registry,
                                                           const char* name) {
    return md::lookup<On1xSDK_Toolchain>(registry, kind, name);
}

extern "C" size_t on1x_sdk_toolchain_count(const On1xSDK_Registry* registry) {
    return count(registry, kind);
}

extern "C" const On1xSDK_Toolchain* on1x_sdk_toolchain_at(const On1xSDK_Registry* registry, size_t index) {
    return static_cast<const On1xSDK_Toolchain*>(at(registry, kind, index));
}

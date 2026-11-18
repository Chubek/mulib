#include "On1xSDK-ExecEnv.h"
#include "MetadataInternal.hpp"

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;

constexpr auto kind = Kind::ExecEnv;
constexpr uint32_t capabilities = ON1X_SDK_EXECENV_FS_READ | ON1X_SDK_EXECENV_FS_WRITE |
                                  ON1X_SDK_EXECENV_NETWORK | ON1X_SDK_EXECENV_SUBPROCESS;
using D = On1xSDK_ExecEnv;
using F = md::Field<D>;

} // namespace

extern "C" On1xSDK_Status on1x_sdk_execenv_register(On1xSDK_Registry* registry,
                                                    const On1xSDK_ExecEnv* environment) {
    if (const auto status = md::check(registry, environment); status != ON1X_SDK_OK) return status;
    if (environment->requested_capabilities & ~capabilities) return ON1X_SDK_INVALID_ARGUMENT;
    md::Validation validate;
    const std::array fields{
        F{&D::name, validate.name(environment->name)},
        F{&D::toolchain, validate.name(environment->toolchain, true)},
        F{&D::description, validate.text(environment->description, ON1X_SDK_MAX_EXECENV_DESCRIPTION_BYTES, true)}
    };
    if (validate.status != ON1X_SDK_OK) return validate.status;
    return md::register_fields(registry, kind, *environment, fields);
}

extern "C" const On1xSDK_ExecEnv* on1x_sdk_execenv_find(const On1xSDK_Registry* registry, const char* name) {
    return md::lookup<On1xSDK_ExecEnv>(registry, kind, name);
}

extern "C" size_t on1x_sdk_execenv_count(const On1xSDK_Registry* registry) {
    return count(registry, kind);
}

extern "C" const On1xSDK_ExecEnv* on1x_sdk_execenv_at(const On1xSDK_Registry* registry, size_t index) {
    return static_cast<const On1xSDK_ExecEnv*>(at(registry, kind, index));
}

#ifndef ON1X_SDK_REGISTRY_INTERNAL_HPP
#define ON1X_SDK_REGISTRY_INTERNAL_HPP

#include "On1xSDK-Types.h"

#include <memory>
#include <string_view>

namespace on1x::sdk {

enum class Kind { Analyzer, Transformer, Backend, Contract, Module, CLIOpt, Toolchain, ExecEnv };

// The caller owns validation/copying of the typed descriptor and its fields.
// This function copies the key and keeps the immutable descriptor alive.
// Destruction of that descriptor must not throw or reenter the registry.
// add may throw; the caller is responsible for its C boundary.
On1xSDK_Status add(On1xSDK_Registry*, Kind, std::string_view name, std::shared_ptr<const void>);
const void* find(const On1xSDK_Registry*, Kind, std::string_view) noexcept;
const void* at(const On1xSDK_Registry*, Kind, size_t) noexcept;
size_t count(const On1xSDK_Registry*, Kind) noexcept;
bool valid_name(std::string_view) noexcept;

} // namespace on1x::sdk

#endif

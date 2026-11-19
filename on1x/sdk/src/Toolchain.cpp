#include "On1xSDK-Toolchain.h"
#include "MetadataInternal.hpp"
#include "toolchain/ToolchainInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;
namespace tc = on1x::sdk::toolchain;

constexpr auto kind = Kind::Toolchain;
constexpr std::uint32_t all_roles =
    ON1X_SDK_TOOL_C_COMPILER | ON1X_SDK_TOOL_ASSEMBLER | ON1X_SDK_TOOL_LINKER |
    ON1X_SDK_TOOL_ARCHIVER | ON1X_SDK_TOOL_OBJDUMP;

bool view(const char *data, std::size_t size, std::size_t limit, bool optional,
          std::string_view &out) noexcept {
    out = {};
    if (size > limit || (size && !data) || (!size && !optional)) return false;
    if (!size) return true;
    out = {data, size};
    return std::none_of(out.begin(), out.end(), [](char c) { return c == '\0'; });
}

bool printable(std::string_view text) noexcept {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return c >= 0x20u && c <= 0x7eu;
           });
}

On1xSDK_Status header(const void *descriptor, std::uint32_t expected,
                      std::size_t required) noexcept {
    if (!descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t abi{}, size{};
    const auto *bytes = static_cast<const unsigned char *>(descriptor);
    std::memcpy(&abi, bytes, sizeof abi);
    std::memcpy(&size, bytes + sizeof abi, sizeof size);
    return abi == expected && size >= required ? ON1X_SDK_OK
                                              : ON1X_SDK_ABI_MISMATCH;
}

template <class Function>
On1xSDK_Status guarded(Function &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

const tc::StoredToolchain *stored_find(const On1xSDK_Registry *registry,
                                      std::string_view name) noexcept {
    return static_cast<const tc::StoredToolchain *>(find(registry, kind, name));
}

} // namespace

namespace on1x::sdk::toolchain {

void StoredToolchain::refresh_legacy() noexcept {
    legacy = {
        ON1X_SDK_ABI_VERSION,
        sizeof(On1xSDK_Toolchain),
        name.c_str(),
        target_text.c_str(),
        driver_id.c_str(),
        version.c_str()
    };
}

bool nonzero_digest(const std::uint8_t *digest) noexcept {
    for (std::size_t index = 0; index < ON1X_SDK_TOOL_DIGEST_BYTES; ++index)
        if (digest[index]) return true;
    return false;
}

} // namespace on1x::sdk::toolchain

extern "C" {

On1xSDK_Status on1x_sdk_toolchain_register(On1xSDK_Registry *registry,
                                            const On1xSDK_Toolchain *toolchain) {
    if (const auto status = md::check(registry, toolchain); status != ON1X_SDK_OK)
        return status;
    md::Validation validate;
    const auto name = validate.name(toolchain->name);
    const auto target = validate.field(toolchain->target, ON1X_SDK_NAME_MAX,
                                       md::valid_target);
    const auto driver = validate.name(toolchain->driver_id);
    const auto version = validate.text(toolchain->version, ON1X_SDK_NAME_MAX);
    if (validate.status != ON1X_SDK_OK) return validate.status;
    return guarded([&] {
        auto stored = std::make_shared<tc::StoredToolchain>();
        stored->flavor = tc::Flavor::Legacy;
        stored->name.assign(name);
        stored->target_text.assign(target);
        stored->driver_id.assign(driver);
        stored->version.assign(version);
        stored->refresh_legacy();
        return add(registry, kind, name, std::move(stored));
    });
}

On1xSDK_Status on1x_sdk_toolchain_provider_register(
    On1xSDK_Registry *registry, const On1xSDK_ToolchainProvider *provider) {
    if (!registry || !provider) return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status =
            header(provider, ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION,
                   sizeof(On1xSDK_ToolchainProvider));
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_ToolchainProvider input{};
    std::memcpy(&input, provider, sizeof input);
    std::string_view name, driver, version;
    if (!view(input.name, input.name_size, ON1X_SDK_NAME_MAX, false, name) ||
        !valid_name(name) ||
        !view(input.driver_id, input.driver_id_size, ON1X_SDK_NAME_MAX, false,
              driver) ||
        !valid_name(driver) ||
        !view(input.version, input.version_size, ON1X_SDK_NAME_MAX, false,
              version) ||
        !printable(version) || !input.target || !input.roles ||
        (input.roles & ~all_roles) || !tc::nonzero_digest(input.driver_sha256))
        return ON1X_SDK_INVALID_ARGUMENT;
    const On1xSDK_TargetDescriptor *target = nullptr;
    if (const auto status = on1x_sdk_target_view(input.target, &target);
        status != ON1X_SDK_OK)
        return status;

    return guarded([&] {
        On1xSDK_Target *clone = nullptr;
        const auto clone_status = on1x_sdk_target_create(target, &clone);
        if (clone_status != ON1X_SDK_OK) return clone_status;
        std::shared_ptr<On1xSDK_Target> target_owner(
            clone, [](On1xSDK_Target *value) { on1x_sdk_target_destroy(value); });
        auto stored = std::make_shared<tc::StoredToolchain>();
        stored->flavor = tc::Flavor::Provider;
        stored->name.assign(name);
        stored->target_text.assign(target->triple, target->triple_size);
        stored->driver_id.assign(driver);
        stored->version.assign(version);
        stored->target = std::move(target_owner);
        stored->priority = input.priority;
        stored->roles = input.roles;
        std::copy(std::begin(input.driver_sha256), std::end(input.driver_sha256),
                  stored->digest.begin());
        stored->refresh_legacy();
        return add(registry, kind, name, std::move(stored));
    });
}

const On1xSDK_Toolchain *on1x_sdk_toolchain_find(
    const On1xSDK_Registry *registry, const char *name) {
    std::string_view key;
    if (md::bounded(name, ON1X_SDK_NAME_MAX, key) != ON1X_SDK_OK ||
        !valid_name(key))
        return nullptr;
    const auto *stored = stored_find(registry, key);
    return stored ? &stored->legacy : nullptr;
}

size_t on1x_sdk_toolchain_count(const On1xSDK_Registry *registry) {
    return count(registry, kind);
}

const On1xSDK_Toolchain *on1x_sdk_toolchain_at(
    const On1xSDK_Registry *registry, size_t index) {
    const auto *stored =
        static_cast<const tc::StoredToolchain *>(at(registry, kind, index));
    return stored ? &stored->legacy : nullptr;
}

} // extern "C"

#include "On1xSDK-ToolchainResolver.h"
#include "RegistryInternal.hpp"
#include "backend/TargetInternal.hpp"
#include "toolchain/ToolchainInternal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

using on1x::sdk::Kind;
namespace tc = on1x::sdk::toolchain;

constexpr std::uint32_t all_roles =
    ON1X_SDK_TOOL_C_COMPILER | ON1X_SDK_TOOL_ASSEMBLER | ON1X_SDK_TOOL_LINKER |
    ON1X_SDK_TOOL_ARCHIVER | ON1X_SDK_TOOL_OBJDUMP;

struct Binding {
    std::string driver_id;
    std::uint32_t roles{};
    std::string path;
    std::array<std::uint8_t, ON1X_SDK_TOOL_DIGEST_BYTES> digest{};
};

bool view(const char *data, std::size_t size, std::size_t limit, bool optional,
          std::string_view &out) noexcept {
    out = {};
    if (size > limit || (size && !data) || (!size && !optional)) return false;
    if (!size) return true;
    out = {data, size};
    return std::none_of(out.begin(), out.end(), [](char c) { return c == '\0'; });
}

On1xSDK_Status header(const void *descriptor, std::size_t required) noexcept {
    if (!descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t abi{}, size{};
    const auto *bytes = static_cast<const unsigned char *>(descriptor);
    std::memcpy(&abi, bytes, sizeof abi);
    std::memcpy(&size, bytes + sizeof abi, sizeof size);
    return abi == ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION && size >= required
               ? ON1X_SDK_OK
               : ON1X_SDK_ABI_MISMATCH;
}

bool one_role(std::uint32_t role) noexcept {
    return role && !(role & (role - 1u)) && !(role & ~all_roles);
}

bool safe_path(std::string_view path) noexcept {
    if (path.empty() || path.size() > ON1X_SDK_MAX_TOOL_PATH_BYTES ||
        path.front() != '/' || path.size() == 1 || path.back() == '/')
        return false;
    std::size_t begin = 1;
    while (begin < path.size()) {
        const auto end = path.find('/', begin);
        const auto part =
            path.substr(begin, end == path.npos ? path.size() - begin : end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        for (const char raw : part) {
            const auto c = static_cast<unsigned char>(raw);
            const bool safe = (c >= 'a' && c <= 'z') ||
                              (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                              c == '.' || c == '+' || c == '@';
            if (!safe) return false;
        }
        if (end == path.npos) break;
        begin = end + 1;
    }
    return true;
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

} // namespace

struct On1xSDK_ToolchainResolver {
    std::size_t maximum{};
    std::vector<Binding> bindings;
};

struct On1xSDK_ToolResolution {
    std::string provider_name;
    std::string driver_id;
    std::string version;
    std::string target_triple;
    std::string path;
    On1xSDK_ToolResolutionView view{};

    void refresh(std::uint32_t role,
                 const std::array<std::uint8_t, ON1X_SDK_TOOL_DIGEST_BYTES> &digest) {
        view = {
            ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION,
            sizeof(On1xSDK_ToolResolutionView),
            provider_name.data(),
            provider_name.size(),
            driver_id.data(),
            driver_id.size(),
            version.data(),
            version.size(),
            target_triple.data(),
            target_triple.size(),
            role,
            0,
            path.data(),
            path.size(),
            {}
        };
        std::copy(digest.begin(), digest.end(), view.sha256);
    }
};

extern "C" {

On1xSDK_Status on1x_sdk_toolchain_resolver_create(
    size_t max_bindings, On1xSDK_ToolchainResolver **out_resolver) {
    if (!out_resolver) return ON1X_SDK_INVALID_ARGUMENT;
    *out_resolver = nullptr;
    if (!max_bindings) return ON1X_SDK_INVALID_ARGUMENT;
    if (max_bindings > ON1X_SDK_MAX_TOOLCHAIN_BINDINGS) return ON1X_SDK_LIMIT;
    return guarded([&] {
        auto resolver = std::make_unique<On1xSDK_ToolchainResolver>();
        resolver->maximum = max_bindings;
        resolver->bindings.reserve(max_bindings);
        *out_resolver = resolver.release();
        return ON1X_SDK_OK;
    });
}

void on1x_sdk_toolchain_resolver_destroy(On1xSDK_ToolchainResolver *resolver) {
    delete resolver;
}

On1xSDK_Status on1x_sdk_toolchain_resolver_authorize(
    On1xSDK_ToolchainResolver *resolver, const On1xSDK_ToolBinding *binding) {
    if (!resolver || !binding) return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status = header(binding, sizeof(On1xSDK_ToolBinding));
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_ToolBinding input{};
    std::memcpy(&input, binding, sizeof input);
    std::string_view driver, path;
    if (!view(input.driver_id, input.driver_id_size, ON1X_SDK_NAME_MAX, false,
              driver) ||
        !on1x::sdk::valid_name(driver) ||
        !view(input.absolute_path, input.absolute_path_size,
              ON1X_SDK_MAX_TOOL_PATH_BYTES, false, path) ||
        !safe_path(path) || !input.roles || (input.roles & ~all_roles) ||
        input.reserved || !tc::nonzero_digest(input.sha256))
        return ON1X_SDK_INVALID_ARGUMENT;
    for (const auto &existing : resolver->bindings)
        if (existing.driver_id == driver && (existing.roles & input.roles))
            return ON1X_SDK_DUPLICATE;
    if (resolver->bindings.size() == resolver->maximum) return ON1X_SDK_LIMIT;
    return guarded([&] {
        Binding stored;
        stored.driver_id.assign(driver);
        stored.roles = input.roles;
        stored.path.assign(path);
        std::copy(std::begin(input.sha256), std::end(input.sha256),
                  stored.digest.begin());
        resolver->bindings.push_back(std::move(stored));
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_toolchain_resolve(
    const On1xSDK_Registry *registry,
    const On1xSDK_ToolchainResolver *resolver,
    const On1xSDK_ToolchainRequest *request,
    On1xSDK_ToolResolution **out_resolution) {
    if (!out_resolution) return ON1X_SDK_INVALID_ARGUMENT;
    *out_resolution = nullptr;
    if (!registry || !resolver || !request) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t request_size{};
    std::memcpy(&request_size, &request->struct_size, sizeof request_size);
    if (const auto status =
            header(request, offsetof(On1xSDK_ToolchainRequest, provider_name));
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_ToolchainRequest input{};
    std::memcpy(&input, request,
                std::min<std::size_t>(request_size, sizeof input));
    if (request_size < offsetof(On1xSDK_ToolchainRequest, provider_name) +
                           sizeof(input.provider_name))
        input.provider_name = nullptr;
    if (request_size <
        offsetof(On1xSDK_ToolchainRequest, provider_name_size) +
            sizeof(input.provider_name_size))
        input.provider_name_size = 0;
    std::string_view requested_name;
    if (!input.target || !one_role(input.role) ||
        !view(input.provider_name, input.provider_name_size, ON1X_SDK_NAME_MAX,
              true, requested_name) ||
        (!requested_name.empty() && !on1x::sdk::valid_name(requested_name)))
        return ON1X_SDK_INVALID_ARGUMENT;

    const tc::StoredToolchain *selected = nullptr;
    bool ambiguous = false;
    std::int32_t priority = std::numeric_limits<std::int32_t>::min();
    const auto entries = on1x::sdk::count(registry, Kind::Toolchain);
    for (std::size_t index = 0; index < entries; ++index) {
        const auto *candidate = static_cast<const tc::StoredToolchain *>(
            on1x::sdk::at(registry, Kind::Toolchain, index));
        if (!candidate || candidate->flavor != tc::Flavor::Provider ||
            !(candidate->roles & input.role) ||
            (!requested_name.empty() && candidate->name != requested_name) ||
            !candidate->target ||
            !on1x::sdk::target::equal(*candidate->target, *input.target))
            continue;
        if (!selected || candidate->priority > priority) {
            selected = candidate;
            priority = candidate->priority;
            ambiguous = false;
        } else if (candidate->priority == priority) {
            ambiguous = true;
        }
    }
    if (!selected) return ON1X_SDK_UNSUPPORTED;
    if (ambiguous) return ON1X_SDK_DUPLICATE;

    const Binding *authorized = nullptr;
    for (const auto &binding : resolver->bindings)
        if (binding.driver_id == selected->driver_id &&
            (binding.roles & input.role) &&
            binding.digest == selected->digest) {
            authorized = &binding;
            break;
        }
    if (!authorized) return ON1X_SDK_UNSUPPORTED;

    /* Copy everything before publishing the resolution; it remains valid after
     * registry and resolver destruction. */
    return guarded([&] {
        auto result = std::make_unique<On1xSDK_ToolResolution>();
        result->provider_name = selected->name;
        result->driver_id = selected->driver_id;
        result->version = selected->version;
        result->target_triple = selected->target_text;
        result->path = authorized->path;
        result->refresh(input.role, selected->digest);
        *out_resolution = result.release();
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_tool_resolution_view(
    const On1xSDK_ToolResolution *resolution,
    const On1xSDK_ToolResolutionView **out_view) {
    if (!out_view) return ON1X_SDK_INVALID_ARGUMENT;
    *out_view = nullptr;
    if (!resolution) return ON1X_SDK_INVALID_ARGUMENT;
    *out_view = &resolution->view;
    return ON1X_SDK_OK;
}

void on1x_sdk_tool_resolution_destroy(On1xSDK_ToolResolution *resolution) {
    delete resolution;
}

} // extern "C"

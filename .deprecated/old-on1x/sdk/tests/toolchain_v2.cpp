#include "On1xSDK-Registry.h"
#include "On1xSDK-Toolchain.h"
#include "On1xSDK-ToolchainResolver.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

using Registry = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;
using Resolver = std::unique_ptr<On1xSDK_ToolchainResolver,
                                 decltype(&on1x_sdk_toolchain_resolver_destroy)>;
using Resolution = std::unique_ptr<On1xSDK_ToolResolution,
                                   decltype(&on1x_sdk_tool_resolution_destroy)>;
using Target = std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>;

Registry registry() {
    On1xSDK_Registry *raw = nullptr;
    CHECK(on1x_sdk_registry_create(&raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_registry_destroy};
}

Resolver resolver(size_t limit) {
    On1xSDK_ToolchainResolver *raw = nullptr;
    CHECK(on1x_sdk_toolchain_resolver_create(limit, &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_toolchain_resolver_destroy};
}

Target target(bool feature = false) {
    const On1xSDK_TargetFeature f{"avx2", 4};
    const On1xSDK_DataLayout layout{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
        ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, 128};
    constexpr char triple[] = "x86_64-unknown-linux-gnu";
    const On1xSDK_TargetDescriptor descriptor{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
        triple, sizeof(triple) - 1, nullptr, 0, layout,
        ON1X_SDK_OBJECT_FORMAT_ELF, ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64,
        feature ? &f : nullptr, feature ? 1u : 0u};
    On1xSDK_Target *raw = nullptr;
    CHECK(on1x_sdk_target_create(&descriptor, &raw) == ON1X_SDK_OK);
    return {raw, on1x_sdk_target_destroy};
}

std::array<uint8_t, ON1X_SDK_TOOL_DIGEST_BYTES> digest(uint8_t seed) {
    std::array<uint8_t, ON1X_SDK_TOOL_DIGEST_BYTES> value{};
    for (size_t index = 0; index < value.size(); ++index)
        value[index] = static_cast<uint8_t>(seed + index);
    return value;
}

On1xSDK_Status add_provider(On1xSDK_Registry *registry, const char *name,
                            const On1xSDK_Target *target, const char *driver,
                            const char *version, int priority, uint32_t roles,
                            const std::array<uint8_t, 32> &sha) {
    On1xSDK_ToolchainProvider provider{
        ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION,
        sizeof(On1xSDK_ToolchainProvider),
        name, std::strlen(name), target,
        driver, std::strlen(driver), version, std::strlen(version),
        priority, roles, {}};
    std::copy(sha.begin(), sha.end(), provider.driver_sha256);
    return on1x_sdk_toolchain_provider_register(registry, &provider);
}

On1xSDK_Status authorize(On1xSDK_ToolchainResolver *resolver,
                         const char *driver, uint32_t roles, const char *path,
                         const std::array<uint8_t, 32> &sha) {
    On1xSDK_ToolBinding binding{
        ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION, sizeof(On1xSDK_ToolBinding),
        driver, std::strlen(driver), roles, 0, path, std::strlen(path), {}};
    std::copy(sha.begin(), sha.end(), binding.sha256);
    return on1x_sdk_toolchain_resolver_authorize(resolver, &binding);
}

On1xSDK_ToolchainRequest request(const On1xSDK_Target *target) {
    return {ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION,
            sizeof(On1xSDK_ToolchainRequest), target,
            ON1X_SDK_TOOL_C_COMPILER, nullptr, 0};
}

} // namespace

int main() {
    On1xSDK_ToolchainResolver *none =
        reinterpret_cast<On1xSDK_ToolchainResolver *>(1);
    CHECK(on1x_sdk_toolchain_resolver_create(0, &none) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(none == nullptr);
    CHECK(on1x_sdk_toolchain_resolver_create(
              ON1X_SDK_MAX_TOOLCHAIN_BINDINGS + 1u, &none) == ON1X_SDK_LIMIT);

    auto owner = registry();
    auto host = resolver(8);
    auto x64 = target();
    const auto low_digest = digest(1);
    const auto high_digest = digest(33);
    char low_name[] = "tool.low";
    char low_driver[] = "clang-low";
    char low_version[] = "18.1";
    CHECK(add_provider(owner.get(), low_name, x64.get(), low_driver, low_version,
                       1, ON1X_SDK_TOOL_C_COMPILER | ON1X_SDK_TOOL_ASSEMBLER,
                       low_digest) == ON1X_SDK_OK);
    low_name[0] = 'x';
    low_driver[0] = 'x';
    low_version[0] = 'x';
    CHECK(authorize(host.get(), "clang-low",
                    ON1X_SDK_TOOL_C_COMPILER | ON1X_SDK_TOOL_ASSEMBLER,
                    "/opt/on1x-sdk/bin/clang", low_digest) == ON1X_SDK_OK);

    const auto *legacy = on1x_sdk_toolchain_find(owner.get(), "tool.low");
    CHECK(legacy && std::strcmp(legacy->driver_id, "clang-low") == 0 &&
          std::strcmp(legacy->target, "x86_64-unknown-linux-gnu") == 0);
    CHECK(on1x_sdk_toolchain_count(owner.get()) == 1);
    CHECK(on1x_sdk_toolchain_at(owner.get(), 0) == legacy);

    auto query = request(x64.get());
    On1xSDK_ToolResolution *raw = nullptr;
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_OK);
    Resolution low(raw, on1x_sdk_tool_resolution_destroy);
    const On1xSDK_ToolResolutionView *view = nullptr;
    CHECK(on1x_sdk_tool_resolution_view(low.get(), &view) == ON1X_SDK_OK);
    CHECK(view && std::string(view->provider_name, view->provider_name_size) ==
                      "tool.low");
    CHECK(std::string(view->absolute_path, view->absolute_path_size) ==
          "/opt/on1x-sdk/bin/clang");
    CHECK(std::equal(low_digest.begin(), low_digest.end(), view->sha256));

    CHECK(add_provider(owner.get(), "tool.high", x64.get(), "clang-high", "19.0",
                       2, ON1X_SDK_TOOL_C_COMPILER, high_digest) == ON1X_SDK_OK);
    CHECK(authorize(host.get(), "clang-high", ON1X_SDK_TOOL_C_COMPILER,
                    "/relocated/toolset/bin/clang", high_digest) == ON1X_SDK_OK);
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_OK);
    Resolution high(raw, on1x_sdk_tool_resolution_destroy);
    CHECK(on1x_sdk_tool_resolution_view(high.get(), &view) == ON1X_SDK_OK);
    CHECK(std::string(view->provider_name, view->provider_name_size) ==
          "tool.high");
    CHECK(std::string(view->absolute_path, view->absolute_path_size) ==
          "/relocated/toolset/bin/clang");

    const auto tie_digest = digest(65);
    CHECK(add_provider(owner.get(), "tool.tie", x64.get(), "clang-tie", "20.0",
                       2, ON1X_SDK_TOOL_C_COMPILER, tie_digest) == ON1X_SDK_OK);
    CHECK(authorize(host.get(), "clang-tie", ON1X_SDK_TOOL_C_COMPILER,
                    "/another/root/bin/clang", tie_digest) == ON1X_SDK_OK);
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_DUPLICATE);
    CHECK(raw == nullptr);
    query.provider_name = "tool.low";
    query.provider_name_size = 8;
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_OK);
    Resolution explicit_low(raw, on1x_sdk_tool_resolution_destroy);

    auto featured = target(true);
    query = request(featured.get());
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_UNSUPPORTED);
    query = request(x64.get());
    query.role = ON1X_SDK_TOOL_C_COMPILER | ON1X_SDK_TOOL_ASSEMBLER;
    CHECK(on1x_sdk_toolchain_resolve(owner.get(), host.get(), &query, &raw) ==
          ON1X_SDK_INVALID_ARGUMENT);

    auto unsafe = resolver(2);
    for (const char *path : {"clang", "/usr/../bin/clang", "/bin/clang;sh",
                             "/bin/clang tool", "/bin//clang", "/bin/"})
        CHECK(authorize(unsafe.get(), "driver", ON1X_SDK_TOOL_C_COMPILER,
                        path, low_digest) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(authorize(unsafe.get(), "/bin/sh", ON1X_SDK_TOOL_C_COMPILER,
                    "/bin/clang", low_digest) == ON1X_SDK_INVALID_ARGUMENT);
    std::array<uint8_t, 32> zero{};
    CHECK(authorize(unsafe.get(), "driver", ON1X_SDK_TOOL_C_COMPILER,
                    "/bin/clang", zero) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(authorize(unsafe.get(), "driver", ON1X_SDK_TOOL_C_COMPILER,
                    "/bin/clang", low_digest) == ON1X_SDK_OK);
    CHECK(authorize(unsafe.get(), "driver", ON1X_SDK_TOOL_C_COMPILER,
                    "/other/clang", low_digest) == ON1X_SDK_DUPLICATE);
    CHECK(authorize(unsafe.get(), "driver-two", ON1X_SDK_TOOL_LINKER,
                    "/bin/ld", high_digest) == ON1X_SDK_OK);
    CHECK(authorize(unsafe.get(), "driver-three", ON1X_SDK_TOOL_ARCHIVER,
                    "/bin/ar", tie_digest) == ON1X_SDK_LIMIT);

    CHECK(add_provider(owner.get(), "tool.zero", x64.get(), "zero", "1",
                       0, ON1X_SDK_TOOL_LINKER, zero) ==
          ON1X_SDK_INVALID_ARGUMENT);
    On1xSDK_ToolchainProvider short_provider{};
    short_provider.abi_version = ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION;
    short_provider.struct_size = 8;
    CHECK(on1x_sdk_toolchain_provider_register(owner.get(), &short_provider) ==
          ON1X_SDK_ABI_MISMATCH);

    CHECK(on1x_sdk_toolchain_resolve(nullptr, host.get(), &query, &raw) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_tool_resolution_view(nullptr, &view) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(view == nullptr);

    auto negotiated_registry = registry();
    auto negotiated_resolver = resolver(1);
    CHECK(add_provider(negotiated_registry.get(), "tool.short", x64.get(),
                       "clang-short", "21.0", 0,
                       ON1X_SDK_TOOL_C_COMPILER, low_digest) == ON1X_SDK_OK);
    CHECK(authorize(negotiated_resolver.get(), "clang-short",
                    ON1X_SDK_TOOL_C_COMPILER, "/sdk/bin/clang",
                    low_digest) == ON1X_SDK_OK);
    auto negotiated_request = request(x64.get());
    negotiated_request.struct_size =
        offsetof(On1xSDK_ToolchainRequest, provider_name);
    negotiated_request.provider_name = reinterpret_cast<const char *>(1);
    negotiated_request.provider_name_size = 99;
    CHECK(on1x_sdk_toolchain_resolve(
              negotiated_registry.get(), negotiated_resolver.get(),
              &negotiated_request, &raw) == ON1X_SDK_OK);
    Resolution negotiated(raw, on1x_sdk_tool_resolution_destroy);
    CHECK(on1x_sdk_tool_resolution_view(negotiated.get(), &view) ==
          ON1X_SDK_OK);
    CHECK(std::string(view->provider_name, view->provider_name_size) ==
          "tool.short");

    /* A resolution owns its path/identity and is relocatable from its sources. */
    owner.reset();
    host.reset();
    x64.reset();
    CHECK(on1x_sdk_tool_resolution_view(high.get(), &view) == ON1X_SDK_OK);
    CHECK(std::string(view->absolute_path, view->absolute_path_size) ==
          "/relocated/toolset/bin/clang");
}

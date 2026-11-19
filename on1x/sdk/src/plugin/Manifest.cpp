#include "On1xSDK-PluginManifest.h"
#include "On1xSDK-PluginDependency.h"

#include "../core/Wire.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t manifest_magic = UINT32_C(0x4f31584d); /* O1XM */
constexpr std::size_t fixed_wire_size =
    4 + 2 + 2 + 4 + 4 + 4 + 4 + 4 +
    ON1X_SDK_BUDGET_KIND_COUNT * 8 + 8 + 8 + 8 + 4 + 4 +
    4 + 4 + 4 + 32;

struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
};

bool parse_component(
    std::string_view text, std::size_t &offset, std::uint32_t &out
) noexcept {
    if (offset >= text.size() || text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    const auto begin = offset;
    std::uint32_t value = 0;
    do {
        const auto digit = static_cast<std::uint32_t>(text[offset] - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        ++offset;
    } while (offset < text.size() &&
             text[offset] >= '0' && text[offset] <= '9');
    if (offset - begin > 1 && text[begin] == '0') {
        return false;
    }
    out = value;
    return true;
}

bool parse_version(On1xSDK_StringView text) noexcept {
    if (!text.data || !text.size || text.size > ON1X_SDK_NAME_MAX) {
        return false;
    }
    std::string_view value{text.data, text.size};
    std::size_t offset = 0;
    Version parsed{};
    return parse_component(value, offset, parsed.major) &&
        offset < value.size() && value[offset++] == '.' &&
        parse_component(value, offset, parsed.minor) &&
        offset < value.size() && value[offset++] == '.' &&
        parse_component(value, offset, parsed.patch) &&
        offset == value.size();
}

bool known_profile(std::uint32_t profile) noexcept {
    return profile >= ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED &&
        profile <= ON1X_SDK_PLUGIN_PROFILE_WASM;
}

bool zero_digest(const std::uint8_t digest[32]) noexcept {
    std::uint8_t combined = 0;
    for (unsigned i = 0; i < 32; ++i) {
        combined |= digest[i];
    }
    return combined == 0;
}

On1xSDK_Status validate_resources(
    const On1xSDK_PluginResourceRequest &resources
) noexcept {
    constexpr std::uint64_t maximum_address_space =
        UINT64_C(1) << 40;
    if (!resources.address_space_bytes ||
        resources.address_space_bytes > maximum_address_space ||
        !resources.cpu_seconds || resources.cpu_seconds > 86400 ||
        !resources.deadline_ms || resources.deadline_ms > 86400000 ||
        !resources.max_open_handles ||
        resources.max_open_handles > (1u << 20) ||
        !resources.max_threads_or_children ||
        resources.max_threads_or_children > 1024) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status validate_facet(
    const On1xSDK_PluginFacet &facet
) noexcept {
    if (on1x_sdk_identifier_validate(facet.name) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(facet.input_schema) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(facet.output_kind) != ON1X_SDK_OK ||
        !facet.operation_version ||
        facet.required_validation_state < 1 ||
        facet.required_validation_state > 3) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

struct OwnedDependency {
    std::string name;
    std::string minimum;
    std::string maximum;
};

struct OwnedFacet {
    std::string name;
    std::string input_schema;
    std::string output_kind;
    std::uint32_t operation_version = 0;
    std::uint32_t validation_state = 0;
};

struct OwnedCapability {
    std::string name;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
    std::uint32_t flags = 0;
};

std::size_t string_wire_size(std::size_t size) {
    if (size > UINT32_MAX ||
        size > std::numeric_limits<std::size_t>::max() - 4) {
        throw std::length_error("manifest string size");
    }
    return 4 + size;
}

void put_string(std::uint8_t *&cursor, On1xSDK_StringView text) noexcept {
    on1x::sdk::wire::put32(
        cursor, static_cast<std::uint32_t>(text.size)
    );
    on1x::sdk::wire::put_bytes(cursor, text.data, text.size);
}

struct Reader {
    const std::uint8_t *cursor = nullptr;
    std::size_t remaining = 0;

    bool take16(std::uint16_t &out) noexcept {
        if (remaining < 2) {
            return false;
        }
        out = on1x::sdk::wire::get16(cursor);
        remaining -= 2;
        return true;
    }

    bool take32(std::uint32_t &out) noexcept {
        if (remaining < 4) {
            return false;
        }
        out = on1x::sdk::wire::get32(cursor);
        remaining -= 4;
        return true;
    }

    bool take64(std::uint64_t &out) noexcept {
        if (remaining < 8) {
            return false;
        }
        out = on1x::sdk::wire::get64(cursor);
        remaining -= 8;
        return true;
    }

    bool take_string(On1xSDK_StringView &out) noexcept {
        std::uint32_t size = 0;
        if (!take32(size) || size > remaining) {
            return false;
        }
        out = {
            reinterpret_cast<const char *>(cursor),
            static_cast<std::size_t>(size)
        };
        cursor += size;
        remaining -= size;
        return true;
    }

    bool take_bytes(void *out, std::size_t size) noexcept {
        if (size > remaining) {
            return false;
        }
        std::memcpy(out, cursor, size);
        cursor += size;
        remaining -= size;
        return true;
    }
};

} // namespace

struct On1xSDK_PluginManifestOwned {
    std::string name;
    std::string version;
    std::vector<OwnedDependency> owned_dependencies;
    std::vector<OwnedFacet> owned_facets;
    std::vector<OwnedCapability> owned_capabilities;
    std::vector<On1xSDK_PluginDependency> dependencies;
    std::vector<On1xSDK_PluginFacet> facets;
    std::vector<On1xSDK_CapabilityRequest> capabilities;
    On1xSDK_PluginManifest view{};

    void refresh() {
        dependencies.clear();
        facets.clear();
        capabilities.clear();
        dependencies.reserve(owned_dependencies.size());
        facets.reserve(owned_facets.size());
        capabilities.reserve(owned_capabilities.size());
        for (const auto &dependency : owned_dependencies) {
            dependencies.push_back({
                {dependency.name.data(), dependency.name.size()},
                {dependency.minimum.data(), dependency.minimum.size()},
                dependency.maximum.empty()
                    ? On1xSDK_StringView{nullptr, 0}
                    : On1xSDK_StringView{
                          dependency.maximum.data(),
                          dependency.maximum.size()}
            });
        }
        for (const auto &facet : owned_facets) {
            facets.push_back({
                {facet.name.data(), facet.name.size()},
                {facet.input_schema.data(), facet.input_schema.size()},
                {facet.output_kind.data(), facet.output_kind.size()},
                facet.operation_version, facet.validation_state
            });
        }
        for (const auto &capability : owned_capabilities) {
            capabilities.push_back({
                {capability.name.data(), capability.name.size()},
                capability.minimum, capability.maximum,
                capability.flags, 0
            });
        }
        view.name = {name.data(), name.size()};
        view.version = {version.data(), version.size()};
        view.dependencies = dependencies.data();
        view.dependency_count = dependencies.size();
        view.facets = facets.data();
        view.facet_count = facets.size();
        view.requested_capabilities = capabilities.data();
        view.requested_capability_count = capabilities.size();
    }
};

namespace {

On1xSDK_Status clone_manifest(
    const On1xSDK_PluginManifest &source,
    std::unique_ptr<On1xSDK_PluginManifestOwned> &out
) {
    auto owned = std::make_unique<On1xSDK_PluginManifestOwned>();
    owned->name.assign(source.name.data, source.name.size);
    owned->version.assign(source.version.data, source.version.size);
    owned->owned_dependencies.reserve(source.dependency_count);
    owned->owned_facets.reserve(source.facet_count);
    owned->owned_capabilities.reserve(
        source.requested_capability_count
    );
    for (std::size_t i = 0; i < source.dependency_count; ++i) {
        const auto &dependency = source.dependencies[i];
        owned->owned_dependencies.push_back({
            std::string(dependency.name.data, dependency.name.size),
            std::string(
                dependency.minimum_version.data,
                dependency.minimum_version.size
            ),
            dependency.maximum_version.size
                ? std::string(
                      dependency.maximum_version.data,
                      dependency.maximum_version.size)
                : std::string{}
        });
    }
    for (std::size_t i = 0; i < source.facet_count; ++i) {
        const auto &facet = source.facets[i];
        owned->owned_facets.push_back({
            std::string(facet.name.data, facet.name.size),
            std::string(
                facet.input_schema.data, facet.input_schema.size
            ),
            std::string(
                facet.output_kind.data, facet.output_kind.size
            ),
            facet.operation_version, facet.required_validation_state
        });
    }
    for (std::size_t i = 0;
         i < source.requested_capability_count; ++i) {
        const auto &capability = source.requested_capabilities[i];
        owned->owned_capabilities.push_back({
            std::string(capability.name.data, capability.name.size),
            capability.min_revision, capability.max_revision,
            capability.flags
        });
    }
    owned->view = source;
    owned->view.abi_version = ON1X_SDK_ABI_VERSION_V2;
    owned->view.struct_size = sizeof(On1xSDK_PluginManifest);
    owned->refresh();
    out = std::move(owned);
    return ON1X_SDK_OK;
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_plugin_manifest_validate(
    const On1xSDK_PluginManifest *manifest
) {
    const auto header = on1x_sdk_abi_header_validate(
        manifest, sizeof(On1xSDK_PluginManifest),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (header != ON1X_SDK_OK) {
        return header;
    }
    if (manifest->schema_major !=
            ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MAJOR ||
        manifest->schema_minor >
            ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MINOR ||
        !known_profile(manifest->execution_profile) ||
        on1x_sdk_identifier_validate(manifest->name) != ON1X_SDK_OK ||
        !parse_version(manifest->version) ||
        !manifest->minimum_sdk_abi ||
        manifest->minimum_sdk_abi > manifest->maximum_sdk_abi ||
        (!manifest->dependencies && manifest->dependency_count) ||
        (!manifest->facets && manifest->facet_count) ||
        (!manifest->requested_capabilities &&
         manifest->requested_capability_count) ||
        (manifest->has_image_sha256 != 0 &&
         manifest->has_image_sha256 != 1) ||
        (!manifest->has_image_sha256 &&
         !zero_digest(manifest->image_sha256))) {
        return manifest->schema_major !=
                   ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MAJOR
            ? ON1X_SDK_ABI_MISMATCH
            : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (manifest->dependency_count >
            ON1X_SDK_MAX_EXTENSION_DEPENDENCIES ||
        manifest->facet_count > ON1X_SDK_PLUGIN_MANIFEST_MAX_FACETS ||
        manifest->requested_capability_count >
            ON1X_SDK_MAX_CAPABILITIES) {
        return ON1X_SDK_LIMIT;
    }
    auto status = validate_resources(manifest->resources);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    for (std::size_t i = 0; i < manifest->dependency_count; ++i) {
        status = on1x_sdk_plugin_dependency_validate(
            manifest->dependencies + i
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    manifest->dependencies[i].name,
                    manifest->dependencies[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
    }
    for (std::size_t i = 0; i < manifest->facet_count; ++i) {
        status = validate_facet(manifest->facets[i]);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    manifest->facets[i].name,
                    manifest->facets[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
    }
    for (std::size_t i = 0;
         i < manifest->requested_capability_count; ++i) {
        status = on1x_sdk_capability_request_validate(
            manifest->requested_capabilities + i
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    manifest->requested_capabilities[i].name,
                    manifest->requested_capabilities[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_plugin_manifest_clone(
    const On1xSDK_PluginManifest *manifest,
    On1xSDK_PluginManifestOwned **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto status = on1x_sdk_plugin_manifest_validate(manifest);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    try {
        std::unique_ptr<On1xSDK_PluginManifestOwned> owned;
        clone_manifest(*manifest, owned);
        *out = owned.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_plugin_manifest_destroy(
    On1xSDK_PluginManifestOwned *manifest
) {
    delete manifest;
}

extern "C" const On1xSDK_PluginManifest *
on1x_sdk_plugin_manifest_view(
    const On1xSDK_PluginManifestOwned *manifest
) {
    return manifest ? &manifest->view : nullptr;
}

extern "C" On1xSDK_Status on1x_sdk_plugin_manifest_encode(
    const On1xSDK_PluginManifest *manifest,
    const On1xSDK_Allocator *allocator,
    On1xSDK_OwnedBytes *out
) {
    if (out) {
        *out = {};
    }
    if (!allocator || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = on1x_sdk_plugin_manifest_validate(manifest);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    try {
        std::size_t total = fixed_wire_size;
        const auto add = [&](std::size_t amount) {
            if (amount > ON1X_SDK_PLUGIN_MANIFEST_MAX_WIRE_BYTES - total) {
                throw std::length_error("manifest limit");
            }
            total += amount;
        };
        add(string_wire_size(manifest->name.size));
        add(string_wire_size(manifest->version.size));
        for (std::size_t i = 0; i < manifest->dependency_count; ++i) {
            const auto &dependency = manifest->dependencies[i];
            add(string_wire_size(dependency.name.size));
            add(string_wire_size(dependency.minimum_version.size));
            add(string_wire_size(dependency.maximum_version.size));
        }
        for (std::size_t i = 0; i < manifest->facet_count; ++i) {
            const auto &facet = manifest->facets[i];
            add(string_wire_size(facet.name.size));
            add(string_wire_size(facet.input_schema.size));
            add(string_wire_size(facet.output_kind.size));
            add(8);
        }
        for (std::size_t i = 0;
             i < manifest->requested_capability_count; ++i) {
            const auto &capability =
                manifest->requested_capabilities[i];
            add(string_wire_size(capability.name.size));
            add(12);
        }

        void *memory = nullptr;
        status = on1x_sdk_allocate(
            allocator, total, alignof(std::max_align_t), &memory
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        auto *cursor = static_cast<std::uint8_t *>(memory);
        on1x::sdk::wire::put32(cursor, manifest_magic);
        on1x::sdk::wire::put16(cursor, manifest->schema_major);
        on1x::sdk::wire::put16(cursor, manifest->schema_minor);
        on1x::sdk::wire::put32(cursor, manifest->execution_profile);
        on1x::sdk::wire::put32(cursor, manifest->minimum_sdk_abi);
        on1x::sdk::wire::put32(cursor, manifest->maximum_sdk_abi);
        on1x::sdk::wire::put32(cursor, manifest->trust_claims);
        on1x::sdk::wire::put32(cursor, manifest->has_image_sha256);
        for (const auto limit : manifest->resources.budget_limits) {
            on1x::sdk::wire::put64(cursor, limit);
        }
        on1x::sdk::wire::put64(
            cursor, manifest->resources.address_space_bytes
        );
        on1x::sdk::wire::put64(
            cursor, manifest->resources.cpu_seconds
        );
        on1x::sdk::wire::put64(
            cursor, manifest->resources.deadline_ms
        );
        on1x::sdk::wire::put32(
            cursor, manifest->resources.max_open_handles
        );
        on1x::sdk::wire::put32(
            cursor, manifest->resources.max_threads_or_children
        );
        on1x::sdk::wire::put32(
            cursor, static_cast<std::uint32_t>(
                manifest->dependency_count)
        );
        on1x::sdk::wire::put32(
            cursor, static_cast<std::uint32_t>(manifest->facet_count)
        );
        on1x::sdk::wire::put32(
            cursor, static_cast<std::uint32_t>(
                manifest->requested_capability_count)
        );
        on1x::sdk::wire::put_bytes(
            cursor, manifest->image_sha256, 32
        );
        put_string(cursor, manifest->name);
        put_string(cursor, manifest->version);
        for (std::size_t i = 0; i < manifest->dependency_count; ++i) {
            const auto &dependency = manifest->dependencies[i];
            put_string(cursor, dependency.name);
            put_string(cursor, dependency.minimum_version);
            put_string(cursor, dependency.maximum_version);
        }
        for (std::size_t i = 0; i < manifest->facet_count; ++i) {
            const auto &facet = manifest->facets[i];
            put_string(cursor, facet.name);
            put_string(cursor, facet.input_schema);
            put_string(cursor, facet.output_kind);
            on1x::sdk::wire::put32(cursor, facet.operation_version);
            on1x::sdk::wire::put32(
                cursor, facet.required_validation_state
            );
        }
        for (std::size_t i = 0;
             i < manifest->requested_capability_count; ++i) {
            const auto &capability =
                manifest->requested_capabilities[i];
            put_string(cursor, capability.name);
            on1x::sdk::wire::put32(
                cursor, capability.min_revision
            );
            on1x::sdk::wire::put32(
                cursor, capability.max_revision
            );
            on1x::sdk::wire::put32(cursor, capability.flags);
        }
        out->data = static_cast<std::uint8_t *>(memory);
        out->size = total;
        out->capacity = total;
        out->allocator = allocator;
        return ON1X_SDK_OK;
    } catch (const std::length_error &) {
        return ON1X_SDK_LIMIT;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_plugin_manifest_decode(
    On1xSDK_ByteView wire, On1xSDK_PluginManifestOwned **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!out || (!wire.data && wire.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (wire.size > ON1X_SDK_PLUGIN_MANIFEST_MAX_WIRE_BYTES) {
        return ON1X_SDK_LIMIT;
    }
    if (wire.size < fixed_wire_size) {
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    try {
        Reader reader{wire.data, wire.size};
        std::uint32_t magic = 0;
        On1xSDK_PluginManifest decoded{};
        decoded.abi_version = ON1X_SDK_ABI_VERSION_V2;
        decoded.struct_size = sizeof decoded;
        std::uint32_t dependency_count = 0;
        std::uint32_t facet_count = 0;
        std::uint32_t capability_count = 0;
        if (!reader.take32(magic) ||
            !reader.take16(decoded.schema_major) ||
            !reader.take16(decoded.schema_minor) ||
            !reader.take32(decoded.execution_profile) ||
            !reader.take32(decoded.minimum_sdk_abi) ||
            !reader.take32(decoded.maximum_sdk_abi) ||
            !reader.take32(decoded.trust_claims) ||
            !reader.take32(decoded.has_image_sha256)) {
            return ON1X_SDK_PROTOCOL_ERROR;
        }
        for (auto &limit : decoded.resources.budget_limits) {
            if (!reader.take64(limit)) {
                return ON1X_SDK_PROTOCOL_ERROR;
            }
        }
        if (!reader.take64(decoded.resources.address_space_bytes) ||
            !reader.take64(decoded.resources.cpu_seconds) ||
            !reader.take64(decoded.resources.deadline_ms) ||
            !reader.take32(decoded.resources.max_open_handles) ||
            !reader.take32(
                decoded.resources.max_threads_or_children) ||
            !reader.take32(dependency_count) ||
            !reader.take32(facet_count) ||
            !reader.take32(capability_count) ||
            !reader.take_bytes(decoded.image_sha256, 32) ||
            !reader.take_string(decoded.name) ||
            !reader.take_string(decoded.version)) {
            return ON1X_SDK_PROTOCOL_ERROR;
        }
        if (magic != manifest_magic) {
            return ON1X_SDK_PROTOCOL_ERROR;
        }
        if (dependency_count > ON1X_SDK_MAX_EXTENSION_DEPENDENCIES ||
            facet_count > ON1X_SDK_PLUGIN_MANIFEST_MAX_FACETS ||
            capability_count > ON1X_SDK_MAX_CAPABILITIES) {
            return ON1X_SDK_LIMIT;
        }

        std::vector<On1xSDK_PluginDependency> dependencies(
            dependency_count
        );
        std::vector<On1xSDK_PluginFacet> facets(facet_count);
        std::vector<On1xSDK_CapabilityRequest> capabilities(
            capability_count
        );
        for (auto &dependency : dependencies) {
            if (!reader.take_string(dependency.name) ||
                !reader.take_string(dependency.minimum_version) ||
                !reader.take_string(dependency.maximum_version)) {
                return ON1X_SDK_PROTOCOL_ERROR;
            }
        }
        for (auto &facet : facets) {
            if (!reader.take_string(facet.name) ||
                !reader.take_string(facet.input_schema) ||
                !reader.take_string(facet.output_kind) ||
                !reader.take32(facet.operation_version) ||
                !reader.take32(facet.required_validation_state)) {
                return ON1X_SDK_PROTOCOL_ERROR;
            }
        }
        for (auto &capability : capabilities) {
            if (!reader.take_string(capability.name) ||
                !reader.take32(capability.min_revision) ||
                !reader.take32(capability.max_revision) ||
                !reader.take32(capability.flags)) {
                return ON1X_SDK_PROTOCOL_ERROR;
            }
        }
        if (reader.remaining) {
            return ON1X_SDK_PROTOCOL_ERROR;
        }
        decoded.dependencies = dependencies.data();
        decoded.dependency_count = dependencies.size();
        decoded.facets = facets.data();
        decoded.facet_count = facets.size();
        decoded.requested_capabilities = capabilities.data();
        decoded.requested_capability_count = capabilities.size();
        const auto status =
            on1x_sdk_plugin_manifest_validate(&decoded);
        if (status != ON1X_SDK_OK) {
            return status == ON1X_SDK_INVALID_ARGUMENT
                ? ON1X_SDK_PROTOCOL_ERROR : status;
        }
        std::unique_ptr<On1xSDK_PluginManifestOwned> owned;
        clone_manifest(decoded, owned);
        *out = owned.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

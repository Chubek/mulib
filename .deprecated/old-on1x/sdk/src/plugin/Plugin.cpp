#include "On1xSDK-Plugin.h"
#include "On1xSDK-PluginEvent.h"
#include "On1xSDK-PluginHost.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-PluginWorker.h"
#include "On1xSDK-Sandbox.h"
#include "On1xSDK-Status.h"

#include "mulib-seccomp.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t maximum_source_label_bytes = 4096;

struct Discovered {
    std::string source;
    std::unique_ptr<
        On1xSDK_PluginManifestOwned,
        decltype(&on1x_sdk_plugin_manifest_destroy)
    > manifest{nullptr, on1x_sdk_plugin_manifest_destroy};
};

struct ParsedVersion {
    std::uint32_t parts[3]{};
};

bool parse_version(
    On1xSDK_StringView input, ParsedVersion &out
) noexcept {
    if (!input.data || !input.size) {
        return false;
    }
    const std::string_view text{input.data, input.size};
    std::size_t offset = 0;
    ParsedVersion parsed{};
    for (unsigned part = 0; part < 3; ++part) {
        if (offset >= text.size() ||
            text[offset] < '0' || text[offset] > '9') {
            return false;
        }
        const auto begin = offset;
        std::uint32_t value = 0;
        do {
            const auto digit =
                static_cast<std::uint32_t>(text[offset] - '0');
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
        parsed.parts[part] = value;
        if (part != 2) {
            if (offset >= text.size() || text[offset++] != '.') {
                return false;
            }
        }
    }
    if (offset != text.size()) {
        return false;
    }
    out = parsed;
    return true;
}

int compare_version(
    On1xSDK_StringView left, On1xSDK_StringView right
) noexcept {
    ParsedVersion left_value{}, right_value{};
    if (!parse_version(left, left_value) ||
        !parse_version(right, right_value)) {
        return 0;
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (left_value.parts[i] != right_value.parts[i]) {
            return left_value.parts[i] < right_value.parts[i] ? -1 : 1;
        }
    }
    return 0;
}

On1xSDK_Status validate_output_header(
    const void *output, std::uint32_t &size
) noexcept {
    On1xSDK_ABIHeader header{};
    const auto status = on1x_sdk_abi_header_validate(
        output, sizeof(On1xSDK_ABIHeader),
        ON1X_SDK_ABI_VERSION_V2, &header
    );
    if (status == ON1X_SDK_OK) {
        size = header.struct_size;
    }
    return status;
}

template<class Type>
void write_output_header(
    Type *destination, const Type &source
) noexcept {
    destination->abi_version = source.abi_version;
    destination->struct_size = source.struct_size;
}

template<class Type, class Field>
void write_output_field(
    Type *destination, const Type &source, std::uint32_t caller_size,
    std::size_t offset, Field Type::*field
) noexcept {
    if (caller_size >= offset + sizeof(Field) &&
        offset + sizeof(Field) <= sizeof(Type)) {
        destination->*field = source.*field;
    }
}

void write_output(
    On1xSDK_SandboxSupport *destination,
    const On1xSDK_SandboxSupport &source,
    std::uint32_t caller_size
) noexcept {
    write_output_header(destination, source);
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_SandboxSupport, enforceable),
        &On1xSDK_SandboxSupport::enforceable
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_SandboxSupport, missing),
        &On1xSDK_SandboxSupport::missing
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_SandboxSupport, status),
        &On1xSDK_SandboxSupport::status
    );
}

void write_output(
    On1xSDK_PluginProfileSupport *destination,
    const On1xSDK_PluginProfileSupport &source,
    std::uint32_t caller_size
) noexcept {
    write_output_header(destination, source);
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginProfileSupport, profile),
        &On1xSDK_PluginProfileSupport::profile
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginProfileSupport, supported),
        &On1xSDK_PluginProfileSupport::supported
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginProfileSupport, reason),
        &On1xSDK_PluginProfileSupport::reason
    );
}

void write_output(
    On1xSDK_PluginWorkerFeatures *destination,
    const On1xSDK_PluginWorkerFeatures &source,
    std::uint32_t caller_size
) noexcept {
    write_output_header(destination, source);
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginWorkerFeatures, protocol_major),
        &On1xSDK_PluginWorkerFeatures::protocol_major
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginWorkerFeatures, isolated_native_available),
        &On1xSDK_PluginWorkerFeatures::isolated_native_available
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginWorkerFeatures, sealed_shared_memory_available),
        &On1xSDK_PluginWorkerFeatures::sealed_shared_memory_available
    );
    write_output_field(
        destination, source, caller_size,
        offsetof(On1xSDK_PluginWorkerFeatures, wasm_available),
        &On1xSDK_PluginWorkerFeatures::wasm_available
    );
}

std::uint32_t enforceable_sandbox_bits() noexcept {
    std::uint32_t bits =
        ON1X_SDK_SANDBOX_RLIMIT |
        ON1X_SDK_SANDBOX_CREDENTIALS;
#if defined(__linux__)
    bits |= ON1X_SDK_SANDBOX_NAMESPACE |
            ON1X_SDK_SANDBOX_FILESYSTEM |
            ON1X_SDK_SANDBOX_NETWORK;
    if (mulib_seccomp_supported()) {
        bits |= ON1X_SDK_SANDBOX_SECCOMP;
    }
#endif
    return bits;
}

} // namespace

struct On1xSDK_PluginDiscoveryResult {
    std::vector<Discovered> entries;
};

struct On1xSDK_PluginAuthorization {
    std::unique_ptr<
        On1xSDK_PluginManifestOwned,
        decltype(&on1x_sdk_plugin_manifest_destroy)
    > manifest{nullptr, on1x_sdk_plugin_manifest_destroy};
    On1xSDK_PluginTrustDecision decision{};
};

extern "C" On1xSDK_Status on1x_sdk_plugin_discover(
    const On1xSDK_PluginManifestCandidate *candidates,
    std::size_t candidate_count,
    On1xSDK_PluginDiscoveryResult **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!out || (!candidates && candidate_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (candidate_count > ON1X_SDK_PLUGIN_DISCOVERY_MAX_CANDIDATES) {
        return ON1X_SDK_LIMIT;
    }
    try {
        auto result = std::make_unique<On1xSDK_PluginDiscoveryResult>();
        result->entries.reserve(candidate_count);
        for (std::size_t i = 0; i < candidate_count; ++i) {
            const auto &candidate = candidates[i];
            auto status = on1x_sdk_string_validate_utf8(
                candidate.source_label, maximum_source_label_bytes
            );
            if (status != ON1X_SDK_OK ||
                !candidate.source_label.size ||
                std::memchr(
                    candidate.source_label.data, 0,
                    candidate.source_label.size)) {
                return status == ON1X_SDK_OK
                    ? ON1X_SDK_INVALID_ARGUMENT : status;
            }
            On1xSDK_PluginManifestOwned *manifest = nullptr;
            status = on1x_sdk_plugin_manifest_decode(
                candidate.manifest_wire, &manifest
            );
            if (status != ON1X_SDK_OK) {
                return status;
            }
            result->entries.push_back({
                std::string(
                    candidate.source_label.data,
                    candidate.source_label.size
                ),
                {manifest, on1x_sdk_plugin_manifest_destroy}
            });
        }
        std::sort(
            result->entries.begin(), result->entries.end(),
            [](const Discovered &left, const Discovered &right) {
                const auto *left_manifest =
                    on1x_sdk_plugin_manifest_view(left.manifest.get());
                const auto *right_manifest =
                    on1x_sdk_plugin_manifest_view(right.manifest.get());
                const std::string_view left_name{
                    left_manifest->name.data, left_manifest->name.size
                };
                const std::string_view right_name{
                    right_manifest->name.data, right_manifest->name.size
                };
                if (left_name != right_name) {
                    return left_name < right_name;
                }
                const auto compared = compare_version(
                    left_manifest->version, right_manifest->version
                );
                return compared != 0
                    ? compared < 0 : left.source < right.source;
            }
        );
        for (std::size_t i = 1; i < result->entries.size(); ++i) {
            const auto *left = on1x_sdk_plugin_manifest_view(
                result->entries[i - 1].manifest.get()
            );
            const auto *right = on1x_sdk_plugin_manifest_view(
                result->entries[i].manifest.get()
            );
            if (on1x_sdk_string_equal(left->name, right->name) &&
                on1x_sdk_string_equal(left->version, right->version)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        *out = result.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_plugin_discovery_destroy(
    On1xSDK_PluginDiscoveryResult *result
) {
    delete result;
}

extern "C" std::size_t on1x_sdk_plugin_discovery_count(
    const On1xSDK_PluginDiscoveryResult *result
) {
    return result ? result->entries.size() : 0;
}

extern "C" const On1xSDK_PluginManifest *
on1x_sdk_plugin_discovery_manifest(
    const On1xSDK_PluginDiscoveryResult *result, std::size_t index
) {
    return result && index < result->entries.size()
        ? on1x_sdk_plugin_manifest_view(
              result->entries[index].manifest.get())
        : nullptr;
}

extern "C" On1xSDK_StringView on1x_sdk_plugin_discovery_source(
    const On1xSDK_PluginDiscoveryResult *result, std::size_t index
) {
    if (!result || index >= result->entries.size()) {
        return {nullptr, 0};
    }
    const auto &source = result->entries[index].source;
    return {source.data(), source.size()};
}

extern "C" On1xSDK_Status on1x_sdk_plugin_authorize(
    const On1xSDK_PluginManifest *manifest,
    On1xSDK_ByteView image,
    On1xSDK_PluginTrustFn trust,
    void *trust_userdata,
    On1xSDK_PluginAuthorization **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!out || !trust || (!image.data && image.size) || !image.size) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (image.size > ON1X_SDK_PLUGIN_IMAGE_MAX_BYTES) {
        return ON1X_SDK_LIMIT;
    }
    auto status = on1x_sdk_plugin_manifest_validate(manifest);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (manifest->minimum_sdk_abi > ON1X_SDK_ABI_VERSION_V2 ||
        manifest->maximum_sdk_abi < ON1X_SDK_ABI_VERSION_V2) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    if (!manifest->has_image_sha256) {
        return ON1X_SDK_VERIFICATION_FAILED;
    }
    std::uint8_t digest[32]{};
    status = on1x_sdk_sha256(image, digest);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!on1x_sdk_digest_equal(digest, manifest->image_sha256)) {
        return ON1X_SDK_VERIFICATION_FAILED;
    }
    On1xSDK_PluginTrustDecision decision{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginTrustDecision),
        0, 0
    };
    try {
        status = trust(
            trust_userdata, manifest, {digest, sizeof digest}, &decision
        );
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
    if (!on1x_sdk_status_is_known(status)) {
        return ON1X_SDK_ERROR;
    }
    if (status != ON1X_SDK_OK) {
        return status;
    }
    status = on1x_sdk_abi_header_validate(
        &decision, sizeof(On1xSDK_PluginTrustDecision),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    constexpr std::uint32_t known_flags =
        ON1X_SDK_PLUGIN_TRUST_EXPLICIT_IMAGE |
        ON1X_SDK_PLUGIN_TRUST_REVIEWED_PROVIDER |
        ON1X_SDK_PLUGIN_TRUST_ALLOW_IN_PROCESS |
        ON1X_SDK_PLUGIN_TRUST_ALLOW_ISOLATED;
    if (status != ON1X_SDK_OK ||
        decision.granted_profile != manifest->execution_profile ||
        (decision.trust_flags & ~known_flags) ||
        !(decision.trust_flags &
          ON1X_SDK_PLUGIN_TRUST_EXPLICIT_IMAGE)) {
        return status != ON1X_SDK_OK
            ? status : ON1X_SDK_CAPABILITY_DENIED;
    }
    const bool in_process =
        decision.granted_profile ==
            ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED ||
        decision.granted_profile ==
            ON1X_SDK_PLUGIN_PROFILE_TRUSTED_DSO;
    if ((in_process &&
         !(decision.trust_flags &
           ON1X_SDK_PLUGIN_TRUST_ALLOW_IN_PROCESS)) ||
        (!in_process &&
         !(decision.trust_flags &
           ON1X_SDK_PLUGIN_TRUST_ALLOW_ISOLATED))) {
        return ON1X_SDK_CAPABILITY_DENIED;
    }
    On1xSDK_PluginProfileSupport support{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginProfileSupport),
        decision.granted_profile, 0, ON1X_SDK_UNSUPPORTED
    };
    status = on1x_sdk_plugin_profile_support(
        decision.granted_profile, &support
    );
    if (status != ON1X_SDK_OK || !support.supported) {
        return status == ON1X_SDK_OK ? support.reason : status;
    }
    try {
        auto authorization =
            std::make_unique<On1xSDK_PluginAuthorization>();
        On1xSDK_PluginManifestOwned *owned = nullptr;
        status = on1x_sdk_plugin_manifest_clone(manifest, &owned);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        authorization->manifest.reset(owned);
        authorization->decision = decision;
        *out = authorization.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_plugin_authorization_destroy(
    On1xSDK_PluginAuthorization *authorization
) {
    delete authorization;
}

extern "C" const On1xSDK_PluginManifest *
on1x_sdk_plugin_authorized_manifest(
    const On1xSDK_PluginAuthorization *authorization
) {
    return authorization
        ? on1x_sdk_plugin_manifest_view(
              authorization->manifest.get())
        : nullptr;
}

extern "C" On1xSDK_PluginTrustDecision
on1x_sdk_plugin_authorized_decision(
    const On1xSDK_PluginAuthorization *authorization
) {
    return authorization
        ? authorization->decision
        : On1xSDK_PluginTrustDecision{};
}

extern "C" On1xSDK_SandboxProfile
on1x_sdk_sandbox_profile_default(void) {
    return {
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_SandboxProfile),
        ON1X_SDK_SANDBOX_SECCOMP |
            ON1X_SDK_SANDBOX_RLIMIT |
            ON1X_SDK_SANDBOX_CREDENTIALS |
            ON1X_SDK_SANDBOX_NAMESPACE |
            ON1X_SDK_SANDBOX_FILESYSTEM |
            ON1X_SDK_SANDBOX_NETWORK,
        0,
        UINT64_C(256) * 1024 * 1024,
        UINT64_C(8) * 1024 * 1024,
        3,
        5000,
        3000,
        1,
        8,
        ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES,
        256
    };
}

extern "C" On1xSDK_Status on1x_sdk_sandbox_profile_validate(
    const On1xSDK_SandboxProfile *profile
) {
    const auto status = on1x_sdk_abi_header_validate(
        profile, sizeof(On1xSDK_SandboxProfile),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    constexpr std::uint32_t known =
        ON1X_SDK_SANDBOX_SECCOMP |
        ON1X_SDK_SANDBOX_RLIMIT |
        ON1X_SDK_SANDBOX_CREDENTIALS |
        ON1X_SDK_SANDBOX_NAMESPACE |
        ON1X_SDK_SANDBOX_CGROUP |
        ON1X_SDK_SANDBOX_FILESYSTEM |
        ON1X_SDK_SANDBOX_NETWORK;
    constexpr std::uint32_t required_baseline =
        ON1X_SDK_SANDBOX_SECCOMP |
        ON1X_SDK_SANDBOX_RLIMIT |
        ON1X_SDK_SANDBOX_CREDENTIALS |
        ON1X_SDK_SANDBOX_NAMESPACE |
        ON1X_SDK_SANDBOX_FILESYSTEM |
        ON1X_SDK_SANDBOX_NETWORK;
    if (profile->reserved ||
        (profile->required_enforcement & ~known) ||
        (profile->required_enforcement & required_baseline) !=
            required_baseline ||
        profile->address_space_bytes < UINT64_C(16) * 1024 * 1024 ||
        profile->address_space_bytes > (UINT64_C(1) << 40) ||
        profile->stack_bytes < 65536 ||
        profile->stack_bytes > profile->address_space_bytes ||
        !profile->cpu_seconds || profile->cpu_seconds > 86400 ||
        !profile->wall_deadline_ms ||
        profile->wall_deadline_ms > 86400000 ||
        !profile->startup_deadline_ms ||
        profile->startup_deadline_ms > 600000 ||
        profile->max_processes != 1 ||
        profile->max_open_files < 4 ||
        profile->max_open_files > 64 ||
        profile->max_control_frame_bytes <
            ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE ||
        profile->max_control_frame_bytes >
            ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES ||
        !profile->max_broker_requests) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_sandbox_support(
    const On1xSDK_SandboxProfile *profile,
    On1xSDK_SandboxSupport *out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::uint32_t caller_size = 0;
    auto status = validate_output_header(out, caller_size);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    status = on1x_sdk_sandbox_profile_validate(profile);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    const auto enforceable = enforceable_sandbox_bits();
    const auto missing =
        profile->required_enforcement & ~enforceable;
    const On1xSDK_SandboxSupport value{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_SandboxSupport),
        enforceable, missing,
        missing ? ON1X_SDK_UNSUPPORTED : ON1X_SDK_OK
    };
    write_output(out, value, caller_size);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_plugin_profile_support(
    std::uint32_t profile, On1xSDK_PluginProfileSupport *out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::uint32_t caller_size = 0;
    auto status = validate_output_header(out, caller_size);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    On1xSDK_PluginProfileSupport value{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginProfileSupport),
        profile, 0, ON1X_SDK_UNSUPPORTED
    };
    switch (profile) {
    case ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED:
    case ON1X_SDK_PLUGIN_PROFILE_TRUSTED_DSO:
        value.supported = 1;
        value.reason = ON1X_SDK_OK;
        break;
    case ON1X_SDK_PLUGIN_PROFILE_ISOLATED_NATIVE:
    case ON1X_SDK_PLUGIN_PROFILE_ON1X_NATIVE: {
        const auto sandbox = on1x_sdk_sandbox_profile_default();
        On1xSDK_SandboxSupport support{
            ON1X_SDK_ABI_VERSION_V2,
            sizeof(On1xSDK_SandboxSupport), 0, 0, ON1X_SDK_UNSUPPORTED
        };
        status = on1x_sdk_sandbox_support(&sandbox, &support);
        value.supported =
            status == ON1X_SDK_OK && support.status == ON1X_SDK_OK;
        value.reason = status != ON1X_SDK_OK
            ? status : support.status;
        break;
    }
    case ON1X_SDK_PLUGIN_PROFILE_WASM:
        break;
    default:
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    write_output(out, value, caller_size);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_plugin_worker_features(
    On1xSDK_PluginWorkerFeatures *out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::uint32_t caller_size = 0;
    auto status = validate_output_header(out, caller_size);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    On1xSDK_PluginProfileSupport support{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginProfileSupport),
        ON1X_SDK_PLUGIN_PROFILE_ISOLATED_NATIVE,
        0, ON1X_SDK_UNSUPPORTED
    };
    status = on1x_sdk_plugin_profile_support(
        ON1X_SDK_PLUGIN_PROFILE_ISOLATED_NATIVE, &support
    );
    const On1xSDK_PluginWorkerFeatures value{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginWorkerFeatures),
        ON1X_SDK_PLUGIN_PROTOCOL_MAJOR,
        static_cast<std::uint32_t>(
            status == ON1X_SDK_OK && support.supported),
        0, /* System V mappings are immutable-by-policy, not sealed. */
        0
    };
    write_output(out, value, caller_size);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_plugin_event_emit(
    const On1xSDK_PluginEventSink *sink,
    const On1xSDK_PluginEvent *event
) {
    return on1x_sdk_event_emit(sink, event);
}

#include "On1xSDK-Plugin.h"
#include "On1xSDK-PluginEvent.h"
#include "On1xSDK-PluginHost.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-PluginWorker.h"
#include "On1xSDK-Sandbox.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

On1xSDK_StringView text(const char *value) {
    return {value, std::strlen(value)};
}

On1xSDK_Status trust_image(
    void *, const On1xSDK_PluginManifest *manifest,
    On1xSDK_ByteView digest, On1xSDK_PluginTrustDecision *decision
) {
    CHECK(digest.size == 32);
    decision->granted_profile = manifest->execution_profile;
    decision->trust_flags =
        ON1X_SDK_PLUGIN_TRUST_EXPLICIT_IMAGE |
        ON1X_SDK_PLUGIN_TRUST_ALLOW_IN_PROCESS;
    return ON1X_SDK_OK;
}

On1xSDK_Status deny_image(
    void *, const On1xSDK_PluginManifest *, On1xSDK_ByteView,
    On1xSDK_PluginTrustDecision *
) {
    return ON1X_SDK_CAPABILITY_DENIED;
}

On1xSDK_Status event_sink(void *opaque, const On1xSDK_Event *) {
    ++*static_cast<unsigned *>(opaque);
    return ON1X_SDK_OK;
}

} // namespace

int main() {
    const unsigned char image[] = "plugin-image";
    std::uint8_t digest[32]{};
    CHECK(on1x_sdk_sha256(
        {image, sizeof(image) - 1}, digest) == ON1X_SDK_OK);

    char mutable_name[] = "fixture.plugin";
    const On1xSDK_PluginDependency dependencies[]{
        {text("fixture.base"), text("1.0.0"), text("2.0.0")}
    };
    const On1xSDK_PluginFacet facets[]{
        {text("analyze"), text("mir.v1"), text("diagnostics"),
         1, 3}
    };
    const On1xSDK_CapabilityRequest capabilities[]{
        {text("diagnostics.emit"), 1, 1,
         ON1X_SDK_CAPABILITY_REQUIRED, 0}
    };
    On1xSDK_PluginManifest manifest{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_PluginManifest),
        ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MAJOR,
        ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MINOR,
        ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED,
        {mutable_name, std::strlen(mutable_name)},
        text("1.2.3"),
        ON1X_SDK_ABI_VERSION_V1,
        ON1X_SDK_ABI_VERSION_V2,
        dependencies, 1,
        facets, 1,
        capabilities, 1,
        {},
        {},
        1,
        0
    };
    for (auto &limit : manifest.resources.budget_limits)
        limit = UINT64_MAX;
    manifest.resources.address_space_bytes = UINT64_C(256) * 1024 * 1024;
    manifest.resources.cpu_seconds = 3;
    manifest.resources.deadline_ms = 5000;
    manifest.resources.max_open_handles = 16;
    manifest.resources.max_threads_or_children = 1;
    std::memcpy(manifest.image_sha256, digest, sizeof digest);
    CHECK(on1x_sdk_plugin_manifest_validate(&manifest) == ON1X_SDK_OK);

    On1xSDK_PluginManifestOwned *raw_clone = nullptr;
    CHECK(on1x_sdk_plugin_manifest_clone(
        &manifest, &raw_clone) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_PluginManifestOwned,
        decltype(&on1x_sdk_plugin_manifest_destroy)
    > clone(raw_clone, on1x_sdk_plugin_manifest_destroy);
    mutable_name[0] = 'X';
    const auto *cloned = on1x_sdk_plugin_manifest_view(clone.get());
    CHECK(cloned && on1x_sdk_string_equal(
        cloned->name, text("fixture.plugin")));

    On1xSDK_OwnedBytes wire{}, second_wire{};
    CHECK(on1x_sdk_plugin_manifest_encode(
        cloned, on1x_sdk_default_allocator(), &wire) == ON1X_SDK_OK);
    CHECK(on1x_sdk_plugin_manifest_encode(
        cloned, on1x_sdk_default_allocator(), &second_wire) ==
          ON1X_SDK_OK);
    CHECK(wire.size == second_wire.size &&
          std::memcmp(wire.data, second_wire.data, wire.size) == 0);

    On1xSDK_PluginManifestOwned *raw_decoded = nullptr;
    CHECK(on1x_sdk_plugin_manifest_decode(
        {wire.data, wire.size}, &raw_decoded) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_PluginManifestOwned,
        decltype(&on1x_sdk_plugin_manifest_destroy)
    > decoded(raw_decoded, on1x_sdk_plugin_manifest_destroy);
    CHECK(on1x_sdk_string_equal(
        on1x_sdk_plugin_manifest_view(decoded.get())->version,
        text("1.2.3")));

    std::vector<std::uint8_t> trailing(
        wire.data, wire.data + wire.size
    );
    trailing.push_back(0);
    CHECK(on1x_sdk_plugin_manifest_decode(
        {trailing.data(), trailing.size()}, &raw_decoded) ==
          ON1X_SDK_PROTOCOL_ERROR);
    std::vector<std::uint8_t> oversized(
        ON1X_SDK_PLUGIN_MANIFEST_MAX_WIRE_BYTES + 1
    );
    CHECK(on1x_sdk_plugin_manifest_decode(
        {oversized.data(), oversized.size()}, &raw_decoded) ==
          ON1X_SDK_LIMIT);

    auto version_two = *cloned;
    version_two.version = text("2.0.0");
    On1xSDK_OwnedBytes version_two_wire{};
    CHECK(on1x_sdk_plugin_manifest_encode(
        &version_two, on1x_sdk_default_allocator(),
        &version_two_wire) == ON1X_SDK_OK);
    const On1xSDK_PluginManifestCandidate candidates[]{
        {text("source-b"), {version_two_wire.data, version_two_wire.size}},
        {text("source-a"), {wire.data, wire.size}}
    };
    On1xSDK_PluginDiscoveryResult *raw_discovery = nullptr;
    CHECK(on1x_sdk_plugin_discover(
        candidates, 2, &raw_discovery) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_PluginDiscoveryResult,
        decltype(&on1x_sdk_plugin_discovery_destroy)
    > discovery(raw_discovery, on1x_sdk_plugin_discovery_destroy);
    CHECK(on1x_sdk_plugin_discovery_count(discovery.get()) == 2);
    CHECK(on1x_sdk_string_equal(
        on1x_sdk_plugin_discovery_manifest(
            discovery.get(), 0)->version,
        text("1.2.3")));
    const On1xSDK_PluginManifestCandidate duplicates[]{
        candidates[1], candidates[1]
    };
    CHECK(on1x_sdk_plugin_discover(
        duplicates, 2, &raw_discovery) == ON1X_SDK_DUPLICATE);

    On1xSDK_PluginAuthorization *raw_authorization = nullptr;
    CHECK(on1x_sdk_plugin_authorize(
        cloned, {image, sizeof(image) - 1},
        trust_image, nullptr, &raw_authorization) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_PluginAuthorization,
        decltype(&on1x_sdk_plugin_authorization_destroy)
    > authorization(
        raw_authorization, on1x_sdk_plugin_authorization_destroy
    );
    CHECK(on1x_sdk_plugin_authorized_decision(
        authorization.get()).granted_profile ==
          ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED);
    unsigned char changed_image[sizeof image];
    std::memcpy(changed_image, image, sizeof image);
    changed_image[0] ^= 1;
    CHECK(on1x_sdk_plugin_authorize(
        cloned, {changed_image, sizeof(changed_image) - 1},
        trust_image, nullptr, &raw_authorization) ==
          ON1X_SDK_VERIFICATION_FAILED);
    CHECK(on1x_sdk_plugin_authorize(
        cloned, {image, sizeof(image) - 1},
        deny_image, nullptr, &raw_authorization) ==
          ON1X_SDK_CAPABILITY_DENIED);
    auto incompatible = *cloned;
    incompatible.minimum_sdk_abi = ON1X_SDK_ABI_VERSION_V2 + 1;
    incompatible.maximum_sdk_abi = ON1X_SDK_ABI_VERSION_V2 + 1;
    CHECK(on1x_sdk_plugin_manifest_validate(&incompatible) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_plugin_authorize(
        &incompatible, {image, sizeof(image) - 1},
        trust_image, nullptr, &raw_authorization) ==
          ON1X_SDK_ABI_MISMATCH);
    CHECK(raw_authorization == nullptr);

    const auto sandbox = on1x_sdk_sandbox_profile_default();
    CHECK(on1x_sdk_sandbox_profile_validate(&sandbox) == ON1X_SDK_OK);
    On1xSDK_SandboxSupport partial_sandbox_support;
    std::memset(
        &partial_sandbox_support, 0xa5,
        sizeof partial_sandbox_support
    );
    partial_sandbox_support.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_sandbox_support.struct_size =
        offsetof(On1xSDK_SandboxSupport, enforceable) + 1;
    CHECK(on1x_sdk_sandbox_support(
        &sandbox, &partial_sandbox_support) == ON1X_SDK_OK);
    CHECK(reinterpret_cast<const unsigned char *>(
              &partial_sandbox_support)
              [offsetof(On1xSDK_SandboxSupport, enforceable)] == 0xa5);

    On1xSDK_SandboxSupport sandbox_support{};
    sandbox_support.abi_version = ON1X_SDK_ABI_VERSION_V2;
    sandbox_support.struct_size = sizeof(On1xSDK_SandboxSupport);
    CHECK(on1x_sdk_sandbox_support(
        &sandbox, &sandbox_support) == ON1X_SDK_OK);
    CHECK((sandbox_support.enforceable & ON1X_SDK_SANDBOX_RLIMIT) != 0);
    On1xSDK_PluginProfileSupport partial_profile_support;
    std::memset(
        &partial_profile_support, 0xa5,
        sizeof partial_profile_support
    );
    partial_profile_support.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_profile_support.struct_size =
        offsetof(On1xSDK_PluginProfileSupport, profile) + 1;
    CHECK(on1x_sdk_plugin_profile_support(
        ON1X_SDK_PLUGIN_PROFILE_WASM, &partial_profile_support) ==
          ON1X_SDK_OK);
    CHECK(reinterpret_cast<const unsigned char *>(
              &partial_profile_support)
              [offsetof(On1xSDK_PluginProfileSupport, profile)] == 0xa5);

    On1xSDK_PluginProfileSupport profile_support{};
    profile_support.abi_version = ON1X_SDK_ABI_VERSION_V2;
    profile_support.struct_size = sizeof(On1xSDK_PluginProfileSupport);
    CHECK(on1x_sdk_plugin_profile_support(
        ON1X_SDK_PLUGIN_PROFILE_WASM, &profile_support) ==
          ON1X_SDK_OK);
    CHECK(!profile_support.supported &&
          profile_support.reason == ON1X_SDK_UNSUPPORTED);
    On1xSDK_PluginWorkerFeatures partial_features;
    std::memset(&partial_features, 0xa5, sizeof partial_features);
    partial_features.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_features.struct_size =
        offsetof(On1xSDK_PluginWorkerFeatures, protocol_major) + 1;
    CHECK(on1x_sdk_plugin_worker_features(&partial_features) ==
          ON1X_SDK_OK);
    CHECK(reinterpret_cast<const unsigned char *>(&partial_features)
              [offsetof(On1xSDK_PluginWorkerFeatures, protocol_major)] ==
          0xa5);

    On1xSDK_PluginWorkerFeatures features{};
    features.abi_version = ON1X_SDK_ABI_VERSION_V2;
    features.struct_size = sizeof(On1xSDK_PluginWorkerFeatures);
    CHECK(on1x_sdk_plugin_worker_features(&features) == ON1X_SDK_OK);
    CHECK(features.protocol_major == ON1X_SDK_PLUGIN_PROTOCOL_MAJOR);
    CHECK(features.sealed_shared_memory_available == 0);
    CHECK(features.wasm_available == 0);

    unsigned emitted = 0;
    const On1xSDK_PluginEventSink sink{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_PluginEventSink),
        event_sink, &emitted
    };
    const On1xSDK_PluginEvent event{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_PluginEvent),
        ON1X_SDK_EVENT_ACTIVE, 0, 1, text("fixture.plugin"),
        ON1X_SDK_OK
    };
    CHECK(on1x_sdk_plugin_event_emit(&sink, &event) == ON1X_SDK_OK);
    CHECK(emitted == 1);

    on1x_sdk_owned_bytes_release(&version_two_wire);
    on1x_sdk_owned_bytes_release(&second_wire);
    on1x_sdk_owned_bytes_release(&wire);
    return 0;
}

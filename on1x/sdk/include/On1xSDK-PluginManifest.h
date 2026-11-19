#ifndef ON1X_SDK_PLUGIN_MANIFEST_H
#define ON1X_SDK_PLUGIN_MANIFEST_H

#include "On1xSDK-Budget.h"
#include "On1xSDK-Capability.h"
#include "On1xSDK-Extension.h"
#include "On1xSDK-Provenance.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MAJOR 1u
#define ON1X_SDK_PLUGIN_MANIFEST_SCHEMA_MINOR 0u
#define ON1X_SDK_PLUGIN_MANIFEST_MAX_WIRE_BYTES (64u * 1024u)
#define ON1X_SDK_PLUGIN_MANIFEST_MAX_FACETS 64u
#define ON1X_SDK_PLUGIN_IMAGE_MAX_BYTES (16u * 1024u * 1024u)

typedef uint32_t On1xSDK_PluginProfile;
enum {
    ON1X_SDK_PLUGIN_PROFILE_STATIC_LINKED = 1,
    ON1X_SDK_PLUGIN_PROFILE_TRUSTED_DSO = 2,
    ON1X_SDK_PLUGIN_PROFILE_ISOLATED_NATIVE = 3,
    ON1X_SDK_PLUGIN_PROFILE_ON1X_NATIVE = 4,
    ON1X_SDK_PLUGIN_PROFILE_WASM = 5
};

typedef struct On1xSDK_PluginFacet {
    On1xSDK_StringView name;
    On1xSDK_StringView input_schema;
    On1xSDK_StringView output_kind;
    uint32_t operation_version;
    uint32_t required_validation_state;
} On1xSDK_PluginFacet;

typedef struct On1xSDK_PluginResourceRequest {
    uint64_t budget_limits[ON1X_SDK_BUDGET_KIND_COUNT];
    uint64_t address_space_bytes;
    uint64_t cpu_seconds;
    uint64_t deadline_ms;
    uint32_t max_open_handles;
    uint32_t max_threads_or_children;
} On1xSDK_PluginResourceRequest;

typedef struct On1xSDK_PluginManifest {
    uint32_t abi_version;
    uint32_t struct_size;
    uint16_t schema_major;
    uint16_t schema_minor;
    uint32_t execution_profile;
    On1xSDK_StringView name;
    On1xSDK_StringView version;
    uint32_t minimum_sdk_abi;
    uint32_t maximum_sdk_abi;
    const On1xSDK_PluginDependency *dependencies;
    size_t dependency_count;
    const On1xSDK_PluginFacet *facets;
    size_t facet_count;
    const On1xSDK_CapabilityRequest *requested_capabilities;
    size_t requested_capability_count;
    On1xSDK_PluginResourceRequest resources;
    uint8_t image_sha256[32];
    uint32_t has_image_sha256;
    uint32_t trust_claims; /* Untrusted claims; never grants authority. */
} On1xSDK_PluginManifest;

On1xSDK_Status on1x_sdk_plugin_manifest_validate(
    const On1xSDK_PluginManifest *manifest
);
On1xSDK_Status on1x_sdk_plugin_manifest_clone(
    const On1xSDK_PluginManifest *manifest,
    On1xSDK_PluginManifestOwned **out
);
void on1x_sdk_plugin_manifest_destroy(On1xSDK_PluginManifestOwned *manifest);
const On1xSDK_PluginManifest *on1x_sdk_plugin_manifest_view(
    const On1xSDK_PluginManifestOwned *manifest
);
On1xSDK_Status on1x_sdk_plugin_manifest_encode(
    const On1xSDK_PluginManifest *manifest,
    const On1xSDK_Allocator *allocator,
    On1xSDK_OwnedBytes *out
);
On1xSDK_Status on1x_sdk_plugin_manifest_decode(
    On1xSDK_ByteView wire,
    On1xSDK_PluginManifestOwned **out
);

#ifdef __cplusplus
}
#endif

#endif

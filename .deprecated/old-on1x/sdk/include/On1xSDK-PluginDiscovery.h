#ifndef ON1X_SDK_PLUGIN_DISCOVERY_H
#define ON1X_SDK_PLUGIN_DISCOVERY_H

#include "On1xSDK-PluginManifest.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PLUGIN_DISCOVERY_MAX_CANDIDATES 256u

typedef struct On1xSDK_PluginManifestCandidate {
    On1xSDK_StringView source_label; /* Inert provenance, never executed. */
    On1xSDK_ByteView manifest_wire;
} On1xSDK_PluginManifestCandidate;

/*
 * Parses and sorts declarative manifests only. It never opens, maps, resolves,
 * or executes a binary named by source_label or manifest text.
 */
On1xSDK_Status on1x_sdk_plugin_discover(
    const On1xSDK_PluginManifestCandidate *candidates,
    size_t candidate_count,
    On1xSDK_PluginDiscoveryResult **out
);
void on1x_sdk_plugin_discovery_destroy(On1xSDK_PluginDiscoveryResult *result);
size_t on1x_sdk_plugin_discovery_count(
    const On1xSDK_PluginDiscoveryResult *result
);
const On1xSDK_PluginManifest *on1x_sdk_plugin_discovery_manifest(
    const On1xSDK_PluginDiscoveryResult *result, size_t index
);
On1xSDK_StringView on1x_sdk_plugin_discovery_source(
    const On1xSDK_PluginDiscoveryResult *result, size_t index
);

#ifdef __cplusplus
}
#endif

#endif

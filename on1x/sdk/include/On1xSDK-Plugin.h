#ifndef ON1X_SDK_PLUGIN_H
#define ON1X_SDK_PLUGIN_H

#include "On1xSDK-PluginDiscovery.h"
#include "On1xSDK-PluginSignature.h"
#include "On1xSDK-PluginTrust.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Verifies the image digest and obtains an explicit host trust decision.
 * Authorization does not load or execute the image.
 */
On1xSDK_Status on1x_sdk_plugin_authorize(
    const On1xSDK_PluginManifest *manifest,
    On1xSDK_ByteView image,
    On1xSDK_PluginTrustFn trust,
    void *trust_userdata,
    On1xSDK_PluginAuthorization **out
);
void on1x_sdk_plugin_authorization_destroy(
    On1xSDK_PluginAuthorization *authorization
);
const On1xSDK_PluginManifest *on1x_sdk_plugin_authorized_manifest(
    const On1xSDK_PluginAuthorization *authorization
);
On1xSDK_PluginTrustDecision on1x_sdk_plugin_authorized_decision(
    const On1xSDK_PluginAuthorization *authorization
);

#ifdef __cplusplus
}
#endif

#endif

#ifndef ON1X_SDK_CAPABILITY_H
#define ON1X_SDK_CAPABILITY_H

#include "On1xSDK-String.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_CAPABILITIES 64u

typedef uint32_t On1xSDK_CapabilityFlags;
enum {
    ON1X_SDK_CAPABILITY_REQUIRED = 1u << 0
};

typedef struct On1xSDK_CapabilityRequest {
    On1xSDK_StringView name;
    uint32_t min_revision;
    uint32_t max_revision; /* zero means unbounded */
    uint32_t flags;
    uint32_t reserved;
} On1xSDK_CapabilityRequest;

typedef struct On1xSDK_CapabilityGrant {
    On1xSDK_StringView name;
    uint32_t revision;
    uint32_t flags;
} On1xSDK_CapabilityGrant;

On1xSDK_Status on1x_sdk_capability_request_validate(
    const On1xSDK_CapabilityRequest *request
);
On1xSDK_Status on1x_sdk_capability_grant_validate(
    const On1xSDK_CapabilityGrant *grant
);
On1xSDK_Status on1x_sdk_capabilities_negotiate(
    const On1xSDK_CapabilityRequest *requests,
    size_t request_count,
    const On1xSDK_CapabilityGrant *host_grants,
    size_t host_grant_count,
    On1xSDK_CapabilitySet **out
);
void on1x_sdk_capability_set_destroy(On1xSDK_CapabilitySet *set);
size_t on1x_sdk_capability_set_count(const On1xSDK_CapabilitySet *set);
On1xSDK_Status on1x_sdk_capability_set_at(
    const On1xSDK_CapabilitySet *set,
    size_t index,
    On1xSDK_CapabilityGrant *out
);
int on1x_sdk_capability_set_contains(
    const On1xSDK_CapabilitySet *set,
    On1xSDK_StringView name,
    uint32_t minimum_revision
);

#ifdef __cplusplus
}
#endif

#endif

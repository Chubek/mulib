#ifndef ON1X_SDK_LIFECYCLE_H
#define ON1X_SDK_LIFECYCLE_H

#include "On1xSDK-ABI.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_LifecycleState;
enum {
    ON1X_SDK_LIFECYCLE_NEW = 0,
    ON1X_SDK_LIFECYCLE_VALIDATED = 1,
    ON1X_SDK_LIFECYCLE_INITIALIZING = 2,
    ON1X_SDK_LIFECYCLE_ACTIVE = 3,
    ON1X_SDK_LIFECYCLE_STOPPING = 4,
    ON1X_SDK_LIFECYCLE_CLOSED = 5,
    ON1X_SDK_LIFECYCLE_FAILED = 6
};

int on1x_sdk_lifecycle_transition_allowed(
    On1xSDK_LifecycleState from, On1xSDK_LifecycleState to
);
const char *on1x_sdk_lifecycle_state_name(On1xSDK_LifecycleState state);

#ifdef __cplusplus
}
#endif

#endif

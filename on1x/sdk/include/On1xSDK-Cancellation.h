#ifndef ON1X_SDK_CANCELLATION_H
#define ON1X_SDK_CANCELLATION_H

#include "On1xSDK-ABI.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_CancellationConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    /* Absolute monotonic nanoseconds; zero means no deadline. */
    uint64_t deadline_ns;
} On1xSDK_CancellationConfig;

uint64_t on1x_sdk_monotonic_now_ns(void);
On1xSDK_Status on1x_sdk_cancellation_create(
    const On1xSDK_CancellationConfig *config,
    On1xSDK_Cancellation **out
);
void on1x_sdk_cancellation_retain(On1xSDK_Cancellation *cancellation);
void on1x_sdk_cancellation_release(On1xSDK_Cancellation *cancellation);
On1xSDK_Status on1x_sdk_cancellation_cancel(On1xSDK_Cancellation *cancellation);
On1xSDK_Status on1x_sdk_cancellation_poll(
    const On1xSDK_Cancellation *cancellation
);

#ifdef __cplusplus
}
#endif

#endif

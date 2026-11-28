#ifndef ON1X_SDK_STATUS_H
#define ON1X_SDK_STATUS_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ABI-v1 callback adapters recognize only the frozen range 0..7. */
int on1x_sdk_status_is_v1(On1xSDK_Status status);
int on1x_sdk_status_is_known(On1xSDK_Status status);
On1xSDK_Status on1x_sdk_status_from_v1(uint32_t numeric_status);
const char *on1x_sdk_status_name(On1xSDK_Status status);

#ifdef __cplusplus
}
#endif

#endif

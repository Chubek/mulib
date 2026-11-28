/* Discovery validates declarative data and never activates an implementation.
 */
#ifndef ON1X_SDK_KERNEL_DISCOVERY_H
#define ON1X_SDK_KERNEL_DISCOVERY_H

#include "On1xSDK-Exotract.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status
on1x_sdk_kernel_discovery_profile_check(const On1xSDK_ExotractProfile *profile);

#ifdef __cplusplus
}
#endif

#endif

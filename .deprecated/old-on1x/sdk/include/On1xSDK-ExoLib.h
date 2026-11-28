/* ExoLib build profiles use the checked Exotract projection. */
#ifndef ON1X_SDK_EXOLIB_H
#define ON1X_SDK_EXOLIB_H

#include "On1xSDK-Exotract.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status
on1x_sdk_exolib_profile_check(const On1xSDK_ExotractProfile *profile);

#ifdef __cplusplus
}
#endif

#endif

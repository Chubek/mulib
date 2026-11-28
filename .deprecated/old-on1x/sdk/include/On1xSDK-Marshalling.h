/* General runtime marshalling is outside the admitted scalar/string profile. */
#ifndef ON1X_SDK_MARSHALLING_H
#define ON1X_SDK_MARSHALLING_H

#include "On1xSDK-FFI.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_foreign_marshalling_supported(void);

#ifdef __cplusplus
}
#endif

#endif

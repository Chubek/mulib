/* Dynamic foreign-module activation is not authorized by contract metadata. */
#ifndef ON1X_SDK_FOREIGN_MODULE_H
#define ON1X_SDK_FOREIGN_MODULE_H

#include "On1xSDK-FFI.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_foreign_module_loading_supported(void);

#ifdef __cplusplus
}
#endif

#endif

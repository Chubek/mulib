/* Retained/general foreign callbacks are deliberately outside the admitted
 * subset. */
#ifndef ON1X_SDK_FOREIGN_CALLBACK_H
#define ON1X_SDK_FOREIGN_CALLBACK_H

#include "On1xSDK-KernelBind.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns UNSUPPORTED; no callback ABI is guessed or synthesized. */
On1xSDK_Status on1x_sdk_foreign_callback_supported(void);

#ifdef __cplusplus
}
#endif

#endif

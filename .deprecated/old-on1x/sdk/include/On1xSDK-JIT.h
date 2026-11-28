/* Runtime native code loading is unsupported by the compiler extension SDK. */
#ifndef ON1X_SDK_JIT_H
#define ON1X_SDK_JIT_H

#include "On1xSDK-TargetMachine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_JIT On1xSDK_JIT;

/* Clears *out_jit and returns UNSUPPORTED for the current SDK profile. */
On1xSDK_Status on1x_sdk_jit_create(const On1xSDK_TargetMachine *machine,
                                   On1xSDK_JIT **out_jit);
void on1x_sdk_jit_destroy(On1xSDK_JIT *jit);

#ifdef __cplusplus
}
#endif

#endif

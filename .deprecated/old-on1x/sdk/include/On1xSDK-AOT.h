/* AOT native publication is unsupported in the current verified SDK subset. */
#ifndef ON1X_SDK_AOT_H
#define ON1X_SDK_AOT_H

#include "On1xSDK-Backend.h"
#include "On1xSDK-TargetMachine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clears *out_artifact and returns UNSUPPORTED for every valid request. */
On1xSDK_Status on1x_sdk_aot_compile(const On1xSDK_TargetMachine *machine,
                                    const On1xSDK_Program *program,
                                    On1xSDK_Artifact **out_artifact);

#ifdef __cplusplus
}
#endif

#endif

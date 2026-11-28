#ifndef ON1X_SDK_INCREMENTAL_H
#define ON1X_SDK_INCREMENTAL_H

#include "On1xSDK-Compilation.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Rebuilds from the compilation's live VFS and entry source. The old immutable
 * compilation remains valid. Cancellation, limits, or failure publish nothing.
 */
On1xSDK_Status on1x_sdk_compilation_refresh(
    const On1xSDK_Compilation *compilation,
    On1xSDK_Compilation **out_compilation);

#ifdef __cplusplus
}
#endif

#endif

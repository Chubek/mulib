#ifndef ON1X_SDK_ELABORATOR_H
#define ON1X_SDK_ELABORATOR_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Frontend source candidates do not bypass the authoritative semantic SDK.
 * Until that component supplies checked-HIR publication, this returns
 * ON1X_SDK_UNSUPPORTED and never mutates or upgrades the candidate.
 */
On1xSDK_Status on1x_sdk_elaborate(
    const On1xSDK_AST *candidate,
    void **out_checked_hir);

#ifdef __cplusplus
}
#endif

#endif

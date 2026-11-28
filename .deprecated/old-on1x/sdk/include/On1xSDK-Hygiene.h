#ifndef ON1X_SDK_HYGIENE_H
#define ON1X_SDK_HYGIENE_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_hygiene_context(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    uint64_t *out_context);
On1xSDK_Status on1x_sdk_hygiene_same_context(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode left,
    On1xSDK_ASTNode right,
    uint32_t *out_same);

#ifdef __cplusplus
}
#endif

#endif

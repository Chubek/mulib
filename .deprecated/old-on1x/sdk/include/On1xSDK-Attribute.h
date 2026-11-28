#ifndef ON1X_SDK_ATTRIBUTE_H
#define ON1X_SDK_ATTRIBUTE_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_ast_attribute_count(
    const On1xSDK_AST *ast,
    size_t *out_count);
On1xSDK_Status on1x_sdk_ast_attribute(
    const On1xSDK_AST *ast,
    size_t index,
    On1xSDK_ASTNode *out_node);

#ifdef __cplusplus
}
#endif

#endif

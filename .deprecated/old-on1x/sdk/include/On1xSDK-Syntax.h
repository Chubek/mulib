#ifndef ON1X_SDK_SYNTAX_H
#define ON1X_SDK_SYNTAX_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_syntax_is_category(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    On1xSDK_ASTNodeCategory category,
    uint32_t *out_matches);
On1xSDK_Status on1x_sdk_syntax_count_tag(
    const On1xSDK_AST *ast,
    const char *tag,
    size_t tag_size,
    size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif

#ifndef ON1X_SDK_MACRO_H
#define ON1X_SDK_MACRO_H

#include "On1xSDK-AST.h"
#include "On1xSDK-Options.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_ast_macro_count(
    const On1xSDK_AST *ast,
    size_t *out_count);

/*
 * Runs the canonical bounded hygienic macro expander transactionally. Failure
 * leaves ast unchanged and publishes no replacement.
 */
On1xSDK_Status on1x_sdk_macro_expand(
    const On1xSDK_AST *ast,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast);

#ifdef __cplusplus
}
#endif

#endif

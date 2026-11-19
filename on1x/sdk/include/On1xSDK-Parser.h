#ifndef ON1X_SDK_PARSER_H
#define ON1X_SDK_PARSER_H

#include "On1xSDK-AST.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-Token.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parses exactly the immutable revision represented by tokens. The result is a
 * structurally validated source candidate; it is not checked HIR.
 */
On1xSDK_Status on1x_sdk_parse(
    const On1xSDK_TokenStream *tokens,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast);

#ifdef __cplusplus
}
#endif

#endif

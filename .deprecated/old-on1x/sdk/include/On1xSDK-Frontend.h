#ifndef ON1X_SDK_FRONTEND_H
#define ON1X_SDK_FRONTEND_H

#include "On1xSDK-Lexer.h"
#include "On1xSDK-Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convenience operation equivalent to canonical lex followed by parse. */
On1xSDK_Status on1x_sdk_frontend_parse(
    const On1xSDK_SourceSnapshot *source,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast);

#ifdef __cplusplus
}
#endif

#endif

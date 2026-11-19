#ifndef ON1X_SDK_LEXER_H
#define ON1X_SDK_LEXER_H

#include "On1xSDK-Options.h"
#include "On1xSDK-Token.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runs the canonical On1x scanner selected by the build. Failure publishes no
 * stream and records one bounded diagnostic on the owning session.
 */
On1xSDK_Status on1x_sdk_lex(
    const On1xSDK_SourceRevision *source,
    const On1xSDK_Options *options,
    On1xSDK_TokenStream **out_tokens);

#ifdef __cplusplus
}
#endif

#endif

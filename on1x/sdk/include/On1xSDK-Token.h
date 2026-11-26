#ifndef ON1X_SDK_TOKEN_H
#define ON1X_SDK_TOKEN_H

#include "On1xSDK-SourceMap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_TokenKind;
enum {
    ON1X_SDK_TOKEN_EOF = 1,
    ON1X_SDK_TOKEN_IDENTIFIER = 2,
    ON1X_SDK_TOKEN_KEYWORD = 3,
    ON1X_SDK_TOKEN_INTEGER = 4,
    ON1X_SDK_TOKEN_FLOAT = 5,
    ON1X_SDK_TOKEN_STRING = 6,
    ON1X_SDK_TOKEN_CHARACTER = 7,
    ON1X_SDK_TOKEN_PUNCTUATION = 8,
    /* An unquoted spelling accepted by the custom operator recognizer.
     * Context may instead make it structural punctuation (e.g. nested >>);
     * this lexical classification does not imply a visible declaration. */
    ON1X_SDK_TOKEN_CUSTOM_OPERATOR = 9
};

typedef struct On1xSDK_TokenInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t token_id;
    uint32_t kind;
    uint32_t reserved;
    const char *text;
    size_t text_size;
    uint64_t source_id;
    uint64_t source_revision;
    size_t byte_begin;
    size_t byte_end;
    size_t line;
    size_t column;
} On1xSDK_TokenInfo;

On1xSDK_Status on1x_sdk_token_stream_retain(On1xSDK_TokenStream *tokens);
void on1x_sdk_token_stream_release(On1xSDK_TokenStream *tokens);
On1xSDK_Status on1x_sdk_token_stream_count(
    const On1xSDK_TokenStream *tokens,
    size_t *out_count);
On1xSDK_Status on1x_sdk_token_stream_get(
    const On1xSDK_TokenStream *tokens,
    size_t index,
    On1xSDK_TokenInfo *out_token);
On1xSDK_Status on1x_sdk_token_stream_source(
    const On1xSDK_TokenStream *tokens,
    On1xSDK_SourceSnapshot **out_revision);

#ifdef __cplusplus
}
#endif

#endif

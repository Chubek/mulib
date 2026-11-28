#ifndef ON1X_SDK_PIPELINE_H
#define ON1X_SDK_PIPELINE_H

#include "On1xSDK-Pass.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PIPELINE_VERSION 1u
#define ON1X_SDK_PIPELINE_MAX_PASSES 16u
#define ON1X_SDK_PIPELINE_MAX_TEXT 1024u
#define ON1X_SDK_PIPELINE_MAX_ROUNDS 64u

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t pipeline_version;
    uint32_t pass_count;
    uint32_t round_limit;
    uint32_t reserved;
    On1xSDK_PassKind passes[ON1X_SDK_PIPELINE_MAX_PASSES];
} On1xSDK_Pipeline;

/*
 * Grammar: ASCII pass names separated by commas, with optional whitespace.
 * Parsing executes no extension code and resolves only the four built-ins.
 */
On1xSDK_Status on1x_sdk_pipeline_parse(
    const char *text, size_t text_size, uint32_t round_limit,
    On1xSDK_Pipeline *out_pipeline);

#ifdef __cplusplus
}
#endif

#endif

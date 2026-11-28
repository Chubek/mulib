#ifndef ON1X_SDK_STAGING_H
#define ON1X_SDK_STAGING_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_StagingInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t quote_count;
    size_t splice_count;
    size_t lift_count;
    size_t run_count;
    size_t maximum_syntax_depth;
    uint32_t requires_semantic_lowering;
    uint32_t reserved;
} On1xSDK_StagingInfo;

On1xSDK_Status on1x_sdk_staging_inspect(
    const On1xSDK_AST *ast,
    On1xSDK_StagingInfo *out_info);

/*
 * Stage lowering requires canonical type inference owned by the semantic SDK.
 * This frontend-only entry point therefore rejects explicitly.
 */
On1xSDK_Status on1x_sdk_staging_lower(
    const On1xSDK_AST *ast,
    On1xSDK_AST **out_ast);

#ifdef __cplusplus
}
#endif

#endif

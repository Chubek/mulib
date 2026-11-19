#ifndef ON1X_SDK_AST_BUILDER_H
#define ON1X_SDK_AST_BUILDER_H

#include "On1xSDK-Options.h"
#include "On1xSDK-Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A builder owns copied candidate bytes. Commit succeeds only if the source is
 * still at expected_revision, then reparses through the canonical scanner and
 * parser before atomically publishing the new immutable source revision.
 */
On1xSDK_Status on1x_sdk_ast_builder_create(
    On1xSDK_Source *source,
    uint64_t expected_revision,
    const On1xSDK_Options *options,
    On1xSDK_ASTBuilder **out_builder);
On1xSDK_Status on1x_sdk_ast_builder_set_source(
    On1xSDK_ASTBuilder *builder,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding);
On1xSDK_Status on1x_sdk_ast_builder_validate(
    On1xSDK_ASTBuilder *builder,
    On1xSDK_AST **out_candidate);
On1xSDK_Status on1x_sdk_ast_builder_commit(
    On1xSDK_ASTBuilder *builder,
    On1xSDK_SourceRevision **out_revision,
    On1xSDK_AST **out_ast);
void on1x_sdk_ast_builder_destroy(On1xSDK_ASTBuilder *builder);

#ifdef __cplusplus
}
#endif

#endif

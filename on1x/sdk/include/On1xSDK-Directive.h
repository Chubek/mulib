#ifndef ON1X_SDK_DIRECTIVE_H
#define ON1X_SDK_DIRECTIVE_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_DirectiveKind;
enum {
    ON1X_SDK_DIRECTIVE_FEATURE = 1,
    ON1X_SDK_DIRECTIVE_SCRUTINY = 2,
    ON1X_SDK_DIRECTIVE_PPX = 3,
    ON1X_SDK_DIRECTIVE_EXTENSION = 4
};

typedef struct On1xSDK_DirectiveInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t kind;
    uint32_t reserved;
    On1xSDK_ASTNode node;
    const char *name;
    size_t name_size;
    const char *value;
    size_t value_size;
} On1xSDK_DirectiveInfo;

On1xSDK_Status on1x_sdk_ast_directive_count(
    const On1xSDK_AST *ast,
    size_t *out_count);
On1xSDK_Status on1x_sdk_ast_directive(
    const On1xSDK_AST *ast,
    size_t index,
    On1xSDK_DirectiveInfo *out_directive);

#ifdef __cplusplus
}
#endif

#endif

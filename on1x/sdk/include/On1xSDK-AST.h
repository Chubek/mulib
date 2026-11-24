#ifndef ON1X_SDK_AST_H
#define ON1X_SDK_AST_H

#include "On1xSDK-SourceMap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t On1xSDK_ASTNode;

typedef uint32_t On1xSDK_ASTValidationState;
enum {
    ON1X_SDK_AST_PARSED_CANDIDATE = 1,
    ON1X_SDK_AST_MACRO_EXPANDED_CANDIDATE = 2,
    ON1X_SDK_AST_PPX_READMITTED_CANDIDATE = 3
};

typedef uint32_t On1xSDK_ASTNodeCategory;
enum {
    ON1X_SDK_AST_UNIT = 1,
    ON1X_SDK_AST_DECLARATION = 2,
    ON1X_SDK_AST_PARAMETER = 3,
    ON1X_SDK_AST_STATEMENT = 4,
    ON1X_SDK_AST_EXPRESSION = 5,
    ON1X_SDK_AST_PATTERN = 6,
    ON1X_SDK_AST_TYPE_SYNTAX = 7,
    ON1X_SDK_AST_IMPORT = 8,
    ON1X_SDK_AST_MACRO = 9,
    ON1X_SDK_AST_MACRO_RULE = 10,
    ON1X_SDK_AST_DIRECTIVE = 11,
    ON1X_SDK_AST_ATTRIBUTE = 12,
    ON1X_SDK_AST_CONSTRAINT = 13,
    ON1X_SDK_AST_EXTENSION = 14
};

typedef uint32_t On1xSDK_ASTNodeFlags;
enum {
    ON1X_SDK_AST_FLAG_EXTERNAL = 1u << 0,
    ON1X_SDK_AST_FLAG_MUTABLE = 1u << 1,
    ON1X_SDK_AST_FLAG_GENERATED = 1u << 2,
    ON1X_SDK_AST_FLAG_REPEATED = 1u << 3,
    ON1X_SDK_AST_FLAG_OPEN = 1u << 4,
    ON1X_SDK_AST_FLAG_UNSAFE = 1u << 5,
    ON1X_SDK_AST_FLAG_INCLUSIVE = 1u << 6,
    /* A borrow expression, a borrow/ref parameter, or a ':=' store. */
    ON1X_SDK_AST_FLAG_REFERENCE = 1u << 7,
    ON1X_SDK_AST_FLAG_STATEMENT_FORM = 1u << 8,
    ON1X_SDK_AST_FLAG_SUM_FORM = 1u << 9,
    /* With REFERENCE: 'borrow mut' expression or 'ref' parameter. */
    ON1X_SDK_AST_FLAG_EXCLUSIVE = 1u << 10,
    /* A function declaration or lambda written with 'proc'. */
    ON1X_SDK_AST_FLAG_PROCEDURE = 1u << 11
};

typedef struct On1xSDK_ASTInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t schema_version;
    uint32_t validation_state;
    uint64_t revision;
    uint64_t root;
    size_t node_count;
    size_t source_count;
} On1xSDK_ASTInfo;

typedef struct On1xSDK_ASTNodeInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t node;
    uint32_t category;
    uint32_t flags;
    const char *tag;
    size_t tag_size;
    const char *text;
    size_t text_size;
    uint64_t hygiene_context;
    uint64_t source_id;
    uint64_t source_revision;
    const char *path;
    size_t path_size;
    size_t byte_begin;
    size_t byte_end;
    size_t line;
    size_t column;
    size_t child_count;
} On1xSDK_ASTNodeInfo;

On1xSDK_Status on1x_sdk_ast_retain(On1xSDK_AST *ast);
void on1x_sdk_ast_release(On1xSDK_AST *ast);
On1xSDK_Status on1x_sdk_ast_info(
    const On1xSDK_AST *ast,
    On1xSDK_ASTInfo *out_info);
On1xSDK_Status on1x_sdk_ast_node(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    On1xSDK_ASTNodeInfo *out_info);
On1xSDK_Status on1x_sdk_ast_child(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    size_t child_index,
    On1xSDK_ASTNode *out_child);
On1xSDK_Status on1x_sdk_ast_source(
    const On1xSDK_AST *ast,
    size_t source_index,
    On1xSDK_SourceSnapshot **out_revision);

#ifdef __cplusplus
}
#endif

#endif

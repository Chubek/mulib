#include "On1xSDK-AST.h"
#include "On1xSDK-ASTBuilder.h"
#include "On1xSDK-ASTVisitor.h"
#include "On1xSDK-Attribute.h"
#include "On1xSDK-Cache.h"
#include "On1xSDK-Compilation.h"
#include "On1xSDK-Compiler.h"
#include "On1xSDK-Dependency.h"
#include "On1xSDK-Directive.h"
#include "On1xSDK-Elaborator.h"
#include "On1xSDK-Frontend.h"
#include "On1xSDK-Grammar.h"
#include "On1xSDK-Hygiene.h"
#include "On1xSDK-Incremental.h"
#include "On1xSDK-Lexer.h"
#include "On1xSDK-Macro.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-PPX.h"
#include "On1xSDK-Parser.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-Source.h"
#include "On1xSDK-SourceMap.h"
#include "On1xSDK-Staging.h"
#include "On1xSDK-Syntax.h"
#include "On1xSDK-Token.h"
#include "On1xSDK-VFS.h"
#include "On1xSDK-Workspace.h"

#include <assert.h>
#include <string.h>

static On1xSDK_Status visit(
    void *userdata,
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    size_t depth) {
    size_t *count = (size_t *)userdata;
    On1xSDK_ASTNodeInfo info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
    (void)depth;
    assert(on1x_sdk_ast_node(ast, node, &info) == ON1X_SDK_OK);
    ++*count;
    return ON1X_SDK_OK;
}

int main(void) {
    static const char source_text[] =
        "fn answer()->i64{return 42;}fn main(){println(answer());}";
    On1xSDK_Session *session = NULL;
    On1xSDK_Options *options = NULL;
    On1xSDK_Source *source = NULL;
    On1xSDK_SourceRevision *revision = NULL;
    On1xSDK_TokenStream *tokens = NULL;
    On1xSDK_AST *ast = NULL;
    size_t token_count = 0;
    size_t visited = 0;

    On1xSDK_CompilerInfo compiler =
        ON1X_SDK_COMPILER_INIT(On1xSDK_CompilerInfo);
    assert(on1x_sdk_compiler_info(&compiler) == ON1X_SDK_OK);
    assert((compiler.capabilities & ON1X_SDK_COMPILER_CAP_CANONICAL_PARSER) != 0);

    assert(on1x_sdk_session_create(NULL, &session) == ON1X_SDK_OK);
    assert(on1x_sdk_options_create(session, &options) == ON1X_SDK_OK);
    assert(on1x_sdk_source_create(
        session, "virtual/c-api.on1x", strlen("virtual/c-api.on1x"),
        source_text, sizeof(source_text) - 1u, ON1X_SDK_SOURCE_UTF8,
        &source) == ON1X_SDK_OK);
    assert(on1x_sdk_source_snapshot(source, &revision) == ON1X_SDK_OK);
    assert(on1x_sdk_lex(revision, options, &tokens) == ON1X_SDK_OK);
    assert(on1x_sdk_token_stream_count(tokens, &token_count) == ON1X_SDK_OK);
    assert(token_count > 2u);

    {
        On1xSDK_TokenInfo token =
            ON1X_SDK_COMPILER_INIT(On1xSDK_TokenInfo);
        assert(on1x_sdk_token_stream_get(tokens, 0, &token) == ON1X_SDK_OK);
        assert(token.byte_begin == 0u && token.line == 1u && token.column == 1u);
    }

    assert(on1x_sdk_parse(tokens, options, &ast) == ON1X_SDK_OK);
    {
        On1xSDK_ASTInfo info = ON1X_SDK_COMPILER_INIT(On1xSDK_ASTInfo);
        assert(on1x_sdk_ast_info(ast, &info) == ON1X_SDK_OK);
        assert(info.node_count > 1u && info.source_count == 1u);
        assert(on1x_sdk_ast_visit(
            ast, info.root, ON1X_SDK_AST_PREORDER, visit, &visited) ==
            ON1X_SDK_OK);
        assert(visited == info.node_count);
    }

    assert(on1x_sdk_session_request_cancel(session) == ON1X_SDK_OK);
    {
        On1xSDK_TokenStream *cancelled = NULL;
        assert(on1x_sdk_lex(revision, options, &cancelled) == ON1X_SDK_BUSY);
        assert(cancelled == NULL);
        assert(on1x_sdk_session_last_condition(session) ==
               ON1X_SDK_COMPILER_CANCELLED);
    }
    assert(on1x_sdk_session_reset_cancel(session) == ON1X_SDK_OK);

    on1x_sdk_ast_release(ast);
    on1x_sdk_token_stream_release(tokens);
    on1x_sdk_source_revision_release(revision);
    on1x_sdk_source_release(source);
    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);
    return 0;
}

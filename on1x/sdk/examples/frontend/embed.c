#include "On1xSDK-Frontend.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-Source.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char program[] = "fn main(){println(42);}";
    On1xSDK_Session *session = NULL;
    On1xSDK_Options *options = NULL;
    On1xSDK_Source *source = NULL;
    On1xSDK_SourceSnapshot *revision = NULL;
    On1xSDK_AST *ast = NULL;
    On1xSDK_ASTInfo info = ON1X_SDK_COMPILER_INIT(On1xSDK_ASTInfo);
    int result = 1;

    if (on1x_sdk_session_create(NULL, &session) != ON1X_SDK_OK ||
        on1x_sdk_options_create(session, &options) != ON1X_SDK_OK ||
        on1x_sdk_source_create(
            session, "example.on1x", strlen("example.on1x"),
            program, sizeof(program) - 1u, ON1X_SDK_SOURCE_UTF8,
            &source) != ON1X_SDK_OK ||
        on1x_sdk_source_snapshot(source, &revision) != ON1X_SDK_OK ||
        on1x_sdk_frontend_parse(revision, options, &ast) != ON1X_SDK_OK ||
        on1x_sdk_ast_info(ast, &info) != ON1X_SDK_OK) {
        On1xSDK_Diagnostic diagnostic;
        if (session &&
            on1x_sdk_session_last_diagnostic(session, &diagnostic) ==
                ON1X_SDK_OK)
            fprintf(stderr, "%s: %s\n", diagnostic.code, diagnostic.message);
        goto done;
    }
    printf("parsed %zu canonical AST nodes\n", info.node_count);
    result = 0;

done:
    on1x_sdk_ast_release(ast);
    on1x_sdk_source_revision_release(revision);
    on1x_sdk_source_release(source);
    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);
    return result;
}

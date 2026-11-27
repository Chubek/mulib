#include <On1xSDK-Dependency.h>
#include <On1xSDK-Elaborator.h>
#include <On1xSDK-Frontend.h>
#include <On1xSDK-Incremental.h>
#include <On1xSDK-Session.h>
#include <On1xSDK-Workspace.h>

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned count;
    char code[128];
    size_t begin;
    size_t message_size;
} Diagnostics;

static void report(void *context, const On1xSDK_Diagnostic *diagnostic) {
    Diagnostics *record = context;
    ++record->count;
    assert(diagnostic->severity == ON1X_SDK_DIAGNOSTIC_ERROR);
    record->message_size = strlen(diagnostic->message);
    assert(record->message_size <= ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES);
    assert(strlen(diagnostic->code) < sizeof(record->code));
    strcpy(record->code, diagnostic->code);
    record->begin = diagnostic->byte_begin;
}

static void write_signature(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs(text, file) >= 0);
    assert(fclose(file) == 0);
}

static void dependency(const On1xSDK_Compilation *compilation, size_t index,
                       const char *suffix, uint32_t is_entry) {
    On1xSDK_DependencyInfo info = ON1X_SDK_COMPILER_INIT(On1xSDK_DependencyInfo);
    assert(on1x_sdk_compilation_dependency(compilation, index, &info) == ON1X_SDK_OK);
    const size_t size = strlen(suffix);
    assert(info.path_size >= size);
    assert(memcmp(info.path + info.path_size - size, suffix, size) == 0);
    assert(info.is_entry == is_entry && info.from_overlay == is_entry);
}

int main(int argc, char **argv) {
    assert(argc == 4);
    const char *project = argv[1], *stdlib = argv[2], *signature = argv[3];
    static const char filename[] = "entry.on1x";
    Diagnostics record = {0};
    On1xSDK_DiagnosticSink sink = {report, &record};
    On1xSDK_Session *session = NULL;
    On1xSDK_Options *options = NULL;
    On1xSDK_Workspace *workspace = NULL;
    On1xSDK_Source *source = NULL;
    On1xSDK_Compilation *compilation = NULL, *next = NULL;
    On1xSDK_AST *candidate = NULL;
    On1xSDK_SourceSnapshot *snapshot = NULL;
    On1xSDK_ASTInfo ast = ON1X_SDK_COMPILER_INIT(On1xSDK_ASTInfo);

    assert(on1x_sdk_session_create(&sink, &session) == ON1X_SDK_OK);
    assert(on1x_sdk_options_create(session, &options) == ON1X_SDK_OK);
    const On1xSDK_BooleanOption stdlib_option = ON1X_SDK_OPTION_FIRST_SEARCH_PATH_IS_STDLIB;
    uint32_t stdlib_selected = 2;
    assert(on1x_sdk_options_get_boolean(options, stdlib_option, &stdlib_selected) == ON1X_SDK_OK);
    assert(stdlib_selected == 0);
    assert(on1x_sdk_options_set_boolean(options, stdlib_option, 1) == ON1X_SDK_INVALID_ARGUMENT);
    assert(on1x_sdk_options_add_search_path(options, stdlib, strlen(stdlib)) == ON1X_SDK_OK);
    assert(on1x_sdk_workspace_create(session, project, strlen(project), options,
                                     &workspace) == ON1X_SDK_OK);
    assert(on1x_sdk_workspace_open(workspace, filename, sizeof(filename) - 1,
                                   ON1X_SDK_SOURCE_UTF8, &source) == ON1X_SDK_OK);
    assert(on1x_sdk_source_snapshot(source, &snapshot) == ON1X_SDK_OK);
    assert(on1x_sdk_frontend_parse(snapshot, options, &candidate) == ON1X_SDK_OK);
    assert(on1x_sdk_ast_info(candidate, &ast) == ON1X_SDK_OK);
    assert(ast.source_count == 1 && ast.validation_state == ON1X_SDK_AST_PARSED_CANDIDATE);
    on1x_sdk_ast_release(candidate);
    candidate = NULL;
    on1x_sdk_source_revision_release(snapshot);

    // The trusted root is neither inherited from the process environment nor
    // inferred from a generic library search path.
    assert(on1x_sdk_workspace_compile(workspace, filename, sizeof(filename) - 1,
                                      &compilation) == ON1X_SDK_VERIFICATION_FAILED);
    assert(!compilation);
    on1x_sdk_source_release(source);
    on1x_sdk_workspace_release(workspace);

    assert(on1x_sdk_options_set_boolean(options, ON1X_SDK_OPTION_CAPTURE_MODULE_GRAPH, 1) == ON1X_SDK_OK);
    const uint64_t before = on1x_sdk_options_revision(options);
    assert(on1x_sdk_options_set_boolean(options, stdlib_option, 1) == ON1X_SDK_OK);
    assert(on1x_sdk_options_revision(options) > before);
    assert(on1x_sdk_workspace_create(session, project, strlen(project), options,
                                     &workspace) == ON1X_SDK_OK);
    assert(on1x_sdk_options_clear_search_paths(options) == ON1X_SDK_OK);
    assert(on1x_sdk_options_get_boolean(options, stdlib_option, &stdlib_selected) == ON1X_SDK_OK);
    assert(stdlib_selected == 0);
    assert(on1x_sdk_workspace_open(workspace, filename, sizeof(filename) - 1,
                                   ON1X_SDK_SOURCE_UTF8, &source) == ON1X_SDK_OK);
    assert(on1x_sdk_workspace_compile(workspace, filename, sizeof(filename) - 1,
                                      &compilation) == ON1X_SDK_OK);
    size_t count = 0;
    assert(on1x_sdk_compilation_dependency_count(compilation, &count) == ON1X_SDK_OK);
    assert(count == 2);
    const char *graph = NULL;
    size_t graph_size = 0;
    assert(on1x_sdk_compilation_graph_json_v1(compilation, 1, &graph, &graph_size) == ON1X_SDK_OK);
    assert(graph && strlen(graph) == graph_size && strstr(graph, "on1x.module-dependency-graph/v1"));
    dependency(compilation, 0, "/Tools.on1xm", 0);
    dependency(compilation, 1, "/entry.on1x", 1);
    assert(on1x_sdk_compilation_ast(compilation, &candidate) == ON1X_SDK_OK);
    assert(on1x_sdk_ast_info(candidate, &ast) == ON1X_SDK_OK);
    assert(ast.source_count == count && ast.validation_state == ON1X_SDK_AST_PARSED_CANDIDATE);
    void *checked_hir = NULL;
    assert(on1x_sdk_elaborate(candidate, &checked_hir) == ON1X_SDK_UNSUPPORTED);
    assert(!checked_hir);

    write_signature(signature,
                    "signature ToolsAPI<T> { fn answer()->T; }; "
                    "module std.Tools : ToolsAPI<bool>;\n");
    record.count = 0;
    assert(on1x_sdk_compilation_refresh(compilation, &next) == ON1X_SDK_VERIFICATION_FAILED);
    assert(!next && record.count == 1);
    assert(strncmp(record.code, "ON1X-INTERFACE-", sizeof("ON1X-INTERFACE-") - 1) == 0);
    assert(record.begin != SIZE_MAX);
    assert(on1x_sdk_session_last_condition(session) == ON1X_SDK_COMPILER_VERIFICATION_FAILED);
    On1xSDK_Diagnostic last = {0};
    assert(on1x_sdk_session_last_diagnostic(session, &last) == ON1X_SDK_OK);
    assert(strcmp(last.code, record.code) == 0 && last.byte_begin == record.begin);
    assert(on1x_sdk_ast_info(candidate, &ast) == ON1X_SDK_OK);
    write_signature(signature, "abstract module std.Tools; type Token; fn answer()->i64;\n");
    record.count = 0;
    assert(on1x_sdk_compilation_refresh(compilation, &next) == ON1X_SDK_UNSUPPORTED);
    assert(!next && record.count == 1);
    assert(strcmp(record.code, "ON1X-SDK-OPAQUE-MODULE") == 0);

    write_signature(signature,
                    "signature ToolsAPI<T> { fn answer()->T; }; "
                    "module std.Tools : ToolsAPI<i64>;\n");
    assert(on1x_sdk_compilation_refresh(compilation, &next) == ON1X_SDK_OK);
    on1x_sdk_compilation_release(next);
    next = NULL;

    static const char cycle_path[] = "cycle.on1x";
    On1xSDK_Source *cycle = NULL;
    assert(on1x_sdk_workspace_open(workspace, cycle_path, sizeof(cycle_path) - 1,
                                   ON1X_SDK_SOURCE_UTF8, &cycle) == ON1X_SDK_OK);
    record.count = 0;
    assert(on1x_sdk_workspace_compile(workspace, cycle_path, sizeof(cycle_path) - 1,
                                      &next) == ON1X_SDK_VERIFICATION_FAILED);
    assert(!next && record.count == 1);
    assert(record.message_size == ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES);
    on1x_sdk_source_release(cycle);

    on1x_sdk_ast_release(candidate);
    on1x_sdk_compilation_release(compilation);
    on1x_sdk_source_release(source);
    on1x_sdk_workspace_release(workspace);

    static const char zero[] = "module local.Tools;\nshared elevate(0) fn helper(){}\n";
    assert(on1x_sdk_source_create(session, "zero.on1xm", sizeof("zero.on1xm") - 1,
                                  zero, sizeof(zero) - 1,
                                  ON1X_SDK_SOURCE_UTF8, &source) == ON1X_SDK_OK);
    assert(on1x_sdk_source_snapshot(source, &snapshot) == ON1X_SDK_OK);
    record.count = 0;
    assert(on1x_sdk_frontend_parse(snapshot, options, &candidate) == ON1X_SDK_VERIFICATION_FAILED);
    assert(!candidate && record.count == 1 && record.begin == (size_t)(strchr(zero, '0') - zero));
    assert(strcmp(record.code, "ON1X-MODULE-ELEVATION") == 0);
    on1x_sdk_source_revision_release(snapshot);
    on1x_sdk_source_release(source);

    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);
    return 0;
}

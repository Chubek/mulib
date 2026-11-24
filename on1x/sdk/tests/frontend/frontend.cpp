#include "On1xSDK-ASTBuilder.h"
#include "On1xSDK-ASTVisitor.h"
#include "On1xSDK-Cache.h"
#include "On1xSDK-Dependency.h"
#include "On1xSDK-Directive.h"
#include "On1xSDK-Frontend.h"
#include "On1xSDK-Incremental.h"
#include "On1xSDK-Macro.h"
#include "On1xSDK-PPX.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-SourceMap.h"
#include "On1xSDK-Staging.h"
#include "On1xSDK-Syntax.h"
#include "On1xSDK-Workspace.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Diagnostics {
    std::size_t count = 0;
    std::string code;
    std::string message;
    std::size_t begin = SIZE_MAX;
};

void diagnostic(void *context, const On1xSDK_Diagnostic *item) {
    auto &out = *static_cast<Diagnostics *>(context);
    ++out.count;
    out.code = item->code;
    out.message = item->message;
    out.begin = item->byte_begin;
}

std::string path_text(const fs::path &path) {
    return path.generic_string();
}

On1xSDK_Source *source_file(
    On1xSDK_Session *session,
    const fs::path &path,
    On1xSDK_SourceEncoding encoding = ON1X_SDK_SOURCE_UTF8) {
    auto text = path_text(path);
    On1xSDK_Source *source = nullptr;
    assert(on1x_sdk_source_from_file(
        session, text.data(), text.size(), encoding, &source) == ON1X_SDK_OK);
    return source;
}

On1xSDK_SourceSnapshot *snapshot(On1xSDK_Source *source) {
    On1xSDK_SourceSnapshot *revision = nullptr;
    assert(on1x_sdk_source_snapshot(source, &revision) == ON1X_SDK_OK);
    return revision;
}

On1xSDK_AST *parse(
    On1xSDK_Source *source,
    On1xSDK_Options *options) {
    auto *revision = snapshot(source);
    On1xSDK_AST *ast = nullptr;
    assert(on1x_sdk_frontend_parse(revision, options, &ast) == ON1X_SDK_OK);
    on1x_sdk_source_revision_release(revision);
    return ast;
}

On1xSDK_ASTInfo ast_info(On1xSDK_AST *ast) {
    On1xSDK_ASTInfo info = ON1X_SDK_COMPILER_INIT(On1xSDK_ASTInfo);
    assert(on1x_sdk_ast_info(ast, &info) == ON1X_SDK_OK);
    return info;
}

On1xSDK_ASTNode find_tag(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    std::string_view tag) {
    On1xSDK_ASTNodeInfo info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
    assert(on1x_sdk_ast_node(ast, node, &info) == ON1X_SDK_OK);
    if (std::string_view(info.tag, info.tag_size) == tag) return node;
    for (std::size_t i = 0; i < info.child_count; ++i) {
        On1xSDK_ASTNode child = 0;
        assert(on1x_sdk_ast_child(ast, node, i, &child) == ON1X_SDK_OK);
        if (const auto found = find_tag(ast, child, tag)) return found;
    }
    return 0;
}

On1xSDK_Status throwing_visitor(
    void *,
    const On1xSDK_AST *,
    On1xSDK_ASTNode,
    std::size_t) {
    throw 1;
}

void check_borrow_ast(On1xSDK_Session *session, On1xSDK_Options *options) {
    constexpr std::string_view path = "virtual/borrow.on1x";
    constexpr std::string_view text =
        "fn f(a:borrow i64,b:ref i64,c:i64){"
        "let shared=borrow a;let exclusive=borrow mut b;exclusive:=c;}";
    On1xSDK_Source *source = nullptr;
    assert(on1x_sdk_source_create(
        session, path.data(), path.size(), text.data(), text.size(),
        ON1X_SDK_SOURCE_UTF8, &source) == ON1X_SDK_OK);
    auto *ast = parse(source, options);
    auto root = ast_info(ast);
    assert(root.validation_state == ON1X_SDK_AST_PARSED_CANDIDATE);
    auto node = [ast](On1xSDK_ASTNode id) {
        On1xSDK_ASTNodeInfo info =
            ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
        assert(on1x_sdk_ast_node(ast, id, &info) == ON1X_SDK_OK);
        return info;
    };
    auto child = [ast, &node](On1xSDK_ASTNode parent, std::size_t index) {
        On1xSDK_ASTNode id = 0;
        assert(on1x_sdk_ast_child(ast, parent, index, &id) == ON1X_SDK_OK);
        return node(id);
    };
    constexpr auto reference = ON1X_SDK_AST_FLAG_REFERENCE;
    constexpr auto exclusive = reference | ON1X_SDK_AST_FLAG_EXCLUSIVE;
    auto check_parameter = [&](std::string_view tag, std::string_view name,
                               On1xSDK_ASTNodeFlags flags) {
        const auto parameter = node(find_tag(ast, root.root, tag));
        assert(parameter.category == ON1X_SDK_AST_PARAMETER);
        assert(parameter.flags == flags);
        assert(std::string_view(parameter.text, parameter.text_size) == name);
        const auto type = child(parameter.node, 0);
        assert(type.category == ON1X_SDK_AST_TYPE_SYNTAX);
        assert(std::string_view(type.tag, type.tag_size) == "i64_type");
    };
    check_parameter("shared_parameter", "a", reference);
    check_parameter("exclusive_parameter", "b", exclusive);
    check_parameter("parameter", "c", 0);

    std::size_t found = 0;
    const auto fn = node(find_tag(ast, root.root, "function"));
    for (std::size_t i = 0; i < fn.child_count; ++i) {
        const auto stmt = child(fn.node, i);
        if (std::string_view(stmt.tag, stmt.tag_size) != "let") continue;
        const bool shared = std::string_view(stmt.text, stmt.text_size) == "shared";
        const auto expr = child(stmt.node, 0);
        assert(expr.category == ON1X_SDK_AST_EXPRESSION && expr.child_count == 1);
        assert(std::string_view(expr.tag, expr.tag_size) == "borrow");
        assert(expr.flags == (shared ? reference : exclusive));
        assert(expr.byte_begin == text.find(shared ? "borrow a" : "borrow mut b"));
        const auto place = child(expr.node, 0);
        assert(std::string_view(place.tag, place.tag_size) == "identifier");
        assert(std::string_view(place.text, place.text_size) == (shared ? "a" : "b"));
        ++found;
    }
    assert(found == 2);
    assert(node(find_tag(ast, root.root, "assign")).flags == reference);
    on1x_sdk_ast_release(ast);
    on1x_sdk_source_release(source);
}

} // namespace

int main(int argc, char **argv) {
    assert(argc == 2);
    const fs::path fixtures = argv[1];
    const auto unique =
        "on1x-sdk-frontend-" + std::to_string(
            static_cast<unsigned long long>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path root = fs::temp_directory_path() / unique;
    fs::create_directories(root);
    fs::copy_file(fixtures / "main.on1x", root / "main.on1x");
    fs::copy_file(fixtures / "dep.on1x", root / "dep.on1x");

    Diagnostics diagnostics;
    On1xSDK_DiagnosticSink sink{diagnostic, &diagnostics};
    On1xSDK_Session *session = nullptr;
    On1xSDK_Options *options = nullptr;
    On1xSDK_VFS *vfs = nullptr;
    assert(on1x_sdk_session_create(&sink, &session) == ON1X_SDK_OK);
    assert(on1x_sdk_options_create(session, &options) == ON1X_SDK_OK);
    assert(on1x_sdk_vfs_create(session, &vfs) == ON1X_SDK_OK);
    check_borrow_ast(session, options);

    /* Invalid option combinations do not partially mutate the options. */
    const auto initial_options_revision = on1x_sdk_options_revision(options);
    std::uint64_t total_limit = 0;
    assert(on1x_sdk_options_get_limit(
        options, ON1X_SDK_OPTION_MAX_TOTAL_BYTES, &total_limit) ==
        ON1X_SDK_OK);
    assert(on1x_sdk_options_set_limit(
        options, ON1X_SDK_OPTION_MAX_TOTAL_BYTES, 1) ==
        ON1X_SDK_INVALID_ARGUMENT);
    std::uint64_t unchanged_total_limit = 0;
    assert(on1x_sdk_options_get_limit(
        options, ON1X_SDK_OPTION_MAX_TOTAL_BYTES,
        &unchanged_total_limit) == ON1X_SDK_OK);
    assert(unchanged_total_limit == total_limit);
    assert(on1x_sdk_options_revision(options) == initial_options_revision);

    auto *main_source = source_file(session, root / "main.on1x");
    auto *dep_source = source_file(session, root / "dep.on1x");
    assert(on1x_sdk_vfs_set(vfs, main_source) == ON1X_SDK_OK);
    assert(on1x_sdk_vfs_set(vfs, dep_source) == ON1X_SDK_OK);

    const std::string dep_overlay =
        "fn value() -> i64 {\n  return 42;\n}\n";
    auto *old_dep = snapshot(dep_source);
    On1xSDK_SourceInfo old_dep_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    assert(on1x_sdk_source_revision_info(old_dep, &old_dep_info) ==
           ON1X_SDK_OK);
    On1xSDK_SourceSnapshot *dep_revision = nullptr;
    assert(on1x_sdk_source_replace(
        dep_source, old_dep_info.revision, dep_overlay.data(),
        dep_overlay.size(), ON1X_SDK_SOURCE_UTF8, &dep_revision) ==
        ON1X_SDK_OK);
    uint32_t current = 1;
    assert(on1x_sdk_source_revision_is_current(old_dep, &current) ==
           ON1X_SDK_OK);
    assert(current == 0);
    on1x_sdk_source_revision_release(old_dep);

    /* Stale identity survives destruction of the mutable source handle. */
    const std::string ephemeral_text = "fn main(){}";
    On1xSDK_Source *ephemeral = nullptr;
    assert(on1x_sdk_source_create(
        session, "virtual/ephemeral.on1x",
        strlen("virtual/ephemeral.on1x"), ephemeral_text.data(),
        ephemeral_text.size(), ON1X_SDK_SOURCE_UTF8,
        &ephemeral) == ON1X_SDK_OK);
    auto *ephemeral_old = snapshot(ephemeral);
    On1xSDK_SourceInfo ephemeral_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    assert(on1x_sdk_source_revision_info(
        ephemeral_old, &ephemeral_info) == ON1X_SDK_OK);
    On1xSDK_SourceSnapshot *ephemeral_new = nullptr;
    assert(on1x_sdk_source_replace(
        ephemeral, ephemeral_info.revision, ephemeral_text.data(),
        ephemeral_text.size(), ON1X_SDK_SOURCE_UTF8,
        &ephemeral_new) == ON1X_SDK_OK);
    on1x_sdk_source_revision_release(ephemeral_new);
    on1x_sdk_source_release(ephemeral);
    assert(on1x_sdk_source_revision_is_current(
        ephemeral_old, &current) == ON1X_SDK_OK);
    assert(current == 0);
    on1x_sdk_source_revision_release(ephemeral_old);

    On1xSDK_Compilation *compilation = nullptr;
    assert(on1x_sdk_compilation_create(
        session, vfs, options, main_source, &compilation) == ON1X_SDK_OK);
    On1xSDK_CompilationInfo compilation_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_CompilationInfo);
    assert(on1x_sdk_compilation_info(compilation, &compilation_info) ==
           ON1X_SDK_OK);
    assert(compilation_info.dependency_count == 2);
    assert(compilation_info.is_current == 1);

    std::size_t dependencies = 0;
    assert(on1x_sdk_compilation_dependency_count(compilation, &dependencies) ==
           ON1X_SDK_OK);
    bool saw_entry = false;
    bool saw_overlay = false;
    for (std::size_t i = 0; i < dependencies; ++i) {
        On1xSDK_DependencyInfo dependency =
            ON1X_SDK_COMPILER_INIT(On1xSDK_DependencyInfo);
        assert(on1x_sdk_compilation_dependency(
            compilation, i, &dependency) == ON1X_SDK_OK);
        saw_entry |= dependency.is_entry != 0;
        saw_overlay |= dependency.from_overlay != 0;
        On1xSDK_SourceSnapshot *exact = nullptr;
        assert(on1x_sdk_compilation_dependency_source(
            compilation, i, &exact) == ON1X_SDK_OK);
        On1xSDK_SourceInfo exact_info =
            ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
        assert(on1x_sdk_source_revision_info(exact, &exact_info) ==
               ON1X_SDK_OK);
        assert(exact_info.byte_size == dependency.byte_size);
        on1x_sdk_source_revision_release(exact);
    }
    assert(saw_entry && saw_overlay);

    On1xSDK_AST *loaded_ast = nullptr;
    assert(on1x_sdk_compilation_ast(compilation, &loaded_ast) == ON1X_SDK_OK);
    const auto loaded_info = ast_info(loaded_ast);
    assert(loaded_info.source_count == 2);
    std::size_t functions = 0;
    assert(on1x_sdk_syntax_count_tag(
        loaded_ast, "function", 8, &functions) == ON1X_SDK_OK);
    assert(functions == 2);
    On1xSDK_ASTNodeInfo root_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
    assert(on1x_sdk_ast_node(loaded_ast, loaded_info.root, &root_info) ==
           ON1X_SDK_OK);
    assert(root_info.source_id != 0);

    /* Generic AST roles preserve cliche values and inherent receivers. */
    const std::string structured_text =
        "record Box{x:i64}"
        "cliche Mark{let code:i64;}"
        "impl Mark for Box{let code:i64=1;}"
        "impl Box{"
        "value answer(ref self):i64=42;"
        "fn peek(ref self)->i64{return 1;}"
        "}"
        "fn main(){let x=Mark<Box>::code;}";
    On1xSDK_Source *structured_source = nullptr;
    assert(on1x_sdk_source_create(
        session, "virtual/structured.on1x",
        strlen("virtual/structured.on1x"), structured_text.data(),
        structured_text.size(), ON1X_SDK_SOURCE_UTF8,
        &structured_source) == ON1X_SDK_OK);
    On1xSDK_AST *structured_ast = parse(structured_source, options);
    std::size_t role_count = 0;
    assert(on1x_sdk_syntax_count_tag(
        structured_ast, "receiver_borrow", strlen("receiver_borrow"),
        &role_count) == ON1X_SDK_OK);
    assert(role_count == 2);
    assert(on1x_sdk_syntax_count_tag(
        structured_ast, "cliche_member_name",
        strlen("cliche_member_name"), &role_count) == ON1X_SDK_OK);
    assert(role_count == 1);
    const auto structured_root = ast_info(structured_ast).root;
    const auto value_node = find_tag(
        structured_ast, structured_root,
        "cliche_implementation_value");
    assert(value_node != 0);
    On1xSDK_ASTNodeInfo value_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
    assert(on1x_sdk_ast_node(
        structured_ast, value_node, &value_info) == ON1X_SDK_OK);
    assert(std::string_view(value_info.text, value_info.text_size) == "code");
    assert(value_info.child_count == 2);
    on1x_sdk_ast_release(structured_ast);
    on1x_sdk_source_release(structured_source);

    /* Extensible outputs preserve a caller's smaller compatible size. */
    On1xSDK_ASTInfo prefix_only{};
    prefix_only.abi_version = ON1X_SDK_COMPILER_ABI_VERSION;
    prefix_only.struct_size = sizeof(std::uint32_t) * 2u;
    prefix_only.schema_version = UINT32_C(0xfeedbeef);
    assert(on1x_sdk_ast_info(loaded_ast, &prefix_only) == ON1X_SDK_OK);
    assert(prefix_only.struct_size == sizeof(std::uint32_t) * 2u);
    assert(prefix_only.schema_version == UINT32_C(0xfeedbeef));

    On1xSDK_ASTInfo short_prefix{};
    short_prefix.abi_version = ON1X_SDK_COMPILER_ABI_VERSION;
    short_prefix.struct_size = sizeof(std::uint32_t);
    assert(on1x_sdk_ast_info(loaded_ast, &short_prefix) ==
           ON1X_SDK_ABI_MISMATCH);

    assert(on1x_sdk_ast_visit(
        loaded_ast, loaded_info.root, ON1X_SDK_AST_PREORDER,
        throwing_visitor, nullptr) == ON1X_SDK_CALLBACK_FAILED);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_INVALID_STATE);

    /* Normalized VFS lookup must resolve an equivalent spelling. */
    const auto alias = path_text(root / "." / "sub" / ".." / "dep.on1x");
    On1xSDK_Source *looked_up = nullptr;
    assert(on1x_sdk_vfs_lookup(
        vfs, alias.data(), alias.size(), &looked_up) == ON1X_SDK_OK);
    on1x_sdk_source_release(looked_up);

    /* Exact PPX-v1 schema readmission is candidate-only and bounded. */
    std::size_t wire_size = 0;
    assert(on1x_sdk_ppx_encode(
        loaded_ast, nullptr, 0, &wire_size) == ON1X_SDK_OK);
    std::vector<char> wire(wire_size);
    assert(on1x_sdk_ppx_encode(
        loaded_ast, wire.data(), wire.size(), &wire_size) == ON1X_SDK_OK);
    On1xSDK_AST *readmitted = nullptr;
    assert(on1x_sdk_ppx_readmit(
        loaded_ast, ON1X_SDK_PPX_SCHEMA_FORMAT,
        strlen(ON1X_SDK_PPX_SCHEMA_FORMAT), ON1X_SDK_PPX_SCHEMA_VERSION,
        wire.data(), wire.size(), options, &readmitted) == ON1X_SDK_OK);
    assert(ast_info(readmitted).validation_state ==
           ON1X_SDK_AST_PPX_READMITTED_CANDIDATE);
    assert(on1x_sdk_ppx_validate_schema(
        "on1x.sexpr-ast/v2", strlen("on1x.sexpr-ast/v2"), 2) ==
        ON1X_SDK_ABI_MISMATCH);
    On1xSDK_AST *bad_wire = nullptr;
    assert(on1x_sdk_ppx_readmit(
        loaded_ast, ON1X_SDK_PPX_SCHEMA_FORMAT,
        strlen(ON1X_SDK_PPX_SCHEMA_FORMAT), ON1X_SDK_PPX_SCHEMA_VERSION,
        "()", 2, options, &bad_wire) == ON1X_SDK_VERIFICATION_FAILED);
    assert(bad_wire == nullptr);
    On1xSDK_PPXLimits ppx_limits =
        ON1X_SDK_COMPILER_INIT(On1xSDK_PPXLimits);
    assert(on1x_sdk_ppx_limits(&ppx_limits) == ON1X_SDK_OK);
    const char one_wire_byte = '(';
    assert(on1x_sdk_ppx_readmit(
        loaded_ast, ON1X_SDK_PPX_SCHEMA_FORMAT,
        strlen(ON1X_SDK_PPX_SCHEMA_FORMAT), ON1X_SDK_PPX_SCHEMA_VERSION,
        &one_wire_byte, ppx_limits.max_wire_bytes + 1u,
        options, &bad_wire) == ON1X_SDK_LIMIT);
    assert(bad_wire == nullptr);
    on1x_sdk_ast_release(readmitted);

    /* Source mutation invalidates the old compilation but not its snapshot. */
    auto *old_main = snapshot(main_source);
    On1xSDK_SourceInfo main_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    assert(on1x_sdk_source_revision_info(old_main, &main_info) == ON1X_SDK_OK);
    On1xSDK_SourceSpan shared_span{
        main_info.source_id, main_info.revision, 0, 2,
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    assert(on1x_sdk_source_validate_span(old_main, &shared_span) ==
           ON1X_SDK_OK);
    ++shared_span.revision;
    assert(on1x_sdk_source_validate_span(old_main, &shared_span) ==
           ON1X_SDK_STALE);
    --shared_span.revision;
    shared_span.line_begin = 1;
    assert(on1x_sdk_source_validate_span(old_main, &shared_span) ==
           ON1X_SDK_INVALID_ARGUMENT);
    const std::string changed_main =
        "# revision two\nimport dep;fn main(){println(dep.value());}";
    On1xSDK_SourceSnapshot *changed_revision = nullptr;
    assert(on1x_sdk_source_replace(
        main_source, main_info.revision, changed_main.data(),
        changed_main.size(), ON1X_SDK_SOURCE_UTF8, &changed_revision) ==
        ON1X_SDK_OK);
    assert(on1x_sdk_compilation_is_current(compilation, &current) ==
           ON1X_SDK_OK);
    assert(current == 0);
    On1xSDK_Compilation *refreshed = nullptr;
    assert(on1x_sdk_compilation_refresh(compilation, &refreshed) ==
           ON1X_SDK_OK);
    assert(on1x_sdk_compilation_is_current(refreshed, &current) ==
           ON1X_SDK_OK);
    assert(current == 1);
    on1x_sdk_source_revision_release(old_main);

    /* Stale transactional builders never publish their candidate. */
    On1xSDK_SourceInfo changed_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    assert(on1x_sdk_source_revision_info(changed_revision, &changed_info) ==
           ON1X_SDK_OK);
    On1xSDK_ASTBuilder *stale_builder = nullptr;
    assert(on1x_sdk_ast_builder_create(
        main_source, changed_info.revision, options, &stale_builder) ==
        ON1X_SDK_OK);
    const std::string candidate =
        "import dep;fn main(){println(dep.value()+1);}";
    assert(on1x_sdk_ast_builder_set_source(
        stale_builder, candidate.data(), candidate.size(),
        ON1X_SDK_SOURCE_UTF8) == ON1X_SDK_OK);
    On1xSDK_SourceSnapshot *third_revision = nullptr;
    assert(on1x_sdk_source_replace(
        main_source, changed_info.revision, changed_main.data(),
        changed_main.size(), ON1X_SDK_SOURCE_UTF8, &third_revision) ==
        ON1X_SDK_OK);
    On1xSDK_SourceSnapshot *should_be_null_revision = nullptr;
    On1xSDK_AST *should_be_null_ast = nullptr;
    assert(on1x_sdk_ast_builder_commit(
        stale_builder, &should_be_null_revision, &should_be_null_ast) ==
        ON1X_SDK_STALE);
    assert(should_be_null_revision == nullptr && should_be_null_ast == nullptr);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_STALE_REVISION);
    on1x_sdk_ast_builder_destroy(stale_builder);

    On1xSDK_SourceInfo third_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    assert(on1x_sdk_source_revision_info(third_revision, &third_info) ==
           ON1X_SDK_OK);
    On1xSDK_ASTBuilder *builder = nullptr;
    assert(on1x_sdk_ast_builder_create(
        main_source, third_info.revision, options, &builder) == ON1X_SDK_OK);
    assert(on1x_sdk_ast_builder_set_source(
        builder, candidate.data(), candidate.size(),
        ON1X_SDK_SOURCE_UTF8) == ON1X_SDK_OK);
    On1xSDK_AST *validated = nullptr;
    assert(on1x_sdk_ast_builder_validate(builder, &validated) == ON1X_SDK_OK);
    uint32_t still_current = 0;
    assert(on1x_sdk_source_revision_is_current(third_revision, &still_current) ==
           ON1X_SDK_OK);
    assert(still_current == 1);
    on1x_sdk_ast_release(validated);
    On1xSDK_SourceSnapshot *committed_revision = nullptr;
    On1xSDK_AST *committed_ast = nullptr;
    assert(on1x_sdk_ast_builder_commit(
        builder, &committed_revision, &committed_ast) == ON1X_SDK_OK);
    on1x_sdk_ast_builder_destroy(builder);

    /* Cache keys include exact source and options revisions/content. */
    On1xSDK_Cache *cache = nullptr;
    assert(on1x_sdk_cache_create(session, 2, &cache) == ON1X_SDK_OK);
    assert(on1x_sdk_cache_store(
        cache, committed_revision, options, committed_ast) == ON1X_SDK_OK);
    On1xSDK_AST *cache_hit = nullptr;
    assert(on1x_sdk_cache_lookup(
        cache, committed_revision, options, &cache_hit) == ON1X_SDK_OK);
    on1x_sdk_ast_release(cache_hit);
    On1xSDK_CacheInfo cache_info =
        ON1X_SDK_COMPILER_INIT(On1xSDK_CacheInfo);
    assert(on1x_sdk_cache_info(cache, &cache_info) == ON1X_SDK_OK);
    assert(cache_info.hits == 1 && cache_info.entry_count == 1);
    On1xSDK_Options *same_content_new_revision = nullptr;
    assert(on1x_sdk_options_clone(
        options, &same_content_new_revision) == ON1X_SDK_OK);
    On1xSDK_AST *cache_miss = reinterpret_cast<On1xSDK_AST *>(
        static_cast<std::uintptr_t>(1));
    assert(on1x_sdk_cache_lookup(
        cache, committed_revision, same_content_new_revision,
        &cache_miss) == ON1X_SDK_NOT_FOUND);
    assert(cache_miss == nullptr);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_NOT_FOUND);
    on1x_sdk_options_release(same_content_new_revision);

    /* Canonical macro expansion is transactional and removes definitions. */
    auto *macro_source = source_file(session, fixtures / "macro.on1x");
    auto *macro_ast = parse(macro_source, options);
    std::size_t macros = 0;
    assert(on1x_sdk_ast_macro_count(macro_ast, &macros) == ON1X_SDK_OK);
    assert(macros == 1);
    On1xSDK_AST *expanded = nullptr;
    assert(on1x_sdk_macro_expand(macro_ast, options, &expanded) ==
           ON1X_SDK_OK);
    assert(on1x_sdk_ast_macro_count(expanded, &macros) == ON1X_SDK_OK);
    assert(macros == 0);

    /* Staging is inspectable, but semantic lowering fails closed. */
    auto *staging_source = source_file(session, fixtures / "staging.on1x");
    auto *staging_ast = parse(staging_source, options);
    On1xSDK_StagingInfo staging =
        ON1X_SDK_COMPILER_INIT(On1xSDK_StagingInfo);
    assert(on1x_sdk_staging_inspect(staging_ast, &staging) == ON1X_SDK_OK);
    assert(staging.quote_count == 1 && staging.lift_count == 1 &&
           staging.run_count == 1 && staging.requires_semantic_lowering == 1);
    On1xSDK_AST *not_lowered = nullptr;
    assert(on1x_sdk_staging_lower(staging_ast, &not_lowered) ==
           ON1X_SDK_UNSUPPORTED);
    assert(not_lowered == nullptr);

    /* Node IDs are generation-bound and reject cross-snapshot use. */
    assert(on1x_sdk_ast_node(
        staging_ast, loaded_info.root, &root_info) == ON1X_SDK_STALE);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_STALE_REVISION);

    /* UTF-8 is explicit: strict source rejects; byte source follows scanner. */
    const char invalid_utf8[] = {'#', ' ', static_cast<char>(0xff), '\n',
                                 'f', 'n', ' ', 'm', 'a', 'i', 'n',
                                 '(', ')', '{', '}'};
    On1xSDK_Source *strict_invalid = nullptr;
    assert(on1x_sdk_source_create(
        session, "virtual/strict.on1x", strlen("virtual/strict.on1x"),
        invalid_utf8, sizeof(invalid_utf8), ON1X_SDK_SOURCE_UTF8,
        &strict_invalid) == ON1X_SDK_VERIFICATION_FAILED);
    assert(strict_invalid == nullptr);
    On1xSDK_Source *byte_source = nullptr;
    assert(on1x_sdk_source_create(
        session, "virtual/bytes.on1x", strlen("virtual/bytes.on1x"),
        invalid_utf8, sizeof(invalid_utf8), ON1X_SDK_SOURCE_BYTES,
        &byte_source) == ON1X_SDK_OK);
    auto *byte_revision = snapshot(byte_source);
    On1xSDK_TokenStream *byte_tokens = nullptr;
    assert(on1x_sdk_lex(byte_revision, options, &byte_tokens) == ON1X_SDK_OK);

    const char with_nul[] = {'f', 'n', '\0', 'x'};
    On1xSDK_Source *nul_source = nullptr;
    assert(on1x_sdk_source_create(
        session, "virtual/nul.on1x", strlen("virtual/nul.on1x"),
        with_nul, sizeof(with_nul), ON1X_SDK_SOURCE_BYTES,
        &nul_source) == ON1X_SDK_OK);
    auto *nul_revision = snapshot(nul_source);
    On1xSDK_TokenStream *nul_tokens = nullptr;
    assert(on1x_sdk_lex(nul_revision, options, &nul_tokens) ==
           ON1X_SDK_VERIFICATION_FAILED);
    assert(nul_tokens == nullptr && diagnostics.begin == 2);

    /* Deterministic token/work budgets and cooperative cancellation differ. */
    On1xSDK_Options *tiny = nullptr;
    assert(on1x_sdk_options_clone(options, &tiny) == ON1X_SDK_OK);
    assert(on1x_sdk_options_set_limit(
        tiny, ON1X_SDK_OPTION_MAX_TOKENS, 1) == ON1X_SDK_OK);
    On1xSDK_TokenStream *limited = nullptr;
    assert(on1x_sdk_lex(committed_revision, tiny, &limited) == ON1X_SDK_LIMIT);
    assert(limited == nullptr);
    assert(on1x_sdk_options_set_limit(
        tiny, ON1X_SDK_OPTION_MAX_TOKENS, 1024) == ON1X_SDK_OK);
    assert(on1x_sdk_options_set_limit(
        tiny, ON1X_SDK_OPTION_WORK_BUDGET, 0) == ON1X_SDK_OK);
    assert(on1x_sdk_lex(committed_revision, tiny, &limited) == ON1X_SDK_LIMIT);
    assert(on1x_sdk_session_request_cancel(session) == ON1X_SDK_OK);
    assert(on1x_sdk_lex(committed_revision, options, &limited) ==
           ON1X_SDK_CANCELLED);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_CANCELLED);
    assert(on1x_sdk_session_reset_cancel(session) == ON1X_SDK_OK);

    /* Cross-session object mixing is rejected distinctly. */
    On1xSDK_Session *other_session = nullptr;
    On1xSDK_Options *other_options = nullptr;
    assert(on1x_sdk_session_create(nullptr, &other_session) == ON1X_SDK_OK);
    assert(on1x_sdk_options_create(other_session, &other_options) ==
           ON1X_SDK_OK);
    assert(on1x_sdk_lex(committed_revision, other_options, &limited) ==
           ON1X_SDK_INVALID_ARGUMENT);
    assert(on1x_sdk_session_last_condition(session) ==
           ON1X_SDK_COMPILER_CROSS_SESSION);
    on1x_sdk_options_release(other_options);
    on1x_sdk_session_release(other_session);

    /* Session-owned APIs reject use from a foreign thread. */
    On1xSDK_Status foreign_thread_status = ON1X_SDK_OK;
    std::thread foreign_thread([&] {
        On1xSDK_SourceInfo info =
            ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
        foreign_thread_status = on1x_sdk_source_revision_info(
            committed_revision, &info);
    });
    foreign_thread.join();
    assert(foreign_thread_status == ON1X_SDK_INVALID_ARGUMENT);

    /* Workspace combines normalized physical files and overlays. */
    const auto root_text = path_text(root);
    On1xSDK_Workspace *workspace = nullptr;
    assert(on1x_sdk_workspace_create(
        session, root_text.data(), root_text.size(), options, &workspace) ==
        ON1X_SDK_OK);
    On1xSDK_Source *escaped = reinterpret_cast<On1xSDK_Source *>(
        static_cast<std::uintptr_t>(1));
    assert(on1x_sdk_workspace_open(
        workspace, "../outside.on1x", strlen("../outside.on1x"),
        ON1X_SDK_SOURCE_UTF8, &escaped) == ON1X_SDK_INVALID_ARGUMENT);
    assert(escaped == nullptr);

    const fs::path outside = root.string() + "-outside";
    fs::create_directories(outside);
    fs::copy_file(fixtures / "dep.on1x", outside / "outside.on1x");
    std::error_code symlink_error;
    fs::create_directory_symlink(
        outside, root / "escape", symlink_error);
    if (!symlink_error) {
        assert(on1x_sdk_workspace_open(
            workspace, "escape/outside.on1x",
            strlen("escape/outside.on1x"), ON1X_SDK_SOURCE_UTF8,
            &escaped) == ON1X_SDK_INVALID_ARGUMENT);
        assert(escaped == nullptr);
    }
    On1xSDK_Source *workspace_main = nullptr;
    On1xSDK_Source *workspace_dep = nullptr;
    assert(on1x_sdk_workspace_open(
        workspace, "main.on1x", strlen("main.on1x"),
        ON1X_SDK_SOURCE_UTF8, &workspace_main) == ON1X_SDK_OK);
    assert(on1x_sdk_workspace_overlay(
        workspace, "dep.on1x", strlen("dep.on1x"),
        dep_overlay.data(), dep_overlay.size(), ON1X_SDK_SOURCE_UTF8,
        &workspace_dep) == ON1X_SDK_OK);
    On1xSDK_Compilation *workspace_compilation = nullptr;
    assert(on1x_sdk_workspace_compile(
        workspace, "main.on1x", strlen("main.on1x"),
        &workspace_compilation) == ON1X_SDK_OK);

    on1x_sdk_compilation_release(workspace_compilation);
    on1x_sdk_source_release(workspace_dep);
    on1x_sdk_source_release(workspace_main);
    on1x_sdk_workspace_release(workspace);
    on1x_sdk_options_release(tiny);
    on1x_sdk_token_stream_release(byte_tokens);
    on1x_sdk_source_revision_release(byte_revision);
    on1x_sdk_source_release(byte_source);
    on1x_sdk_source_revision_release(nul_revision);
    on1x_sdk_source_release(nul_source);
    on1x_sdk_ast_release(staging_ast);
    on1x_sdk_source_release(staging_source);
    on1x_sdk_ast_release(expanded);
    on1x_sdk_ast_release(macro_ast);
    on1x_sdk_source_release(macro_source);
    on1x_sdk_cache_release(cache);
    on1x_sdk_ast_release(committed_ast);
    on1x_sdk_source_revision_release(committed_revision);
    on1x_sdk_source_revision_release(third_revision);
    on1x_sdk_source_revision_release(changed_revision);
    on1x_sdk_ast_release(loaded_ast);
    on1x_sdk_compilation_release(refreshed);
    on1x_sdk_compilation_release(compilation);
    on1x_sdk_source_revision_release(dep_revision);
    on1x_sdk_source_release(dep_source);
    on1x_sdk_source_release(main_source);
    on1x_sdk_vfs_release(vfs);
    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);

    fs::remove_all(root);
    fs::remove_all(outside);

    /* Immutable snapshots remain readable after their session handle closes. */
    On1xSDK_Session *lifetime_session = nullptr;
    On1xSDK_Options *lifetime_options = nullptr;
    On1xSDK_Source *lifetime_source = nullptr;
    On1xSDK_SourceSnapshot *lifetime_revision = nullptr;
    On1xSDK_AST *lifetime_ast = nullptr;
    assert(on1x_sdk_session_create(nullptr, &lifetime_session) ==
           ON1X_SDK_OK);
    assert(on1x_sdk_options_create(
        lifetime_session, &lifetime_options) == ON1X_SDK_OK);
    assert(on1x_sdk_source_create(
        lifetime_session, "virtual/lifetime.on1x",
        strlen("virtual/lifetime.on1x"), ephemeral_text.data(),
        ephemeral_text.size(), ON1X_SDK_SOURCE_UTF8,
        &lifetime_source) == ON1X_SDK_OK);
    lifetime_revision = snapshot(lifetime_source);
    assert(on1x_sdk_frontend_parse(
        lifetime_revision, lifetime_options, &lifetime_ast) == ON1X_SDK_OK);
    on1x_sdk_session_release(lifetime_session);
    assert(ast_info(lifetime_ast).node_count > 1u);
    const void *lifetime_bytes = nullptr;
    std::size_t lifetime_size = 0;
    assert(on1x_sdk_source_revision_bytes(
        lifetime_revision, &lifetime_bytes, &lifetime_size) == ON1X_SDK_OK);
    assert(lifetime_size == ephemeral_text.size());
    On1xSDK_SourceSnapshot *closed_mutation =
        reinterpret_cast<On1xSDK_SourceSnapshot *>(
            static_cast<std::uintptr_t>(1));
    assert(on1x_sdk_source_replace(
        lifetime_source, 0, ephemeral_text.data(), ephemeral_text.size(),
        ON1X_SDK_SOURCE_UTF8, &closed_mutation) == ON1X_SDK_INVALID_STATE);
    assert(closed_mutation == nullptr);
    on1x_sdk_ast_release(lifetime_ast);
    on1x_sdk_source_revision_release(lifetime_revision);
    on1x_sdk_source_release(lifetime_source);
    on1x_sdk_options_release(lifetime_options);

    assert(diagnostics.count >= 8);
}

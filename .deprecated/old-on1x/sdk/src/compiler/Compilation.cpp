#include "FrontendInternal.hpp"

#include "On1xSDK-Incremental.h"

#include <on1x/clone.hpp>
#include <on1x/loader.hpp>
#include <on1x/meta.hpp>
#include <on1x/module_graph.hpp>
#include <on1x/operator_scope.hpp>

#include <fstream>

namespace on1x::sdk2 {
namespace {

struct OverlaySnapshot {
    std::shared_ptr<RevisionData> revision;
    bool from_overlay = false;
};

bool path_within(const fs::path &root, const fs::path &candidate) {
    auto left = root.begin();
    auto right = candidate.begin();
    for (; left != root.end() && right != candidate.end(); ++left, ++right)
        if (*left != *right) return false;
    return left == root.end();
}

std::string workspace_path(
    const On1xSDK_Workspace &workspace,
    const char *relative,
    std::size_t relative_size) {
    auto text = checked_string(
        relative, relative_size, hard_max_path_bytes,
        "workspace relative path", true, false);
    fs::path path{text};
    if (path.is_absolute()) invalid("workspace path must be relative");
    auto joined = (fs::path(workspace.root) / path).lexically_normal();
    const auto root = fs::path(workspace.root).lexically_normal();
    if (!path_within(root, joined))
        invalid("workspace path escapes its root");
    auto normalized = normalize_path(
        workspace.state, joined.generic_string());
    if (!path_within(root, fs::path(normalized)))
        invalid("workspace path resolves outside its root");
    return normalized;
}

std::string read_workspace_file(
    const std::string &path,
    std::size_t maximum) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_IO_FAILURE,
             "ON1X-SDK-IO", "cannot open workspace source: " + path);
    std::string bytes;
    char block[8192];
    while (input) {
        const auto remaining = maximum + 1u - bytes.size();
        const auto count = std::min<std::size_t>(
            sizeof(block), remaining);
        input.read(block, static_cast<std::streamsize>(count));
        bytes.append(block, static_cast<std::size_t>(input.gcount()));
        if (bytes.size() > maximum)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-SOURCE-LIMIT",
                 "workspace source exceeds configured byte limit");
    }
    if (input.bad())
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_IO_FAILURE,
             "ON1X-SDK-IO", "cannot read workspace source: " + path);
    return bytes;
}

void ensure_source_policy(
    const RevisionData &source,
    const OptionsData &options) {
    if (source.bytes.size() > options.max_source_bytes)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-SOURCE-LIMIT",
             "source exceeds the configured source byte limit");
    if ((options.strict_utf8 ||
         source.encoding == ON1X_SDK_SOURCE_UTF8) &&
        !valid_utf8(source.bytes))
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_VERIFICATION_FAILED,
             "ON1X-SDK-UTF8", "source is not valid shortest-form UTF-8");
}

std::shared_ptr<RevisionData> detached_revision(
    const std::shared_ptr<SessionState> &state,
    std::string path,
    std::string bytes) {
    auto source = make_source(
        state, std::move(path), std::move(bytes), ON1X_SDK_SOURCE_BYTES);
    return source->current;
}

std::shared_ptr<ASTData> maybe_expand(
    const std::shared_ptr<ASTData> &input,
    const OptionsData &options) {
    if (!options.expand_macros || !on1x::needs_macro_expansion(*input->unit))
        return input;
    Operation operation(input->state, options);
    operation.charge(input->nodes.size());
    std::size_t budget = options.max_ast_nodes;
    auto unit = on1x::clone_unit(*input->unit, budget);
    on1x::expand_macros(unit);
    operation.poll();
    return build_ast(
        std::move(unit), input->sources, options,
        ON1X_SDK_AST_MACRO_EXPANDED_CANDIDATE, input->source_operator_imports);
}

} // namespace

std::shared_ptr<CompilationData> compile_impl(
    const std::shared_ptr<SessionState> &state,
    const std::shared_ptr<VFSData> &vfs,
    const OptionsData &options,
    const std::shared_ptr<SourceData> &entry) {
    require_same(state, vfs->state);
    require_same(state, entry->state);
    if (vfs->overlays.size() > options.max_overlays)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-VFS-LIMIT",
             "configured VFS overlay count limit exceeded");

    Operation operation(state, options);
    const auto entry_revision = entry->current;
    ensure_source_policy(*entry_revision, options);
    operation.charge(entry_revision->bytes.size() + 1u);

    std::map<std::string, OverlaySnapshot> snapshots;
    for (const auto &[path, source] : vfs->overlays) {
        require_same(state, source->state);
        ensure_source_policy(*source->current, options);
        snapshots[path] = {source->current, true};
    }
    snapshots[entry_revision->path] = {entry_revision, true};

    auto result = std::make_shared<CompilationData>();
    result->state = state;
    result->vfs = vfs;
    result->entry = entry;
    result->options = options;
    result->vfs_revision = vfs->revision;
    result->entry_revision = entry_revision->revision;

    std::error_code file_error;
    const bool physical =
        fs::is_regular_file(fs::path(entry_revision->path), file_error);
    if (!file_error && physical) {
        on1x::LoaderOptions loader;
        loader.prelude = options.prelude;
        loader.max_modules = options.max_modules;
        loader.max_depth = options.max_import_depth;
        loader.max_bytes = options.max_total_bytes;
        loader.extensions.execute = false;
        loader.capture_module_graph = options.capture_module_graph;
        loader.extensions.enabled.insert(
            options.features.begin(), options.features.end());
        // Reserved standard identities require a separately selected root.
        // Do not use CLI defaults: an embedded caller supplies its own root.
        auto path = options.search_paths.begin();
        if (options.first_search_path_is_stdlib) {
            loader.stdlib_paths.emplace_back(*path);
            ++path;
        }
        for (; path != options.search_paths.end(); ++path)
            loader.paths.emplace_back(*path);
        for (const auto &[path, snapshot] : snapshots)
            loader.source_overrides.emplace(
                fs::path(path), snapshot.revision->bytes);

        operation.poll();
        auto loaded = on1x::load_unit(entry_revision->path, loader);
        operation.poll();
        // The existing AST and dependency-source APIs expose full source and
        // syntax snapshots. Do not publish them for an admitted opaque unit.
        if (!loaded.unit.opaque_nominal_owners.empty() ||
            !loaded.unit.opaque_function_owners.empty())
            fail(ON1X_SDK_UNSUPPORTED,
                 ON1X_SDK_COMPILER_CAPABILITY_DENIED,
                 "ON1X-SDK-OPAQUE-MODULE",
                 "opaque modules are not supported by frontend snapshots");
        if (loaded.dependencies.size() != loaded.dependency_sources.size())
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-LOADER",
                 "canonical loader returned misaligned source snapshots");
        if (loaded.dependencies.size() > options.max_modules)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-MODULE-LIMIT",
                 "loaded dependency count exceeds configured limit");

        if (options.capture_module_graph) {
            if (!loaded.module_graph) invalid("loader omitted requested module graph");
            result->import_graph = loaded.module_graph->serialize(false, "json");
            operation.charge(result->import_graph.size());
            result->module_graph = loaded.module_graph->serialize(true, "json");
            operation.charge(result->module_graph.size());
        }
        std::size_t total_bytes = 0;
        std::size_t total_tokens = 0;
        std::vector<std::shared_ptr<RevisionData>> sources;
        std::vector<on1x::Import> source_operator_imports;
        for (std::size_t i = 0; i < loaded.dependencies.size(); ++i) {
            auto path = normalize_path(
                state, loaded.dependencies[i].generic_string());
            const auto &bytes = loaded.dependency_sources[i];
            if (bytes.size() > options.max_source_bytes ||
                bytes.size() > options.max_total_bytes - total_bytes)
                fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-SOURCE-LIMIT",
                     "loaded source byte budget exhausted");
            total_bytes += bytes.size();
            if (options.strict_utf8 && !valid_utf8(bytes))
                fail(ON1X_SDK_ERROR,
                     ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                     "ON1X-SDK-UTF8",
                     "loaded source is not valid shortest-form UTF-8");
            operation.charge(bytes.size() + 1u);
            auto scanned = on1x::scan(bytes, path);
            if (scanned.size() > options.max_tokens - total_tokens)
                fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-TOKEN-LIMIT",
                     "loaded token budget exhausted");
            total_tokens += scanned.size();
            operation.charge(scanned.size());

            // The merged Unit has already consumed its imports. Project the
            // operator-facing statements from the exact loaded snapshot, not
            // from disk or from the rewritten namespace.
            for (auto &import : on1x::discover_operators(scanned).imports) {
                if (import.quoted || std::any_of(
                        import.names.begin(), import.names.end(),
                        [](const auto &name) { return name.is_operator; }))
                    source_operator_imports.push_back(std::move(import));
            }

            bool from_overlay = false;
            std::shared_ptr<RevisionData> revision;
            auto found = snapshots.find(path);
            if (found != snapshots.end() &&
                found->second.revision->bytes == bytes) {
                revision = found->second.revision;
                from_overlay = found->second.from_overlay;
            } else {
                revision = detached_revision(state, path, bytes);
            }
            const bool is_entry = path == entry_revision->path;
            result->dependencies.push_back(
                {revision, is_entry, from_overlay});
            sources.push_back(std::move(revision));
        }
        if (result->dependencies.empty())
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-LOADER",
                 "canonical loader omitted the entry source");
        result->ast = build_ast(
            std::move(loaded.unit), std::move(sources), options,
            ON1X_SDK_AST_PARSED_CANDIDATE, std::move(source_operator_imports));
        result->ast = maybe_expand(result->ast, options);
    } else {
        if (options.capture_module_graph)
            fail(ON1X_SDK_UNSUPPORTED, ON1X_SDK_COMPILER_CAPABILITY_DENIED,
                 "ON1X-SDK-VIRTUAL-GRAPH", "graph capture requires a physical entry path");
        auto ast = parse_revision_impl(entry_revision, options);
        on1x::require_no_kernel_imports(*ast->unit);
        if (!ast->unit->module_imports.empty())
            fail(ON1X_SDK_UNSUPPORTED,
                 ON1X_SDK_COMPILER_CAPABILITY_DENIED,
                 "ON1X-SDK-VIRTUAL-IMPORT",
                 "virtual sources with imports require a physical anchor path");
        result->ast = maybe_expand(ast, options);
        result->dependencies.push_back({entry_revision, true, true});
    }

    operation.poll();
    if (entry->current != entry_revision)
        stale("entry source changed while compilation was in progress");
    result->revision = publish_revision(state);
    result->session_revision =
        state->revision.load(std::memory_order_relaxed);
    return result;
}

bool compilation_current(const CompilationData &compilation) {
    if (compilation.vfs->revision != compilation.vfs_revision ||
        compilation.entry->current->revision != compilation.entry_revision)
        return false;
    for (const auto &dependency : compilation.dependencies)
        if (!revision_is_current(dependency.source)) return false;
    return true;
}

} // namespace on1x::sdk2

using namespace on1x::sdk2;

extern "C" {

On1xSDK_Status on1x_sdk_compilation_create(
    On1xSDK_Session *session,
    On1xSDK_VFS *vfs,
    const On1xSDK_Options *options,
    On1xSDK_Source *entry,
    On1xSDK_Compilation **out_compilation) {
    if (!out_compilation) return ON1X_SDK_INVALID_ARGUMENT;
    *out_compilation = nullptr;
    if (!session || !vfs || !options || !entry)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        require_same(session->state, vfs->data->state);
        require_same(session->state, options->state);
        require_same(session->state, entry->data->state);
        auto compilation = std::make_unique<On1xSDK_Compilation>();
        compilation->data = compile_impl(
            session->state, vfs->data, options->data, entry->data);
        *out_compilation = compilation.release();
    });
}

On1xSDK_Status on1x_sdk_compilation_retain(
    On1xSDK_Compilation *compilation) {
    return retain_object(compilation);
}

void on1x_sdk_compilation_release(On1xSDK_Compilation *compilation) {
    release_object(compilation);
}

On1xSDK_Status on1x_sdk_compilation_info(
    const On1xSDK_Compilation *compilation,
    On1xSDK_CompilationInfo *out_info) {
    if (!compilation) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        On1xSDK_CompilationInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_CompilationInfo);
        value.revision = compilation->data->revision;
        value.session_revision = compilation->data->session_revision;
        value.vfs_revision = compilation->data->vfs_revision;
        value.entry_source_id = compilation->data->entry->source_id;
        value.entry_source_revision = compilation->data->entry_revision;
        value.dependency_count = compilation->data->dependencies.size();
        value.is_current = compilation_current(*compilation->data) ? 1u : 0u;
        write_descriptor(out_info, value);
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_ast(
    const On1xSDK_Compilation *compilation,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!compilation) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        auto ast = std::make_unique<On1xSDK_AST>();
        ast->data = compilation->data->ast;
        *out_ast = ast.release();
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_is_current(
    const On1xSDK_Compilation *compilation,
    uint32_t *out_current) {
    if (!compilation || !out_current) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        *out_current = compilation_current(*compilation->data) ? 1u : 0u;
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_graph_json_v1(
    const On1xSDK_Compilation *compilation, uint32_t logical,
    const char **out_bytes, size_t *out_size) {
    if (!out_bytes || !out_size) return ON1X_SDK_INVALID_ARGUMENT;
    *out_bytes = nullptr;
    *out_size = 0;
    if (!compilation || logical > 1) return ON1X_SDK_INVALID_ARGUMENT;
    if (!compilation->data->options.capture_module_graph) return ON1X_SDK_UNSUPPORTED;
    return api(compilation->data->state, [&] {
        const auto &graph = logical ? compilation->data->module_graph : compilation->data->import_graph;
        *out_bytes = graph.data();
        *out_size = graph.size();
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_dependency_count(
    const On1xSDK_Compilation *compilation,
    size_t *out_count) {
    if (!compilation || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        *out_count = compilation->data->dependencies.size();
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_dependency(
    const On1xSDK_Compilation *compilation,
    size_t index,
    On1xSDK_DependencyInfo *out_dependency) {
    if (!compilation) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        if (index >= compilation->data->dependencies.size())
            not_found("dependency index is out of range");
        const auto &dependency = compilation->data->dependencies[index];
        On1xSDK_DependencyInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_DependencyInfo);
        value.index = index;
        value.source_id = dependency.source->source_id;
        value.source_revision = dependency.source->revision;
        value.path = dependency.source->path.data();
        value.path_size = dependency.source->path.size();
        value.byte_size = dependency.source->bytes.size();
        value.is_entry = dependency.is_entry ? 1u : 0u;
        value.from_overlay = dependency.from_overlay ? 1u : 0u;
        write_descriptor(out_dependency, value);
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_dependency_source(
    const On1xSDK_Compilation *compilation,
    size_t index,
    On1xSDK_SourceSnapshot **out_revision) {
    if (!out_revision) return ON1X_SDK_INVALID_ARGUMENT;
    *out_revision = nullptr;
    if (!compilation) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        if (index >= compilation->data->dependencies.size())
            not_found("dependency index is out of range");
        auto revision = std::make_unique<On1xSDK_SourceSnapshot>();
        revision->data = compilation->data->dependencies[index].source;
        *out_revision = revision.release();
    }, true);
}

On1xSDK_Status on1x_sdk_compilation_refresh(
    const On1xSDK_Compilation *compilation,
    On1xSDK_Compilation **out_compilation) {
    if (!out_compilation) return ON1X_SDK_INVALID_ARGUMENT;
    *out_compilation = nullptr;
    if (!compilation) return ON1X_SDK_INVALID_ARGUMENT;
    return api(compilation->data->state, [&] {
        auto next = std::make_unique<On1xSDK_Compilation>();
        next->data = compile_impl(
            compilation->data->state,
            compilation->data->vfs,
            compilation->data->options,
            compilation->data->entry);
        *out_compilation = next.release();
    });
}

On1xSDK_Status on1x_sdk_cache_create(
    On1xSDK_Session *session,
    size_t capacity,
    On1xSDK_Cache **out_cache) {
    if (!out_cache) return ON1X_SDK_INVALID_ARGUMENT;
    *out_cache = nullptr;
    if (!session || !capacity || capacity > 1024u)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        auto cache = std::make_unique<On1xSDK_Cache>();
        cache->state = session->state;
        cache->capacity = capacity;
        cache->entries.reserve(capacity);
        *out_cache = cache.release();
    });
}

On1xSDK_Status on1x_sdk_cache_retain(On1xSDK_Cache *cache) {
    return retain_object(cache);
}

void on1x_sdk_cache_release(On1xSDK_Cache *cache) {
    release_object(cache);
}

On1xSDK_Status on1x_sdk_cache_store(
    On1xSDK_Cache *cache,
    const On1xSDK_SourceSnapshot *source,
    const On1xSDK_Options *options,
    const On1xSDK_AST *ast) {
    if (!cache || !source || !options || !ast)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(cache->state, [&] {
        require_same(cache->state, source->data->state);
        require_same(cache->state, options->state);
        require_same(cache->state, ast->data->state);
        const auto belongs = std::any_of(
            ast->data->sources.begin(), ast->data->sources.end(),
            [&](const auto &item) {
                return item->source_id == source->data->source_id &&
                       item->revision == source->data->revision;
            });
        if (!belongs) invalid("AST does not represent the cache source revision");
        auto found = std::find_if(
            cache->entries.begin(), cache->entries.end(),
            [&](const auto &entry) {
                return entry.source_id == source->data->source_id &&
                       entry.source_revision == source->data->revision &&
                       entry.options == options->data;
            });
        if (found != cache->entries.end()) {
            found->ast = ast->data;
            return;
        }
        On1xSDK_CacheEntry next{
            source->data->source_id, source->data->revision,
            options->data, ast->data};
        if (cache->entries.size() == cache->capacity)
            cache->entries.erase(cache->entries.begin());
        cache->entries.push_back(std::move(next));
    });
}

On1xSDK_Status on1x_sdk_cache_lookup(
    On1xSDK_Cache *cache,
    const On1xSDK_SourceSnapshot *source,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!cache || !source || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(cache->state, [&] {
        require_same(cache->state, source->data->state);
        require_same(cache->state, options->state);
        auto found = std::find_if(
            cache->entries.begin(), cache->entries.end(),
            [&](const auto &entry) {
                return entry.source_id == source->data->source_id &&
                       entry.source_revision == source->data->revision &&
                       entry.options == options->data;
            });
        if (found == cache->entries.end()) {
            ++cache->misses;
            not_found("no exact revision/options cache entry");
        }
        ++cache->hits;
        auto ast = std::make_unique<On1xSDK_AST>();
        ast->data = found->ast;
        *out_ast = ast.release();
    });
}

On1xSDK_Status on1x_sdk_cache_clear(On1xSDK_Cache *cache) {
    if (!cache) return ON1X_SDK_INVALID_ARGUMENT;
    return api(cache->state, [&] { cache->entries.clear(); });
}

On1xSDK_Status on1x_sdk_cache_info(
    const On1xSDK_Cache *cache,
    On1xSDK_CacheInfo *out_info) {
    if (!cache) return ON1X_SDK_INVALID_ARGUMENT;
    return api(cache->state, [&] {
        On1xSDK_CacheInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_CacheInfo);
        value.capacity = cache->capacity;
        value.entry_count = cache->entries.size();
        value.hits = cache->hits;
        value.misses = cache->misses;
        write_descriptor(out_info, value);
    });
}

On1xSDK_Status on1x_sdk_workspace_create(
    On1xSDK_Session *session,
    const char *root,
    size_t root_size,
    const On1xSDK_Options *options,
    On1xSDK_Workspace **out_workspace) {
    if (!out_workspace) return ON1X_SDK_INVALID_ARGUMENT;
    *out_workspace = nullptr;
    if (!session || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        require_same(session->state, options->state);
        auto workspace = std::make_unique<On1xSDK_Workspace>();
        auto workspace_options = std::make_unique<On1xSDK_Options>();
        auto workspace_vfs = std::make_unique<On1xSDK_VFS>();
        workspace->state = session->state;
        workspace->root = normalize_path(session->state, root, root_size);
        std::error_code root_error;
        if (!fs::is_directory(workspace->root, root_error)) {
            const auto detail = root_error
                ? ": " + root_error.message() : std::string{};
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_IO_FAILURE,
                 "ON1X-SDK-WORKSPACE",
                 "workspace root is not a directory" + detail);
        }
        workspace_options->state = session->state;
        workspace_options->data = options->data;
        workspace_options->data.revision =
            publish_revision(session->state);
        workspace_vfs->data = std::make_shared<VFSData>();
        workspace_vfs->data->state = session->state;
        workspace_vfs->data->revision =
            publish_revision(session->state);
        workspace->options = workspace_options.release();
        workspace->vfs = workspace_vfs.release();
        *out_workspace = workspace.release();
    });
}

On1xSDK_Status on1x_sdk_workspace_retain(On1xSDK_Workspace *workspace) {
    return retain_object(workspace);
}

void on1x_sdk_workspace_release(On1xSDK_Workspace *workspace) {
    if (!workspace) return;
    if (workspace->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        on1x_sdk_vfs_release(workspace->vfs);
        on1x_sdk_options_release(workspace->options);
        delete workspace;
    }
}

On1xSDK_Status on1x_sdk_workspace_overlay(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source) {
    if (!out_source) return ON1X_SDK_INVALID_ARGUMENT;
    *out_source = nullptr;
    if (!workspace) return ON1X_SDK_INVALID_ARGUMENT;
    return api(workspace->state, [&] {
        auto path = workspace_path(
            *workspace, relative_path, relative_path_size);
        auto text = checked_bytes(
            bytes, byte_size, workspace->options->data.max_source_bytes,
            "workspace source");
        if (encoding != ON1X_SDK_SOURCE_BYTES &&
            encoding != ON1X_SDK_SOURCE_UTF8)
            invalid("unknown source encoding policy");
        if ((encoding == ON1X_SDK_SOURCE_UTF8 ||
             workspace->options->data.strict_utf8) &&
            !valid_utf8(text))
            fail(ON1X_SDK_ERROR,
                 ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                 "ON1X-SDK-UTF8",
                 "workspace source is not valid shortest-form UTF-8");
        auto handle = std::make_unique<On1xSDK_Source>();
        auto found = workspace->vfs->data->overlays.find(path);
        std::shared_ptr<SourceData> source;
        std::shared_ptr<RevisionData> replacement;
        if (found == workspace->vfs->data->overlays.end()) {
            if (workspace->vfs->data->overlays.size() >=
                workspace->options->data.max_overlays)
                fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-VFS-LIMIT",
                     "workspace overlay count limit exceeded");
            source = make_source(
                workspace->state, path, std::move(text), encoding);
        } else {
            source = found->second;
            replacement = make_revision(
                workspace->state, source->source_id, path,
                std::move(text), encoding);
            replacement->identity = source->identity;
        }
        handle->data = source;
        const auto vfs_revision = publish_revision(workspace->state);
        if (found == workspace->vfs->data->overlays.end())
            workspace->vfs->data->overlays.emplace(path, source);
        else
            set_current_revision(source, std::move(replacement));
        workspace->vfs->data->revision = vfs_revision;
        *out_source = handle.release();
    });
}

On1xSDK_Status on1x_sdk_workspace_open(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source) {
    if (!out_source) return ON1X_SDK_INVALID_ARGUMENT;
    *out_source = nullptr;
    if (!workspace) return ON1X_SDK_INVALID_ARGUMENT;
    return api(workspace->state, [&] {
        auto path = workspace_path(
            *workspace, relative_path, relative_path_size);
        auto found = workspace->vfs->data->overlays.find(path);
        if (found != workspace->vfs->data->overlays.end()) {
            auto handle = std::make_unique<On1xSDK_Source>();
            handle->data = found->second;
            *out_source = handle.release();
            return;
        }
        if (encoding != ON1X_SDK_SOURCE_BYTES &&
            encoding != ON1X_SDK_SOURCE_UTF8)
            invalid("unknown source encoding policy");
        if (workspace->vfs->data->overlays.size() >=
            workspace->options->data.max_overlays)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-VFS-LIMIT",
                 "workspace overlay count limit exceeded");
        auto bytes = read_workspace_file(
            path, workspace->options->data.max_source_bytes);
        if ((encoding == ON1X_SDK_SOURCE_UTF8 ||
             workspace->options->data.strict_utf8) &&
            !valid_utf8(bytes))
            fail(ON1X_SDK_ERROR,
                 ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                 "ON1X-SDK-UTF8",
                 "workspace source is not valid shortest-form UTF-8");
        auto handle = std::make_unique<On1xSDK_Source>();
        auto source = make_source(
            workspace->state, path, std::move(bytes), encoding);
        handle->data = source;
        const auto vfs_revision = publish_revision(workspace->state);
        workspace->vfs->data->overlays.emplace(path, source);
        workspace->vfs->data->revision = vfs_revision;
        *out_source = handle.release();
    });
}

On1xSDK_Status on1x_sdk_workspace_compile(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    On1xSDK_Compilation **out_compilation) {
    if (!out_compilation) return ON1X_SDK_INVALID_ARGUMENT;
    *out_compilation = nullptr;
    if (!workspace) return ON1X_SDK_INVALID_ARGUMENT;
    return api(workspace->state, [&] {
        auto path = workspace_path(
            *workspace, relative_path, relative_path_size);
        auto found = workspace->vfs->data->overlays.find(path);
        if (found == workspace->vfs->data->overlays.end())
            not_found("workspace source has not been opened or overlaid");
        auto compilation = std::make_unique<On1xSDK_Compilation>();
        compilation->data = compile_impl(
            workspace->state, workspace->vfs->data,
            workspace->options->data, found->second);
        *out_compilation = compilation.release();
    });
}

On1xSDK_Status on1x_sdk_workspace_vfs(
    const On1xSDK_Workspace *workspace,
    On1xSDK_VFS **out_vfs) {
    if (!out_vfs) return ON1X_SDK_INVALID_ARGUMENT;
    *out_vfs = nullptr;
    if (!workspace) return ON1X_SDK_INVALID_ARGUMENT;
    return api(workspace->state, [&] {
        const auto status = on1x_sdk_vfs_retain(workspace->vfs);
        if (status != ON1X_SDK_OK)
            fail(status, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-RETAIN", "VFS reference limit exhausted");
        *out_vfs = workspace->vfs;
    });
}

On1xSDK_Status on1x_sdk_workspace_options(
    const On1xSDK_Workspace *workspace,
    On1xSDK_Options **out_options) {
    if (!out_options) return ON1X_SDK_INVALID_ARGUMENT;
    *out_options = nullptr;
    if (!workspace) return ON1X_SDK_INVALID_ARGUMENT;
    return api(workspace->state, [&] {
        const auto status = on1x_sdk_options_retain(workspace->options);
        if (status != ON1X_SDK_OK)
            fail(status, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-RETAIN", "options reference limit exhausted");
        *out_options = workspace->options;
    });
}

} // extern "C"

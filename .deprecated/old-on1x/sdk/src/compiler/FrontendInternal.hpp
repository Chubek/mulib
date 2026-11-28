#pragma once

#include "On1xSDK-AST.h"
#include "On1xSDK-Cache.h"
#include "On1xSDK-Compilation.h"
#include "On1xSDK-Dependency.h"
#include "On1xSDK-Directive.h"
#include "On1xSDK-Grammar.h"
#include "On1xSDK-Lexer.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-PPX.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-Staging.h"
#include "On1xSDK-Workspace.h"

#include <on1x/ast.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace on1x::sdk2 {

namespace fs = std::filesystem;

inline constexpr std::size_t hard_max_total_bytes = 16u * 1024u * 1024u;
inline constexpr std::size_t hard_max_tokens = 1024u * 1024u;
inline constexpr std::size_t hard_max_ast_nodes = 262144u;
inline constexpr std::size_t hard_max_modules = 1024u;
inline constexpr std::size_t hard_max_import_depth = 64u;
inline constexpr std::size_t hard_max_diagnostics = 1024u;
inline constexpr std::size_t hard_max_overlays = 1024u;
inline constexpr std::size_t hard_max_path_bytes = 4096u;
inline constexpr std::size_t hard_max_feature_bytes = 128u;

struct SessionState;
struct SourceIdentity;
struct SourceData;
struct RevisionData;
struct VFSData;
struct TokenData;
struct ASTData;
struct CompilationData;

struct OptionsData {
    std::size_t max_source_bytes = on1x::max_source_bytes;
    std::size_t max_total_bytes = hard_max_total_bytes;
    std::size_t max_tokens = 262144u;
    std::size_t max_ast_nodes = hard_max_ast_nodes;
    std::size_t max_modules = hard_max_modules;
    std::size_t max_import_depth = hard_max_import_depth;
    std::size_t max_diagnostics = 64u;
    std::uint64_t work_budget = UINT64_C(32) * 1024u * 1024u;
    std::uint64_t deadline_milliseconds = 0;
    std::size_t max_overlays = hard_max_overlays;
    bool prelude = false;
    bool expand_macros = false;
    bool strict_utf8 = false;
    bool capture_module_graph = false;
    bool first_search_path_is_stdlib = false;
    std::vector<std::string> search_paths;
    std::vector<std::string> features;
    std::uint64_t revision = 0;

    bool operator==(const OptionsData &) const = default;
};

struct SessionState {
    std::thread::id owner = std::this_thread::get_id();
    std::atomic<bool> cancelled{false};
    std::atomic<bool> closed{false};
    std::atomic<std::uint64_t> revision{1};
    std::uint32_t generation = 1;
    std::uint32_t next_source_slot = 1;
    fs::path working_directory;
    On1xSDK_DiagnosticSink diagnostics{};
    On1xSDK_CompilerCondition condition = ON1X_SDK_COMPILER_CONDITION_NONE;
    std::string diagnostic_code;
    std::string diagnostic_message;
    std::size_t diagnostic_begin = SIZE_MAX;
    std::size_t diagnostic_end = SIZE_MAX;
};

struct RevisionData {
    std::shared_ptr<SessionState> state;
    std::shared_ptr<SourceIdentity> identity;
    std::uint64_t source_id = 0;
    std::uint64_t revision = 0;
    On1xSDK_SourceEncoding encoding = ON1X_SDK_SOURCE_BYTES;
    std::string path;
    std::string bytes;
    std::vector<std::size_t> lines;
};

struct SourceIdentity {
    std::uint64_t current_revision = 0;
};

struct SourceData {
    std::shared_ptr<SessionState> state;
    std::shared_ptr<SourceIdentity> identity;
    std::uint64_t source_id = 0;
    std::shared_ptr<RevisionData> current;
};

struct VFSData {
    std::shared_ptr<SessionState> state;
    std::map<std::string, std::shared_ptr<SourceData>> overlays;
    std::uint64_t revision = 0;
};

struct TokenData {
    std::shared_ptr<SessionState> state;
    std::shared_ptr<RevisionData> source;
    std::vector<on1x::Token> tokens;
    std::vector<on1x::Docstring> docstrings;
    std::uint32_t generation = 1;
};

struct NodeData {
    On1xSDK_ASTNodeCategory category = ON1X_SDK_AST_UNIT;
    On1xSDK_ASTNodeFlags flags = 0;
    std::string tag;
    std::string text;
    std::uint64_t hygiene_context = 0;
    on1x::Location location;
    std::uint64_t source_id = 0;
    std::uint64_t source_revision = 0;
    std::size_t byte_end = 0;
    std::vector<std::uint32_t> children;
};

struct DirectiveData {
    On1xSDK_DirectiveKind kind = ON1X_SDK_DIRECTIVE_FEATURE;
    std::uint32_t node = 0;
    std::string name;
    std::string value;
};

struct ASTData {
    std::shared_ptr<SessionState> state;
    std::shared_ptr<const on1x::Unit> unit;
    // Source-view projection only; never reintroduced into a linked Unit.
    std::vector<on1x::Import> source_operator_imports;
    std::vector<std::shared_ptr<RevisionData>> sources;
    std::map<std::string, std::shared_ptr<RevisionData>> source_by_path;
    std::vector<NodeData> nodes;
    std::vector<std::uint32_t> attributes;
    std::vector<std::uint32_t> macros;
    std::vector<DirectiveData> directives;
    std::uint32_t generation = 1;
    std::uint64_t revision = 0;
    On1xSDK_ASTValidationState validation = ON1X_SDK_AST_PARSED_CANDIDATE;
};

struct DependencyData {
    std::shared_ptr<RevisionData> source;
    bool is_entry = false;
    bool from_overlay = false;
};

struct CompilationData {
    std::shared_ptr<SessionState> state;
    std::shared_ptr<VFSData> vfs;
    std::shared_ptr<SourceData> entry;
    OptionsData options;
    std::shared_ptr<ASTData> ast;
    std::vector<DependencyData> dependencies;
    std::string import_graph, module_graph;
    std::uint64_t revision = 0;
    std::uint64_t session_revision = 0;
    std::uint64_t vfs_revision = 0;
    std::uint64_t entry_revision = 0;
};

struct StatusFailure {
    On1xSDK_Status status;
    On1xSDK_CompilerCondition condition;
    std::string code;
    std::string message;
    std::size_t begin = SIZE_MAX;
    std::size_t end = SIZE_MAX;
};

struct Operation {
    std::shared_ptr<SessionState> state;
    const OptionsData &options;
    std::uint64_t work;
    std::optional<std::chrono::steady_clock::time_point> deadline;

    Operation(std::shared_ptr<SessionState> owner, const OptionsData &limits);
    void poll() const;
    void charge(std::uint64_t amount);
};

std::uint64_t publish_revision(const std::shared_ptr<SessionState> &state);
std::uint32_t object_generation(const std::shared_ptr<SessionState> &state);
void require_owner(const std::shared_ptr<SessionState> &state, bool allow_closed = false);
void require_same(
    const std::shared_ptr<SessionState> &left,
    const std::shared_ptr<SessionState> &right);

[[noreturn]] void fail(
    On1xSDK_Status status,
    On1xSDK_CompilerCondition condition,
    std::string code,
    std::string message,
    std::size_t begin = SIZE_MAX,
    std::size_t end = SIZE_MAX);
[[noreturn]] void invalid(std::string message);
[[noreturn]] void not_found(std::string message);
[[noreturn]] void stale(std::string message);

void clear_error(const std::shared_ptr<SessionState> &state);
void record_error(
    const std::shared_ptr<SessionState> &state,
    On1xSDK_CompilerCondition condition,
    std::string_view code,
    std::string_view message,
    std::size_t begin = SIZE_MAX,
    std::size_t end = SIZE_MAX,
    bool emit = true) noexcept;
On1xSDK_Status caught(
    const std::shared_ptr<SessionState> &state,
    std::exception_ptr error) noexcept;

template <class Function>
On1xSDK_Status api(
    const std::shared_ptr<SessionState> &state,
    Function &&function,
    bool allow_closed = false) noexcept {
    try {
        require_owner(state, allow_closed);
        clear_error(state);
        function();
        return ON1X_SDK_OK;
    } catch (...) {
        return caught(state, std::current_exception());
    }
}

template <class T>
On1xSDK_Status retain_object(T *object) noexcept {
    if (!object) return ON1X_SDK_INVALID_ARGUMENT;
    auto count = object->references.load(std::memory_order_relaxed);
    while (count && count != std::numeric_limits<std::size_t>::max()) {
        if (object->references.compare_exchange_weak(
                count, count + 1u, std::memory_order_relaxed))
            return ON1X_SDK_OK;
    }
    return ON1X_SDK_LIMIT;
}

template <class T>
void release_object(T *object) noexcept {
    if (object && object->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete object;
}

template <class T>
void write_descriptor(T *output, const T &value) {
    if (!output) invalid("output descriptor is null");
    const auto abi = output->abi_version;
    const auto size = output->struct_size;
    if (size < sizeof(std::uint32_t) * 2u)
        fail(ON1X_SDK_ABI_MISMATCH, ON1X_SDK_COMPILER_INVALID_STATE,
             "ON1X-SDK-ABI", "descriptor is shorter than the ABI prefix");
    if (abi != ON1X_SDK_COMPILER_ABI_VERSION)
        fail(ON1X_SDK_ABI_MISMATCH, ON1X_SDK_COMPILER_INVALID_STATE,
             "ON1X-SDK-ABI", "compiler descriptor ABI version mismatch");
    std::memcpy(output, &value, std::min<std::size_t>(size, sizeof(T)));
    output->abi_version = ON1X_SDK_COMPILER_ABI_VERSION;
    output->struct_size = size;
}

bool valid_utf8(std::string_view bytes);
std::string checked_string(
    const char *data,
    std::size_t size,
    std::size_t maximum,
    const char *field,
    bool require_utf8 = true,
    bool allow_empty = false);
std::string checked_bytes(
    const void *data,
    std::size_t size,
    std::size_t maximum,
    const char *field);
std::string normalize_path(
    const std::shared_ptr<SessionState> &state,
    const char *data,
    std::size_t size);
std::string normalize_path(
    const std::shared_ptr<SessionState> &state,
    std::string_view path);

std::shared_ptr<RevisionData> make_revision(
    const std::shared_ptr<SessionState> &state,
    std::uint64_t source_id,
    std::string path,
    std::string bytes,
    On1xSDK_SourceEncoding encoding);
std::shared_ptr<SourceData> make_source(
    const std::shared_ptr<SessionState> &state,
    std::string path,
    std::string bytes,
    On1xSDK_SourceEncoding encoding);
void set_current_revision(
    const std::shared_ptr<SourceData> &source,
    std::shared_ptr<RevisionData> revision);
bool revision_is_current(const std::shared_ptr<RevisionData> &revision);
std::pair<std::size_t, std::size_t> line_column(
    const RevisionData &revision,
    std::size_t offset);

std::shared_ptr<TokenData> lex_impl(
    const std::shared_ptr<RevisionData> &source,
    const OptionsData &options);
std::shared_ptr<ASTData> parse_impl(
    const std::shared_ptr<TokenData> &tokens,
    const OptionsData &options);
std::shared_ptr<ASTData> parse_revision_impl(
    const std::shared_ptr<RevisionData> &source,
    const OptionsData &options);
std::shared_ptr<ASTData> build_ast(
    on1x::Unit unit,
    std::vector<std::shared_ptr<RevisionData>> sources,
    const OptionsData &options,
    On1xSDK_ASTValidationState validation,
    std::vector<on1x::Import> source_operator_imports = {});
std::shared_ptr<CompilationData> compile_impl(
    const std::shared_ptr<SessionState> &state,
    const std::shared_ptr<VFSData> &vfs,
    const OptionsData &options,
    const std::shared_ptr<SourceData> &entry);
bool compilation_current(const CompilationData &compilation);

On1xSDK_TokenKind classify_token(std::string_view text);
std::uint64_t node_id(const ASTData &ast, std::uint32_t index);
std::uint32_t node_index(const ASTData &ast, On1xSDK_ASTNode node);

} // namespace on1x::sdk2

struct On1xSDK_Session {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::SessionState> state;
};

struct On1xSDK_Options {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::SessionState> state;
    on1x::sdk2::OptionsData data;
};

struct On1xSDK_Source {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::SourceData> data;
};

struct On1xSDK_SourceSnapshot {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::RevisionData> data;
};

struct On1xSDK_VFS {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::VFSData> data;
};

struct On1xSDK_TokenStream {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::TokenData> data;
};

struct On1xSDK_AST {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::ASTData> data;
};

struct On1xSDK_ASTBuilder {
    std::shared_ptr<on1x::sdk2::SessionState> state;
    std::shared_ptr<on1x::sdk2::SourceData> source;
    on1x::sdk2::OptionsData options;
    std::uint64_t expected_revision = 0;
    std::string bytes;
    On1xSDK_SourceEncoding encoding = ON1X_SDK_SOURCE_BYTES;
    bool has_candidate = false;
};

struct On1xSDK_Compilation {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::CompilationData> data;
};

struct On1xSDK_CacheEntry {
    std::uint64_t source_id = 0;
    std::uint64_t source_revision = 0;
    on1x::sdk2::OptionsData options;
    std::shared_ptr<on1x::sdk2::ASTData> ast;
};

struct On1xSDK_Cache {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::SessionState> state;
    std::size_t capacity = 0;
    std::vector<On1xSDK_CacheEntry> entries;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

struct On1xSDK_Workspace {
    std::atomic<std::size_t> references{1};
    std::shared_ptr<on1x::sdk2::SessionState> state;
    std::string root;
    On1xSDK_VFS *vfs = nullptr;
    On1xSDK_Options *options = nullptr;
};

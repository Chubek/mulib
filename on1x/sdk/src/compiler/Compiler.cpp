#include "FrontendInternal.hpp"

#include "On1xSDK-SourceMap.h"

#include <on1x/extensions.hpp>

#include <array>
#include <fstream>
#include <new>
#include <system_error>

namespace on1x::sdk2 {
namespace {

std::atomic<std::uint32_t> next_session_generation{1};
std::atomic<std::uint32_t> next_global_object_generation{1};

std::uint32_t take_generation(
    std::atomic<std::uint32_t> &counter) noexcept {
    auto value = counter.load(std::memory_order_relaxed);
    while (value && value < UINT32_MAX) {
        if (counter.compare_exchange_weak(
                value, value + 1u, std::memory_order_relaxed))
            return value;
    }
    return 0;
}

bool limit_error(const on1x::Error &error) {
    const std::string_view id = error.diagnostic_id;
    const std::string_view message = error.what();
    return id.find("LIMIT") != std::string_view::npos ||
           id.find("ALLOCATION") != std::string_view::npos ||
           message.find("limit") != std::string_view::npos ||
           message.find("exceeds") != std::string_view::npos ||
           message.find("too many") != std::string_view::npos ||
           message.find("budget") != std::string_view::npos ||
           message.find("allocation failure") != std::string_view::npos;
}

std::string read_bounded_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_IO_FAILURE,
             "ON1X-SDK-IO", "cannot open source file: " + path);
    std::string bytes;
    std::array<char, 8192> chunk{};
    while (input) {
        const auto remaining = on1x::max_source_bytes + 1u - bytes.size();
        const auto count = std::min(chunk.size(), remaining);
        input.read(chunk.data(), static_cast<std::streamsize>(count));
        bytes.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
        if (bytes.size() > on1x::max_source_bytes)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-LEX-LIMIT", "source exceeds 2 MiB limit");
    }
    if (input.bad())
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_IO_FAILURE,
             "ON1X-SDK-IO", "cannot read source file: " + path);
    return bytes;
}

void validate_encoding(std::string_view bytes, On1xSDK_SourceEncoding encoding) {
    if (encoding != ON1X_SDK_SOURCE_BYTES && encoding != ON1X_SDK_SOURCE_UTF8)
        invalid("unknown source encoding policy");
    if (encoding == ON1X_SDK_SOURCE_UTF8 && !valid_utf8(bytes))
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_VERIFICATION_FAILED,
             "ON1X-SDK-UTF8", "source is not valid shortest-form UTF-8");
}

void fill_source_info(
    const std::shared_ptr<RevisionData> &revision,
    On1xSDK_SourceInfo *output) {
    On1xSDK_SourceInfo value = ON1X_SDK_COMPILER_INIT(On1xSDK_SourceInfo);
    value.source_id = revision->source_id;
    value.revision = revision->revision;
    value.encoding = revision->encoding;
    value.is_current = revision_is_current(revision) ? 1u : 0u;
    value.path = revision->path.data();
    value.path_size = revision->path.size();
    value.byte_size = revision->bytes.size();
    write_descriptor(output, value);
}

std::uint64_t &limit_field(OptionsData &options, On1xSDK_Option option) {
    switch (option) {
    case ON1X_SDK_OPTION_WORK_BUDGET: return options.work_budget;
    case ON1X_SDK_OPTION_DEADLINE_MILLISECONDS:
        return options.deadline_milliseconds;
    default: invalid("option is not a 64-bit limit");
    }
}

const std::uint64_t &limit_field(
    const OptionsData &options,
    On1xSDK_Option option) {
    switch (option) {
    case ON1X_SDK_OPTION_WORK_BUDGET: return options.work_budget;
    case ON1X_SDK_OPTION_DEADLINE_MILLISECONDS:
        return options.deadline_milliseconds;
    default: invalid("option is not a 64-bit limit");
    }
}

std::size_t &size_limit_field(OptionsData &options, On1xSDK_Option option) {
    switch (option) {
    case ON1X_SDK_OPTION_MAX_SOURCE_BYTES: return options.max_source_bytes;
    case ON1X_SDK_OPTION_MAX_TOTAL_BYTES: return options.max_total_bytes;
    case ON1X_SDK_OPTION_MAX_TOKENS: return options.max_tokens;
    case ON1X_SDK_OPTION_MAX_AST_NODES: return options.max_ast_nodes;
    case ON1X_SDK_OPTION_MAX_MODULES: return options.max_modules;
    case ON1X_SDK_OPTION_MAX_IMPORT_DEPTH: return options.max_import_depth;
    case ON1X_SDK_OPTION_MAX_DIAGNOSTICS: return options.max_diagnostics;
    case ON1X_SDK_OPTION_MAX_OVERLAYS: return options.max_overlays;
    default: invalid("unknown size limit option");
    }
}

const std::size_t &size_limit_field(
    const OptionsData &options,
    On1xSDK_Option option) {
    switch (option) {
    case ON1X_SDK_OPTION_MAX_SOURCE_BYTES: return options.max_source_bytes;
    case ON1X_SDK_OPTION_MAX_TOTAL_BYTES: return options.max_total_bytes;
    case ON1X_SDK_OPTION_MAX_TOKENS: return options.max_tokens;
    case ON1X_SDK_OPTION_MAX_AST_NODES: return options.max_ast_nodes;
    case ON1X_SDK_OPTION_MAX_MODULES: return options.max_modules;
    case ON1X_SDK_OPTION_MAX_IMPORT_DEPTH: return options.max_import_depth;
    case ON1X_SDK_OPTION_MAX_DIAGNOSTICS: return options.max_diagnostics;
    case ON1X_SDK_OPTION_MAX_OVERLAYS: return options.max_overlays;
    default: invalid("unknown size limit option");
    }
}

std::uint64_t option_maximum(On1xSDK_Option option) {
    switch (option) {
    case ON1X_SDK_OPTION_MAX_SOURCE_BYTES: return on1x::max_source_bytes;
    case ON1X_SDK_OPTION_MAX_TOTAL_BYTES: return hard_max_total_bytes;
    case ON1X_SDK_OPTION_MAX_TOKENS: return hard_max_tokens;
    case ON1X_SDK_OPTION_MAX_AST_NODES: return hard_max_ast_nodes;
    case ON1X_SDK_OPTION_MAX_MODULES: return hard_max_modules;
    case ON1X_SDK_OPTION_MAX_IMPORT_DEPTH: return hard_max_import_depth;
    case ON1X_SDK_OPTION_MAX_DIAGNOSTICS: return hard_max_diagnostics;
    case ON1X_SDK_OPTION_WORK_BUDGET: return UINT64_C(1) << 36;
    case ON1X_SDK_OPTION_DEADLINE_MILLISECONDS: return UINT64_C(3600000);
    case ON1X_SDK_OPTION_MAX_OVERLAYS: return hard_max_overlays;
    default: invalid("unknown limit option");
    }
}

} // namespace

Operation::Operation(
    std::shared_ptr<SessionState> owner,
    const OptionsData &limits)
    : state(std::move(owner)), options(limits), work(limits.work_budget) {
    if (limits.deadline_milliseconds)
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(limits.deadline_milliseconds);
    poll();
}

void Operation::poll() const {
    if (state->cancelled.load(std::memory_order_acquire))
        fail(ON1X_SDK_BUSY, ON1X_SDK_COMPILER_CANCELLED,
             "ON1X-SDK-CANCELLED", "compiler operation was cancelled");
    if (deadline && std::chrono::steady_clock::now() >= *deadline)
        fail(ON1X_SDK_BUSY, ON1X_SDK_COMPILER_DEADLINE_EXPIRED,
             "ON1X-SDK-DEADLINE", "compiler operation deadline expired");
}

void Operation::charge(std::uint64_t amount) {
    poll();
    if (amount > work)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-WORK-LIMIT", "deterministic work budget exhausted");
    work -= amount;
}

std::uint64_t publish_revision(const std::shared_ptr<SessionState> &state) {
    auto before = state->revision.load(std::memory_order_relaxed);
    while (before < UINT64_MAX) {
        if (state->revision.compare_exchange_weak(
                before, before + 1u, std::memory_order_relaxed))
            return before + 1u;
    }
    fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
         "ON1X-SDK-REVISION", "session revision space exhausted");
}

std::uint32_t object_generation(const std::shared_ptr<SessionState> &) {
    const auto value = take_generation(next_global_object_generation);
    if (!value)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_INVALID_STATE,
             "ON1X-SDK-HANDLE-LIMIT", "object generation space exhausted");
    return value;
}

void require_owner(
    const std::shared_ptr<SessionState> &state,
    bool allow_closed) {
    if (!state) invalid("object has no owning session");
    if (state->owner != std::this_thread::get_id())
        invalid("operation called from a thread other than the session owner");
    if (!allow_closed && state->closed.load(std::memory_order_acquire))
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
             "ON1X-SDK-LIFECYCLE", "owning session has been released");
}

void require_same(
    const std::shared_ptr<SessionState> &left,
    const std::shared_ptr<SessionState> &right) {
    if (left != right)
        fail(ON1X_SDK_INVALID_ARGUMENT, ON1X_SDK_COMPILER_CROSS_SESSION,
             "ON1X-SDK-CROSS-SESSION",
             "objects belong to different compiler sessions");
}

[[noreturn]] void fail(
    On1xSDK_Status status,
    On1xSDK_CompilerCondition condition,
    std::string code,
    std::string message,
    std::size_t begin,
    std::size_t end) {
    throw StatusFailure{
        status, condition, std::move(code), std::move(message), begin, end};
}

[[noreturn]] void invalid(std::string message) {
    fail(ON1X_SDK_INVALID_ARGUMENT, ON1X_SDK_COMPILER_CONDITION_NONE,
         "ON1X-SDK-ARGUMENT", std::move(message));
}

[[noreturn]] void not_found(std::string message) {
    fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_NOT_FOUND,
         "ON1X-SDK-NOT-FOUND", std::move(message));
}

[[noreturn]] void stale(std::string message) {
    fail(ON1X_SDK_BUSY, ON1X_SDK_COMPILER_STALE_REVISION,
         "ON1X-SDK-STALE-REVISION", std::move(message));
}

void clear_error(const std::shared_ptr<SessionState> &state) {
    state->condition = ON1X_SDK_COMPILER_CONDITION_NONE;
    state->diagnostic_code.clear();
    state->diagnostic_message.clear();
    state->diagnostic_begin = SIZE_MAX;
    state->diagnostic_end = SIZE_MAX;
}

void record_error(
    const std::shared_ptr<SessionState> &state,
    On1xSDK_CompilerCondition condition,
    std::string code,
    std::string message,
    std::size_t begin,
    std::size_t end,
    bool emit) noexcept {
    if (!state) return;
    try {
        state->condition = condition;
        state->diagnostic_code = std::move(code);
        state->diagnostic_message = std::move(message);
        state->diagnostic_begin = begin;
        state->diagnostic_end = end;
        if (emit &&
            !state->closed.load(std::memory_order_acquire) &&
            state->diagnostics.emit) {
            On1xSDK_Diagnostic diagnostic{};
            diagnostic.severity = ON1X_SDK_DIAGNOSTIC_ERROR;
            diagnostic.code = state->diagnostic_code.c_str();
            diagnostic.message = state->diagnostic_message.c_str();
            diagnostic.byte_begin = begin;
            diagnostic.byte_end = end;
            try {
                state->diagnostics.emit(
                    state->diagnostics.userdata, &diagnostic);
            } catch (...) {
                /* A provider violated the no-unwind callback contract. */
                state->condition = ON1X_SDK_COMPILER_INVALID_STATE;
                state->diagnostic_code = "ON1X-SDK-CALLBACK";
                state->diagnostic_message =
                    "diagnostic callback unwound through the C boundary";
                state->diagnostic_begin = SIZE_MAX;
                state->diagnostic_end = SIZE_MAX;
            }
        }
    } catch (...) {
        /* Error recording itself must not cross the C boundary. */
    }
}

On1xSDK_Status caught(
    const std::shared_ptr<SessionState> &state,
    std::exception_ptr error) noexcept {
    try {
        std::rethrow_exception(error);
    } catch (const StatusFailure &failure) {
        record_error(state, failure.condition, failure.code, failure.message,
                     failure.begin, failure.end);
        return failure.status;
    } catch (const on1x::Error &failure) {
        const bool limited = limit_error(failure);
        record_error(
            state,
            limited ? ON1X_SDK_COMPILER_CONDITION_NONE
                    : ON1X_SDK_COMPILER_VERIFICATION_FAILED,
            failure.diagnostic_id.empty() ? "ON1X-SDK-FRONTEND"
                                          : failure.diagnostic_id,
            failure.what(), failure.where.offset, failure.where.offset);
        return limited ? ON1X_SDK_LIMIT : ON1X_SDK_ERROR;
    } catch (const std::bad_alloc &) {
        record_error(state, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-ALLOCATION", "compiler allocation failed");
        return ON1X_SDK_LIMIT;
    } catch (const std::length_error &failure) {
        record_error(state, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-LIMIT", failure.what());
        return ON1X_SDK_LIMIT;
    } catch (const fs::filesystem_error &failure) {
        record_error(state, ON1X_SDK_COMPILER_IO_FAILURE,
                     "ON1X-SDK-IO", failure.what());
        return ON1X_SDK_ERROR;
    } catch (const std::exception &failure) {
        record_error(state, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-ERROR", failure.what());
        return ON1X_SDK_ERROR;
    } catch (...) {
        record_error(state, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-ERROR", "unknown compiler failure");
        return ON1X_SDK_ERROR;
    }
}

bool valid_utf8(std::string_view bytes) {
    for (std::size_t i = 0; i < bytes.size();) {
        const auto first = static_cast<unsigned char>(bytes[i++]);
        if (first < 0x80u) continue;
        std::uint32_t value = 0;
        std::size_t remaining = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xc2u && first <= 0xdfu) {
            value = first & 0x1fu; remaining = 1; minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            value = first & 0x0fu; remaining = 2; minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            value = first & 0x07u; remaining = 3; minimum = 0x10000u;
        } else return false;
        if (remaining > bytes.size() - i) return false;
        for (std::size_t n = 0; n < remaining; ++n) {
            const auto next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xc0u) != 0x80u) return false;
            value = (value << 6u) | (next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffffu ||
            (value >= 0xd800u && value <= 0xdfffu))
            return false;
    }
    return true;
}

std::string checked_string(
    const char *data,
    std::size_t size,
    std::size_t maximum,
    const char *field,
    bool require_utf8,
    bool allow_empty) {
    if (!data && size) invalid(std::string(field) + " has a null nonempty view");
    if (!allow_empty && !size) invalid(std::string(field) + " is empty");
    if (size > maximum) invalid(std::string(field) + " exceeds its byte limit");
    std::string value(data ? data : "", size);
    if (value.find('\0') != std::string::npos)
        invalid(std::string(field) + " contains NUL");
    if (require_utf8 && !valid_utf8(value))
        invalid(std::string(field) + " is not valid UTF-8");
    return value;
}

std::string checked_bytes(
    const void *data,
    std::size_t size,
    std::size_t maximum,
    const char *field) {
    if (!data && size) invalid(std::string(field) + " has a null nonempty view");
    if (size > maximum)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-BYTE-LIMIT",
             std::string(field) + " exceeds its byte limit");
    return std::string(
        size ? static_cast<const char *>(data) : "",
        size);
}

std::string normalize_path(
    const std::shared_ptr<SessionState> &state,
    const char *data,
    std::size_t size) {
    return normalize_path(
        state,
        checked_string(data, size, hard_max_path_bytes, "path", true, false));
}

std::string normalize_path(
    const std::shared_ptr<SessionState> &state,
    std::string_view path) {
    if (path.empty() || path.size() > hard_max_path_bytes || !valid_utf8(path))
        invalid("invalid UTF-8 path");
    fs::path value{std::string(path)};
    if (value.is_relative()) value = state->working_directory / value;
    std::error_code error;
    auto normalized = fs::weakly_canonical(value, error);
    if (error) normalized = value.lexically_normal();
    auto text = normalized.generic_string();
    if (text.empty() || text.size() > hard_max_path_bytes)
        invalid("normalized path exceeds its byte limit");
    if (!valid_utf8(text)) invalid("normalized path is not valid UTF-8");
    return text;
}

std::shared_ptr<RevisionData> make_revision(
    const std::shared_ptr<SessionState> &state,
    std::uint64_t source_id,
    std::string path,
    std::string bytes,
    On1xSDK_SourceEncoding encoding) {
    if (bytes.size() > on1x::max_source_bytes)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-LEX-LIMIT", "source exceeds 2 MiB limit");
    validate_encoding(bytes, encoding);
    auto revision = std::make_shared<RevisionData>();
    revision->state = state;
    revision->source_id = source_id;
    revision->revision = publish_revision(state);
    revision->encoding = encoding;
    revision->path = std::move(path);
    revision->bytes = std::move(bytes);
    revision->lines.push_back(0);
    for (std::size_t i = 0; i < revision->bytes.size(); ++i)
        if (revision->bytes[i] == '\n') revision->lines.push_back(i + 1u);
    return revision;
}

std::shared_ptr<SourceData> make_source(
    const std::shared_ptr<SessionState> &state,
    std::string path,
    std::string bytes,
    On1xSDK_SourceEncoding encoding) {
    if (!state->next_source_slot ||
        state->next_source_slot == UINT32_MAX)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_INVALID_STATE,
             "ON1X-SDK-HANDLE-LIMIT", "source handle space exhausted");
    const auto slot = state->next_source_slot++;
    const auto id = (static_cast<std::uint64_t>(state->generation) << 32u) |
                    slot;
    auto source = std::make_shared<SourceData>();
    source->state = state;
    source->identity = std::make_shared<SourceIdentity>();
    source->source_id = id;
    auto revision = make_revision(
        state, id, std::move(path), std::move(bytes), encoding);
    set_current_revision(source, std::move(revision));
    return source;
}

void set_current_revision(
    const std::shared_ptr<SourceData> &source,
    std::shared_ptr<RevisionData> revision) {
    if (!source || !revision ||
        source->state != revision->state ||
        source->source_id != revision->source_id)
        invalid("source revision identity mismatch");
    revision->identity = source->identity;
    source->current = std::move(revision);
    source->identity->current_revision = source->current->revision;
}

bool revision_is_current(const std::shared_ptr<RevisionData> &revision) {
    return revision->identity &&
           revision->identity->current_revision == revision->revision;
}

std::pair<std::size_t, std::size_t> line_column(
    const RevisionData &revision,
    std::size_t offset) {
    if (offset > revision.bytes.size()) invalid("source offset is out of range");
    const auto found = std::upper_bound(
        revision.lines.begin(), revision.lines.end(), offset);
    const auto line_index = static_cast<std::size_t>(
        found - revision.lines.begin() - 1);
    return {line_index + 1u, offset - revision.lines[line_index] + 1u};
}

} // namespace on1x::sdk2

using namespace on1x::sdk2;

extern "C" {

On1xSDK_Status on1x_sdk_compiler_info(On1xSDK_CompilerInfo *out_info) {
    try {
        On1xSDK_CompilerInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_CompilerInfo);
        value.capabilities =
            ON1X_SDK_COMPILER_CAP_SESSIONS |
            ON1X_SDK_COMPILER_CAP_VFS |
            ON1X_SDK_COMPILER_CAP_CANONICAL_LEXER |
            ON1X_SDK_COMPILER_CAP_CANONICAL_PARSER |
            ON1X_SDK_COMPILER_CAP_AST_VIEW |
            ON1X_SDK_COMPILER_CAP_SOURCE_CANDIDATE |
            ON1X_SDK_COMPILER_CAP_PPX_V1_READMISSION |
            ON1X_SDK_COMPILER_CAP_MACRO_EXPANSION |
            ON1X_SDK_COMPILER_CAP_MODULE_LOADING |
            ON1X_SDK_COMPILER_CAP_INCREMENTAL_REVISIONS;
        value.frontend_schema_version = ON1X_SDK_FRONTEND_SCHEMA_VERSION;
        value.ppx_schema_version = ON1X_SDK_PPX_SCHEMA_VERSION;
#ifdef ON1X_SDK_GENERATED_SCANNER
        static constexpr char scanner[] =
            "LibStone ASDL+Aurocks generated frontend";
#else
        static constexpr char scanner[] =
            "portable fallback scanner (bootstrap only)";
#endif
        static constexpr char schema[] = ON1X_SDK_PPX_SCHEMA_FORMAT;
        value.scanner_profile = scanner;
        value.scanner_profile_size = sizeof(scanner) - 1u;
        value.ppx_schema_format = schema;
        value.ppx_schema_format_size = sizeof(schema) - 1u;
        write_descriptor(out_info, value);
        return ON1X_SDK_OK;
    } catch (const StatusFailure &failure) {
        return failure.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_compiler_allocator_supported(
    uint32_t *out_supported) {
    if (!out_supported) return ON1X_SDK_INVALID_ARGUMENT;
    *out_supported = 0;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_session_create(
    const On1xSDK_DiagnosticSink *diagnostics,
    On1xSDK_Session **out_session) {
    if (!out_session) return ON1X_SDK_INVALID_ARGUMENT;
    *out_session = nullptr;
    try {
        auto state = std::make_shared<SessionState>();
        const auto generation = take_generation(next_session_generation);
        if (!generation) return ON1X_SDK_LIMIT;
        state->generation = generation;
        std::error_code error;
        state->working_directory = fs::current_path(error);
        if (error) return ON1X_SDK_ERROR;
        if (diagnostics) state->diagnostics = *diagnostics;
        auto session = std::make_unique<On1xSDK_Session>();
        session->state = std::move(state);
        *out_session = session.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_session_retain(On1xSDK_Session *session) {
    return retain_object(session);
}

void on1x_sdk_session_release(On1xSDK_Session *session) {
    if (!session) return;
    if (session->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        session->state->closed.store(true, std::memory_order_release);
        delete session;
    }
}

On1xSDK_Status on1x_sdk_session_set_diagnostics(
    On1xSDK_Session *session,
    const On1xSDK_DiagnosticSink *diagnostics) {
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        session->state->diagnostics =
            diagnostics ? *diagnostics : On1xSDK_DiagnosticSink{};
    });
}

On1xSDK_Status on1x_sdk_session_request_cancel(On1xSDK_Session *session) {
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    if (session->state->closed.load(std::memory_order_acquire))
        return ON1X_SDK_ERROR;
    session->state->cancelled.store(true, std::memory_order_release);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_session_reset_cancel(On1xSDK_Session *session) {
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        session->state->cancelled.store(false, std::memory_order_release);
    });
}

On1xSDK_Status on1x_sdk_session_is_cancelled(
    const On1xSDK_Session *session,
    uint32_t *out_cancelled) {
    if (!session || !out_cancelled) return ON1X_SDK_INVALID_ARGUMENT;
    *out_cancelled =
        session->state->cancelled.load(std::memory_order_acquire) ? 1u : 0u;
    return ON1X_SDK_OK;
}

uint64_t on1x_sdk_session_revision(const On1xSDK_Session *session) {
    return session ? session->state->revision.load(std::memory_order_relaxed) : 0;
}

On1xSDK_CompilerCondition on1x_sdk_session_last_condition(
    const On1xSDK_Session *session) {
    return session ? session->state->condition
                   : static_cast<On1xSDK_CompilerCondition>(
                         ON1X_SDK_COMPILER_INVALID_STATE);
}

On1xSDK_Status on1x_sdk_session_last_diagnostic(
    const On1xSDK_Session *session,
    On1xSDK_Diagnostic *out_diagnostic) {
    if (!session || !out_diagnostic) return ON1X_SDK_INVALID_ARGUMENT;
    try {
        require_owner(session->state, true);
        if (session->state->diagnostic_message.empty())
            return ON1X_SDK_ERROR;
        out_diagnostic->severity = ON1X_SDK_DIAGNOSTIC_ERROR;
        out_diagnostic->code = session->state->diagnostic_code.c_str();
        out_diagnostic->message = session->state->diagnostic_message.c_str();
        out_diagnostic->byte_begin = session->state->diagnostic_begin;
        out_diagnostic->byte_end = session->state->diagnostic_end;
        return ON1X_SDK_OK;
    } catch (...) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
}

On1xSDK_Status on1x_sdk_session_clear_diagnostic(
    On1xSDK_Session *session) {
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    try {
        require_owner(session->state, true);
        clear_error(session->state);
        return ON1X_SDK_OK;
    } catch (...) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
}

On1xSDK_Status on1x_sdk_options_create(
    On1xSDK_Session *session,
    On1xSDK_Options **out_options) {
    if (!out_options) return ON1X_SDK_INVALID_ARGUMENT;
    *out_options = nullptr;
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        auto options = std::make_unique<On1xSDK_Options>();
        options->state = session->state;
        options->data.revision = publish_revision(session->state);
        *out_options = options.release();
    });
}

On1xSDK_Status on1x_sdk_options_clone(
    const On1xSDK_Options *options,
    On1xSDK_Options **out_options) {
    if (!out_options) return ON1X_SDK_INVALID_ARGUMENT;
    *out_options = nullptr;
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto copy = std::make_unique<On1xSDK_Options>();
        copy->state = options->state;
        copy->data = options->data;
        copy->data.revision = publish_revision(options->state);
        *out_options = copy.release();
    });
}

On1xSDK_Status on1x_sdk_options_retain(On1xSDK_Options *options) {
    return retain_object(options);
}

void on1x_sdk_options_release(On1xSDK_Options *options) {
    release_object(options);
}

On1xSDK_Status on1x_sdk_options_set_limit(
    On1xSDK_Options *options,
    On1xSDK_Option option,
    uint64_t value) {
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        if (value > option_maximum(option))
            invalid("option value exceeds the supported finite maximum");
        auto next = options->data;
        if (option == ON1X_SDK_OPTION_WORK_BUDGET ||
            option == ON1X_SDK_OPTION_DEADLINE_MILLISECONDS)
            limit_field(next, option) = value;
        else
            size_limit_field(next, option) =
                static_cast<std::size_t>(value);
        if (next.max_source_bytes > next.max_total_bytes)
            invalid("source byte limit exceeds total byte limit");
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

On1xSDK_Status on1x_sdk_options_get_limit(
    const On1xSDK_Options *options,
    On1xSDK_Option option,
    uint64_t *out_value) {
    if (!options || !out_value) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        if (option == ON1X_SDK_OPTION_WORK_BUDGET ||
            option == ON1X_SDK_OPTION_DEADLINE_MILLISECONDS)
            *out_value = limit_field(options->data, option);
        else
            *out_value = size_limit_field(options->data, option);
    });
}

On1xSDK_Status on1x_sdk_options_set_boolean(
    On1xSDK_Options *options,
    On1xSDK_BooleanOption option,
    uint32_t value) {
    if (!options || value > 1u) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto next = options->data;
        switch (option) {
        case ON1X_SDK_OPTION_PRELUDE:
            next.prelude = value != 0; break;
        case ON1X_SDK_OPTION_EXPAND_MACROS:
            next.expand_macros = value != 0; break;
        case ON1X_SDK_OPTION_STRICT_UTF8:
            next.strict_utf8 = value != 0; break;
        default: invalid("unknown boolean option");
        }
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

On1xSDK_Status on1x_sdk_options_get_boolean(
    const On1xSDK_Options *options,
    On1xSDK_BooleanOption option,
    uint32_t *out_value) {
    if (!options || !out_value) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        switch (option) {
        case ON1X_SDK_OPTION_PRELUDE:
            *out_value = options->data.prelude; break;
        case ON1X_SDK_OPTION_EXPAND_MACROS:
            *out_value = options->data.expand_macros; break;
        case ON1X_SDK_OPTION_STRICT_UTF8:
            *out_value = options->data.strict_utf8; break;
        default: invalid("unknown boolean option");
        }
    });
}

On1xSDK_Status on1x_sdk_options_add_search_path(
    On1xSDK_Options *options,
    const char *path,
    size_t path_size) {
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto normalized = normalize_path(options->state, path, path_size);
        auto next = options->data;
        if (std::find(next.search_paths.begin(), next.search_paths.end(),
                      normalized) == next.search_paths.end()) {
            if (next.search_paths.size() >= 256u)
                fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-PATH-LIMIT",
                     "search path count limit exceeded");
            next.search_paths.push_back(std::move(normalized));
        }
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

On1xSDK_Status on1x_sdk_options_enable_feature(
    On1xSDK_Options *options,
    const char *name,
    size_t name_size) {
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto value = checked_string(
            name, name_size, hard_max_feature_bytes, "feature name");
        value = on1x::extensions::canonical(value, {"<sdk-options>"});
        auto next = options->data;
        if (std::find(next.features.begin(), next.features.end(), value) ==
            next.features.end()) {
            if (next.features.size() >= 128u)
                fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                     "ON1X-SDK-FEATURE-LIMIT",
                     "feature count limit exceeded");
            next.features.push_back(std::move(value));
        }
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

On1xSDK_Status on1x_sdk_options_clear_search_paths(
    On1xSDK_Options *options) {
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto next = options->data;
        next.search_paths.clear();
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

On1xSDK_Status on1x_sdk_options_clear_features(
    On1xSDK_Options *options) {
    if (!options) return ON1X_SDK_INVALID_ARGUMENT;
    return api(options->state, [&] {
        auto next = options->data;
        next.features.clear();
        next.revision = publish_revision(options->state);
        options->data = std::move(next);
    });
}

uint64_t on1x_sdk_options_revision(const On1xSDK_Options *options) {
    return options ? options->data.revision : 0;
}

On1xSDK_Status on1x_sdk_source_create(
    On1xSDK_Session *session,
    const char *path,
    size_t path_size,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source) {
    if (!out_source) return ON1X_SDK_INVALID_ARGUMENT;
    *out_source = nullptr;
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        auto text = checked_bytes(
            bytes, byte_size, on1x::max_source_bytes, "source bytes");
        validate_encoding(text, encoding);
        auto source = std::make_unique<On1xSDK_Source>();
        source->data = make_source(
            session->state,
            normalize_path(session->state, path, path_size),
            std::move(text), encoding);
        *out_source = source.release();
    });
}

On1xSDK_Status on1x_sdk_source_from_file(
    On1xSDK_Session *session,
    const char *path,
    size_t path_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source) {
    if (!out_source) return ON1X_SDK_INVALID_ARGUMENT;
    *out_source = nullptr;
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        auto normalized = normalize_path(session->state, path, path_size);
        auto bytes = read_bounded_file(normalized);
        validate_encoding(bytes, encoding);
        auto source = std::make_unique<On1xSDK_Source>();
        source->data = make_source(
            session->state, std::move(normalized), std::move(bytes), encoding);
        *out_source = source.release();
    });
}

On1xSDK_Status on1x_sdk_source_retain(On1xSDK_Source *source) {
    return retain_object(source);
}

void on1x_sdk_source_release(On1xSDK_Source *source) {
    release_object(source);
}

On1xSDK_Status on1x_sdk_source_replace(
    On1xSDK_Source *source,
    uint64_t expected_revision,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_SourceRevision **out_revision) {
    if (!out_revision) return ON1X_SDK_INVALID_ARGUMENT;
    *out_revision = nullptr;
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        if (source->data->current->revision != expected_revision)
            stale("source changed before replacement");
        auto text = checked_bytes(
            bytes, byte_size, on1x::max_source_bytes, "source bytes");
        validate_encoding(text, encoding);
        auto view = std::make_unique<On1xSDK_SourceRevision>();
        auto next = make_revision(
            source->data->state, source->data->source_id,
            source->data->current->path, std::move(text), encoding);
        next->identity = source->data->identity;
        view->data = next;
        set_current_revision(source->data, next);
        *out_revision = view.release();
    });
}

On1xSDK_Status on1x_sdk_source_snapshot(
    const On1xSDK_Source *source,
    On1xSDK_SourceRevision **out_revision) {
    if (!out_revision) return ON1X_SDK_INVALID_ARGUMENT;
    *out_revision = nullptr;
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        auto view = std::make_unique<On1xSDK_SourceRevision>();
        view->data = source->data->current;
        *out_revision = view.release();
    });
}

On1xSDK_Status on1x_sdk_source_info(
    const On1xSDK_Source *source,
    On1xSDK_SourceInfo *out_info) {
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        fill_source_info(source->data->current, out_info);
    });
}

On1xSDK_Status on1x_sdk_source_revision_retain(
    On1xSDK_SourceRevision *revision) {
    return retain_object(revision);
}

void on1x_sdk_source_revision_release(On1xSDK_SourceRevision *revision) {
    release_object(revision);
}

On1xSDK_Status on1x_sdk_source_revision_info(
    const On1xSDK_SourceRevision *revision,
    On1xSDK_SourceInfo *out_info) {
    if (!revision) return ON1X_SDK_INVALID_ARGUMENT;
    return api(revision->data->state, [&] {
        fill_source_info(revision->data, out_info);
    }, true);
}

On1xSDK_Status on1x_sdk_source_revision_bytes(
    const On1xSDK_SourceRevision *revision,
    const void **out_bytes,
    size_t *out_size) {
    if (out_bytes) *out_bytes = nullptr;
    if (out_size) *out_size = 0;
    if (!revision || !out_bytes || !out_size)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(revision->data->state, [&] {
        *out_bytes = revision->data->bytes.data();
        *out_size = revision->data->bytes.size();
    }, true);
}

On1xSDK_Status on1x_sdk_source_revision_is_current(
    const On1xSDK_SourceRevision *revision,
    uint32_t *out_current) {
    if (!revision || !out_current) return ON1X_SDK_INVALID_ARGUMENT;
    return api(revision->data->state, [&] {
        *out_current = revision_is_current(revision->data) ? 1u : 0u;
    }, true);
}

On1xSDK_Status on1x_sdk_source_location_at(
    const On1xSDK_SourceRevision *revision,
    size_t byte_offset,
    On1xSDK_SourceLocation *out_location) {
    if (!revision) return ON1X_SDK_INVALID_ARGUMENT;
    return api(revision->data->state, [&] {
        auto [line, column] = line_column(*revision->data, byte_offset);
        On1xSDK_SourceLocation value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_SourceLocation);
        value.source_id = revision->data->source_id;
        value.revision = revision->data->revision;
        value.byte_offset = byte_offset;
        value.line = line;
        value.column = column;
        value.path = revision->data->path.data();
        value.path_size = revision->data->path.size();
        write_descriptor(out_location, value);
    }, true);
}

On1xSDK_Status on1x_sdk_source_validate_span(
    const On1xSDK_SourceRevision *revision,
    const On1xSDK_SourceSpan *span) {
    if (!revision || !span) return ON1X_SDK_INVALID_ARGUMENT;
    return api(revision->data->state, [&] {
        if (span->struct_size < sizeof(On1xSDK_SourceSpan) ||
            span->abi_version != ON1X_SDK_COMPILER_ABI_VERSION)
            fail(ON1X_SDK_ABI_MISMATCH,
                 ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-ABI", "source span ABI mismatch");
        if (span->source_id != revision->data->source_id ||
            span->revision != revision->data->revision)
            stale("source span names a different immutable revision");
        if (span->byte_begin > span->byte_end ||
            span->byte_end > revision->data->bytes.size())
            invalid("source span is outside its immutable revision");
    }, true);
}

On1xSDK_Status on1x_sdk_vfs_create(
    On1xSDK_Session *session,
    On1xSDK_VFS **out_vfs) {
    if (!out_vfs) return ON1X_SDK_INVALID_ARGUMENT;
    *out_vfs = nullptr;
    if (!session) return ON1X_SDK_INVALID_ARGUMENT;
    return api(session->state, [&] {
        auto vfs = std::make_unique<On1xSDK_VFS>();
        vfs->data = std::make_shared<VFSData>();
        vfs->data->state = session->state;
        vfs->data->revision = publish_revision(session->state);
        *out_vfs = vfs.release();
    });
}

On1xSDK_Status on1x_sdk_vfs_retain(On1xSDK_VFS *vfs) {
    return retain_object(vfs);
}

void on1x_sdk_vfs_release(On1xSDK_VFS *vfs) {
    release_object(vfs);
}

On1xSDK_Status on1x_sdk_vfs_set(
    On1xSDK_VFS *vfs,
    On1xSDK_Source *source) {
    if (!vfs || !source) return ON1X_SDK_INVALID_ARGUMENT;
    return api(vfs->data->state, [&] {
        require_same(vfs->data->state, source->data->state);
        const auto &path = source->data->current->path;
        if (!vfs->data->overlays.contains(path) &&
            vfs->data->overlays.size() >= hard_max_overlays)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-VFS-LIMIT", "VFS overlay count limit exceeded");
        const auto revision = publish_revision(vfs->data->state);
        vfs->data->overlays[path] = source->data;
        vfs->data->revision = revision;
    });
}

On1xSDK_Status on1x_sdk_vfs_remove(
    On1xSDK_VFS *vfs,
    const char *path,
    size_t path_size) {
    if (!vfs) return ON1X_SDK_INVALID_ARGUMENT;
    return api(vfs->data->state, [&] {
        auto normalized = normalize_path(vfs->data->state, path, path_size);
        auto found = vfs->data->overlays.find(normalized);
        if (found == vfs->data->overlays.end())
            not_found("VFS path has no overlay");
        const auto revision = publish_revision(vfs->data->state);
        vfs->data->overlays.erase(found);
        vfs->data->revision = revision;
    });
}

On1xSDK_Status on1x_sdk_vfs_lookup(
    const On1xSDK_VFS *vfs,
    const char *path,
    size_t path_size,
    On1xSDK_Source **out_source) {
    if (!out_source) return ON1X_SDK_INVALID_ARGUMENT;
    *out_source = nullptr;
    if (!vfs) return ON1X_SDK_INVALID_ARGUMENT;
    return api(vfs->data->state, [&] {
        auto normalized = normalize_path(vfs->data->state, path, path_size);
        auto found = vfs->data->overlays.find(normalized);
        if (found == vfs->data->overlays.end())
            not_found("VFS path has no overlay");
        auto source = std::make_unique<On1xSDK_Source>();
        source->data = found->second;
        *out_source = source.release();
    });
}

On1xSDK_Status on1x_sdk_vfs_count(
    const On1xSDK_VFS *vfs,
    size_t *out_count) {
    if (!vfs || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(vfs->data->state, [&] {
        *out_count = vfs->data->overlays.size();
    });
}

uint64_t on1x_sdk_vfs_revision(const On1xSDK_VFS *vfs) {
    return vfs ? vfs->data->revision : 0;
}

} // extern "C"

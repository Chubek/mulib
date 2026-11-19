#include "CoreInternal.hpp"

#include "On1xSDK-Configuration.h"
#include "On1xSDK-Event.h"
#include "On1xSDK-Lifecycle.h"
#include "On1xSDK-Metadata.h"
#include "On1xSDK-Provenance.h"
#include "On1xSDK-Status.h"
#include "On1xSDK-Version.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string_view>

namespace {

constexpr std::uint32_t default_max_handles = 4096;
constexpr std::uint32_t absolute_max_handles = 1u << 20;
std::atomic<std::uint64_t> next_owner{1};

std::uint64_t acquire_owner() noexcept {
    auto candidate = next_owner.load(std::memory_order_relaxed);
    while (candidate) {
        const auto following = candidate == UINT64_MAX
            ? UINT64_C(0) : candidate + 1;
        if (next_owner.compare_exchange_weak(
                candidate, following,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return candidate;
        }
    }
    return 0;
}

bool power_of_two(std::size_t value) noexcept {
    return value && !(value & (value - 1));
}

bool allocation_shape(std::size_t size, std::size_t alignment) noexcept {
    return size <= ON1X_SDK_MAX_ALLOCATION_BYTES &&
           power_of_two(alignment) && alignment <= ON1X_SDK_MAX_ALIGNMENT &&
           alignment != 0;
}

template<class Structure, class Field>
bool field_covered(
    std::uint32_t structure_size, std::size_t offset
) noexcept {
    return structure_size >= offset + sizeof(Field) &&
        offset + sizeof(Field) <= sizeof(Structure);
}

#define ON1X_SDK_COPY_COVERED(destination_, source_, size_, type_, field_) \
    do { \
        if (field_covered< \
                type_, decltype(((type_ *)nullptr)->field_)>( \
                (size_), offsetof(type_, field_))) { \
            (destination_).field_ = (source_)->field_; \
        } \
    } while (false)

void *default_allocate(void *, std::size_t size, std::size_t alignment) {
    if (!size) {
        return nullptr;
    }
    if (alignment <= alignof(std::max_align_t)) {
        return std::malloc(size);
    }
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    void *memory = nullptr;
    return posix_memalign(&memory, alignment, size) == 0 ? memory : nullptr;
#endif
}

void default_release(void *, void *memory, std::size_t, std::size_t alignment) {
    if (!memory) {
        return;
    }
#if defined(_WIN32)
    if (alignment > alignof(std::max_align_t)) {
        _aligned_free(memory);
        return;
    }
#else
    (void)alignment;
#endif
    std::free(memory);
}

void *default_reallocate(
    void *userdata, void *memory, std::size_t old_size, std::size_t new_size,
    std::size_t alignment
) {
    if (!memory) {
        return default_allocate(userdata, new_size, alignment);
    }
    if (!new_size) {
        default_release(userdata, memory, old_size, alignment);
        return nullptr;
    }
    if (alignment <= alignof(std::max_align_t)) {
        return std::realloc(memory, new_size);
    }
    void *replacement = default_allocate(userdata, new_size, alignment);
    if (!replacement) {
        return nullptr;
    }
    std::memcpy(replacement, memory, std::min(old_size, new_size));
    default_release(userdata, memory, old_size, alignment);
    return replacement;
}

const On1xSDK_Allocator default_allocator{
    ON1X_SDK_ABI_VERSION_V2,
    sizeof(On1xSDK_Allocator),
    nullptr,
    default_allocate,
    default_reallocate,
    default_release
};

bool view_shape(const void *data, std::size_t size) noexcept {
    return data || size == 0;
}

bool ascii_identifier(On1xSDK_StringView text) noexcept {
    if (!text.data || text.size == 0 || text.size > ON1X_SDK_NAME_MAX) {
        return false;
    }
    const auto letter = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };
    const auto first = static_cast<unsigned char>(text.data[0]);
    if (!letter(first) && first != '_') {
        return false;
    }
    for (std::size_t i = 1; i < text.size; ++i) {
        const auto c = static_cast<unsigned char>(text.data[i]);
        if (!letter(c) && !(c >= '0' && c <= '9') &&
            c != '_' && c != '-' && c != '.' && c != ':') {
            return false;
        }
    }
    return true;
}

bool valid_utf8(On1xSDK_StringView text) noexcept {
    std::size_t i = 0;
    while (i < text.size) {
        const auto first = static_cast<unsigned char>(text.data[i++]);
        if (first < 0x80) {
            continue;
        }
        unsigned continuation = 0;
        std::uint32_t codepoint = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation = 1;
            codepoint = first & 0x1f;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation = 2;
            codepoint = first & 0x0f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation = 3;
            codepoint = first & 0x07;
        } else {
            return false;
        }
        if (continuation > text.size - i) {
            return false;
        }
        for (unsigned j = 0; j < continuation; ++j) {
            const auto next = static_cast<unsigned char>(text.data[i++]);
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation == 2 && codepoint < 0x800) ||
            (continuation == 3 && codepoint < 0x10000) ||
            codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

On1xSDK_Status host_emit(
    void *opaque, const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
) {
    const auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    if (!token || !token->context) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return on1x_sdk_diagnostic_emit_v2(
        &token->context->diagnostics, diagnostic, source
    );
}

On1xSDK_Status host_log(
    void *opaque, On1xSDK_LogLevel level,
    On1xSDK_StringView target, On1xSDK_StringView message
) {
    auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    if (!token || !token->context || !token->context->log ||
        level < ON1X_SDK_LOG_DEBUG || level > ON1X_SDK_LOG_ERROR ||
        on1x_sdk_identifier_validate(target) != ON1X_SDK_OK ||
        !message.size ||
        on1x_sdk_string_validate_utf8(
            message, ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES) !=
            ON1X_SDK_OK) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        return on1x::sdk::v2::normalize_status(
            token->context->log(
                token->context->log_userdata, level, target, message)
        );
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

On1xSDK_Status host_poll(
    void *opaque, const On1xSDK_Cancellation *cancellation
) {
    if (!opaque) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return on1x_sdk_cancellation_poll(cancellation);
}

On1xSDK_Status host_consume(
    void *opaque, On1xSDK_Budget *budget,
    On1xSDK_BudgetKind kind, std::uint64_t amount
) {
    if (!opaque) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return on1x_sdk_budget_consume(budget, kind, amount);
}

On1xSDK_Status host_retain(
    void *opaque, const On1xSDK_HandleRef *reference
) {
    auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    return token && token->context
        ? on1x_sdk_handle_retain(token->context, reference)
        : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status host_release(
    void *opaque, const On1xSDK_HandleRef *reference
) {
    auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    return token && token->context
        ? on1x_sdk_handle_release(token->context, reference)
        : ON1X_SDK_INVALID_ARGUMENT;
}

int host_capability(
    void *opaque, On1xSDK_StringView name, std::uint32_t revision
) {
    const auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    return token && token->capabilities
        ? on1x_sdk_capability_set_contains(token->capabilities, name, revision)
        : 0;
}

On1xSDK_Status host_submit(
    void *opaque, On1xSDK_StringView schema, std::uint32_t schema_version,
    On1xSDK_ByteView candidate, std::uint64_t source_revision,
    On1xSDK_Handle *candidate_handle
) {
    auto *token = static_cast<On1xSDK_Context::HostToken *>(opaque);
    if (candidate_handle) {
        *candidate_handle = 0;
    }
    if (!token || !token->context || !candidate_handle ||
        on1x_sdk_identifier_validate(schema) != ON1X_SDK_OK ||
        !schema_version || (!candidate.data && candidate.size) ||
        !source_revision) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (candidate.size > ON1X_SDK_MAX_CANDIDATE_BYTES) {
        return ON1X_SDK_LIMIT;
    }
    auto *context = token->context;
    if (!context->submit_candidate) {
        return ON1X_SDK_UNSUPPORTED;
    }
    try {
        const auto status = on1x::sdk::v2::normalize_status(
            context->submit_candidate(
                context->submit_userdata, schema, schema_version,
                candidate, source_revision, candidate_handle
            )
        );
        if (status != ON1X_SDK_OK) {
            *candidate_handle = 0;
            return status;
        }
        return *candidate_handle ? ON1X_SDK_OK : ON1X_SDK_ERROR;
    } catch (const std::bad_alloc &) {
        *candidate_handle = 0;
        return ON1X_SDK_LIMIT;
    } catch (...) {
        *candidate_handle = 0;
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

bool metadata_key(On1xSDK_StringView key) noexcept {
    return ascii_identifier(key);
}

} // namespace

namespace on1x::sdk::v2 {

On1xSDK_Status normalize_status(On1xSDK_Status status) noexcept {
    return on1x_sdk_status_is_known(status) ? status : ON1X_SDK_ERROR;
}

bool valid_state(std::uint32_t state) noexcept {
    constexpr std::uint32_t all =
        ON1X_SDK_HANDLE_STATE_CREATED | ON1X_SDK_HANDLE_STATE_ACTIVE |
        ON1X_SDK_HANDLE_STATE_FROZEN | ON1X_SDK_HANDLE_STATE_CLOSING |
        ON1X_SDK_HANDLE_STATE_CLOSED | ON1X_SDK_HANDLE_STATE_FAILED;
    return state && !(state & ~all) && !(state & (state - 1));
}

void make_host(
    On1xSDK_Context *context,
    const On1xSDK_CapabilitySet *capabilities,
    On1xSDK_Context::HostToken &token,
    On1xSDK_Host &host
) noexcept {
    token = {context, capabilities};
    host = {};
    host.abi_version = ON1X_SDK_ABI_VERSION_V2;
    host.struct_size = sizeof host;
    host.feature_bits =
        ON1X_SDK_HOST_FEATURE_ALLOCATOR |
        ON1X_SDK_HOST_FEATURE_DIAGNOSTICS |
        ON1X_SDK_HOST_FEATURE_CANCELLATION |
        ON1X_SDK_HOST_FEATURE_BUDGET |
        ON1X_SDK_HOST_FEATURE_HANDLES |
        ON1X_SDK_HOST_FEATURE_CAPABILITIES;
    if (context && context->submit_candidate) {
        host.feature_bits |= ON1X_SDK_HOST_FEATURE_CANDIDATES;
    }
    if (context && context->log) {
        host.feature_bits |= ON1X_SDK_HOST_FEATURE_LOGGING;
    }
    host.owner_context = context ? context->owner : 0;
    host.context_token = &token;
    host.allocator = context ? &context->allocator : &default_allocator;
    host.log = context && context->log ? host_log : nullptr;
    host.emit_diagnostic = host_emit;
    host.poll_cancellation = host_poll;
    host.consume_budget = host_consume;
    host.retain_handle = host_retain;
    host.release_handle = host_release;
    host.capability_granted = host_capability;
    host.submit_candidate = host_submit;
}

} // namespace on1x::sdk::v2

extern "C" On1xSDK_Status on1x_sdk_abi_header_validate(
    const void *descriptor, std::uint32_t required_size,
    std::uint32_t supported_abi, On1xSDK_ABIHeader *copied_header
) {
    if (copied_header) {
        *copied_header = {};
    }
    if (!descriptor || required_size < sizeof(On1xSDK_ABIHeader)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_ABIHeader header{};
    std::memcpy(&header, descriptor, sizeof header);
    if (copied_header) {
        *copied_header = header;
    }
    if (header.abi_version != supported_abi ||
        header.struct_size < required_size) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    return ON1X_SDK_OK;
}

extern "C" int on1x_sdk_status_is_v1(On1xSDK_Status status) {
    return static_cast<std::uint32_t>(status) <=
           static_cast<std::uint32_t>(ON1X_SDK_BUSY);
}

extern "C" int on1x_sdk_status_is_known(On1xSDK_Status status) {
    return static_cast<std::uint32_t>(status) <=
           static_cast<std::uint32_t>(ON1X_SDK_UNSATISFIABLE);
}

extern "C" On1xSDK_Status on1x_sdk_status_from_v1(
    std::uint32_t numeric_status
) {
    return numeric_status <= ON1X_SDK_BUSY
        ? static_cast<On1xSDK_Status>(numeric_status)
        : ON1X_SDK_ERROR;
}

extern "C" const char *on1x_sdk_status_name(On1xSDK_Status status) {
    static constexpr std::array names{
        "OK", "ERROR", "UNSUPPORTED", "LIMIT", "INVALID_ARGUMENT",
        "ABI_MISMATCH", "DUPLICATE", "BUSY", "CANCELLED",
        "DEADLINE_EXCEEDED", "STALE", "CONFLICT", "NOT_FOUND",
        "INVALID_STATE", "VERIFICATION_FAILED", "CAPABILITY_DENIED",
        "IO_ERROR", "PROTOCOL_ERROR", "CALLBACK_FAILED", "UNSATISFIABLE"
    };
    const auto value = static_cast<std::uint32_t>(status);
    return value < names.size() ? names[value] : "UNKNOWN_STATUS";
}

extern "C" On1xSDK_StringView on1x_sdk_string_view(
    const char *data, std::size_t size
) {
    return {data, size};
}

extern "C" On1xSDK_ByteView on1x_sdk_byte_view(
    const void *data, std::size_t size
) {
    return {static_cast<const std::uint8_t *>(data), size};
}

extern "C" On1xSDK_Status on1x_sdk_string_validate(
    On1xSDK_StringView text, std::size_t limit
) {
    return view_shape(text.data, text.size) && text.size <= limit
        ? ON1X_SDK_OK : (text.size > limit ? ON1X_SDK_LIMIT
                                           : ON1X_SDK_INVALID_ARGUMENT);
}

extern "C" On1xSDK_Status on1x_sdk_string_validate_utf8(
    On1xSDK_StringView text, std::size_t limit
) {
    const auto status = on1x_sdk_string_validate(text, limit);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    return valid_utf8(text) ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

extern "C" On1xSDK_Status on1x_sdk_identifier_validate(
    On1xSDK_StringView text
) {
    return ascii_identifier(text) ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

extern "C" int on1x_sdk_string_equal(
    On1xSDK_StringView left, On1xSDK_StringView right
) {
    return left.size == right.size &&
        (left.size == 0 ||
         (left.data && right.data &&
          std::memcmp(left.data, right.data, left.size) == 0));
}

extern "C" void on1x_sdk_owned_bytes_release(On1xSDK_OwnedBytes *bytes) {
    if (!bytes) {
        return;
    }
    auto *const data = bytes->data;
    const auto capacity = bytes->capacity;
    const auto *const allocator = bytes->allocator;
    auto *const retained_context = bytes->retained_context;
    /*
     * Retire the public owner before invoking allocator or context callbacks.
     * A callback that observes this object cannot release the same allocation
     * a second time.
     */
    *bytes = {};
    if (data && allocator) {
        (void)on1x_sdk_release(
            allocator, data, capacity,
            alignof(std::max_align_t)
        );
    }
    if (retained_context) {
        on1x_sdk_context_release(retained_context);
    }
}

extern "C" On1xSDK_SourceSpan on1x_sdk_span_unlocated(void) {
    On1xSDK_SourceSpan span{};
    span.byte_begin = UINT64_MAX;
    span.byte_end = UINT64_MAX;
    span.line_begin = UINT32_MAX;
    span.column_begin = UINT32_MAX;
    span.line_end = UINT32_MAX;
    span.column_end = UINT32_MAX;
    return span;
}

extern "C" On1xSDK_Status on1x_sdk_span_validate(
    const On1xSDK_SourceSpan *span, const On1xSDK_SourceRevision *source,
    int allow_unlocated
) {
    if (!span || (allow_unlocated != 0 && allow_unlocated != 1)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const bool unlocated =
        span->source_id == 0 && span->revision == 0 &&
        span->byte_begin == UINT64_MAX && span->byte_end == UINT64_MAX &&
        span->line_begin == UINT32_MAX &&
        span->column_begin == UINT32_MAX &&
        span->line_end == UINT32_MAX &&
        span->column_end == UINT32_MAX;
    if (unlocated) {
        return allow_unlocated ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!source || !source->source_id || !source->revision ||
        span->source_id != source->source_id ||
        span->revision != source->revision ||
        span->byte_begin > span->byte_end ||
        span->byte_end > source->byte_size) {
        return ON1X_SDK_STALE;
    }
    const bool display_unknown =
        span->line_begin == UINT32_MAX && span->column_begin == UINT32_MAX &&
        span->line_end == UINT32_MAX && span->column_end == UINT32_MAX;
    const bool display_known =
        span->line_begin != UINT32_MAX && span->column_begin != UINT32_MAX &&
        span->line_end != UINT32_MAX && span->column_end != UINT32_MAX &&
        (span->line_begin < span->line_end ||
         (span->line_begin == span->line_end &&
          span->column_begin <= span->column_end));
    return display_unknown || display_known
        ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

extern "C" const On1xSDK_Allocator *on1x_sdk_default_allocator(void) {
    return &default_allocator;
}

extern "C" On1xSDK_Status on1x_sdk_allocator_validate(
    const On1xSDK_Allocator *allocator
) {
    const auto status = on1x_sdk_abi_header_validate(
        allocator, sizeof(On1xSDK_Allocator), ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    return allocator->allocate && allocator->reallocate && allocator->release
        ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

extern "C" On1xSDK_Status on1x_sdk_allocate(
    const On1xSDK_Allocator *allocator, std::size_t size,
    std::size_t alignment, void **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    const auto status = on1x_sdk_allocator_validate(allocator);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!allocation_shape(size, alignment)) {
        return size > ON1X_SDK_MAX_ALLOCATION_BYTES
            ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!size) {
        return ON1X_SDK_OK;
    }
    try {
        *out = allocator->allocate(allocator->userdata, size, alignment);
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
    return *out ? ON1X_SDK_OK : ON1X_SDK_LIMIT;
}

extern "C" On1xSDK_Status on1x_sdk_reallocate(
    const On1xSDK_Allocator *allocator, void *memory, std::size_t old_size,
    std::size_t new_size, std::size_t alignment, void **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = memory;
    const auto status = on1x_sdk_allocator_validate(allocator);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if ((!memory && old_size) || !allocation_shape(old_size, alignment) ||
        !allocation_shape(new_size, alignment)) {
        return old_size > ON1X_SDK_MAX_ALLOCATION_BYTES ||
                       new_size > ON1X_SDK_MAX_ALLOCATION_BYTES
            ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        void *replacement = allocator->reallocate(
            allocator->userdata, memory, old_size, new_size, alignment
        );
        if (!replacement && new_size) {
            return ON1X_SDK_LIMIT;
        }
        *out = replacement;
        return ON1X_SDK_OK;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

extern "C" On1xSDK_Status on1x_sdk_release(
    const On1xSDK_Allocator *allocator, void *memory, std::size_t size,
    std::size_t alignment
) {
    const auto status = on1x_sdk_allocator_validate(allocator);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if ((!memory && size) || !allocation_shape(size, alignment)) {
        return size > ON1X_SDK_MAX_ALLOCATION_BYTES
            ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!memory) {
        return ON1X_SDK_OK;
    }
    try {
        allocator->release(allocator->userdata, memory, size, alignment);
        return ON1X_SDK_OK;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

extern "C" On1xSDK_Status on1x_sdk_version(On1xSDK_Version *out) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto status = on1x_sdk_abi_header_validate(
        out, sizeof(On1xSDK_ABIHeader), ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    const auto caller_size = out->struct_size;
    On1xSDK_Version value{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_Version),
        ON1X_SDK_VERSION_MAJOR, ON1X_SDK_VERSION_MINOR,
        ON1X_SDK_VERSION_PATCH, {nullptr, 0}
    };
    out->abi_version = value.abi_version;
    out->struct_size = value.struct_size;
    if (field_covered<On1xSDK_Version, decltype(value.major)>(
            caller_size, offsetof(On1xSDK_Version, major))) {
        out->major = value.major;
    }
    if (field_covered<On1xSDK_Version, decltype(value.minor)>(
            caller_size, offsetof(On1xSDK_Version, minor))) {
        out->minor = value.minor;
    }
    if (field_covered<On1xSDK_Version, decltype(value.patch)>(
            caller_size, offsetof(On1xSDK_Version, patch))) {
        out->patch = value.patch;
    }
    if (field_covered<On1xSDK_Version, decltype(value.prerelease)>(
            caller_size, offsetof(On1xSDK_Version, prerelease))) {
        out->prerelease = value.prerelease;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_negotiate_abi(
    std::uint32_t minimum_abi, std::uint32_t maximum_abi,
    std::uint32_t *selected_abi
) {
    if (!selected_abi || !minimum_abi || minimum_abi > maximum_abi) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *selected_abi = 0;
    if (minimum_abi <= ON1X_SDK_ABI_VERSION_V2 &&
        maximum_abi >= ON1X_SDK_ABI_VERSION_V2) {
        *selected_abi = ON1X_SDK_ABI_VERSION_V2;
        return ON1X_SDK_OK;
    }
    if (minimum_abi <= ON1X_SDK_ABI_VERSION_V1 &&
        maximum_abi >= ON1X_SDK_ABI_VERSION_V1) {
        *selected_abi = ON1X_SDK_ABI_VERSION_V1;
        return ON1X_SDK_OK;
    }
    return ON1X_SDK_ABI_MISMATCH;
}

struct On1xSDK_Cancellation {
    std::atomic<std::uint32_t> references{1};
    std::atomic<bool> cancelled{false};
    std::uint64_t deadline_ns = 0;
};

extern "C" std::uint64_t on1x_sdk_monotonic_now_ns(void) {
    const auto value = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(value).count();
    return nanoseconds > 0 ? static_cast<std::uint64_t>(nanoseconds) : 0;
}

extern "C" On1xSDK_Status on1x_sdk_cancellation_create(
    const On1xSDK_CancellationConfig *config, On1xSDK_Cancellation **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    std::uint64_t deadline = 0;
    if (config) {
        const auto status = on1x_sdk_abi_header_validate(
            config, sizeof(On1xSDK_CancellationConfig),
            ON1X_SDK_ABI_VERSION_V2, nullptr
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        deadline = config->deadline_ns;
    }
    try {
        auto *value = new On1xSDK_Cancellation;
        value->deadline_ns = deadline;
        *out = value;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_cancellation_retain(
    On1xSDK_Cancellation *cancellation
) {
    if (cancellation) {
        cancellation->references.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void on1x_sdk_cancellation_release(
    On1xSDK_Cancellation *cancellation
) {
    if (cancellation &&
        cancellation->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete cancellation;
    }
}

extern "C" On1xSDK_Status on1x_sdk_cancellation_cancel(
    On1xSDK_Cancellation *cancellation
) {
    if (!cancellation) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    cancellation->cancelled.store(true, std::memory_order_release);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_cancellation_poll(
    const On1xSDK_Cancellation *cancellation
) {
    if (!cancellation) {
        return ON1X_SDK_OK;
    }
    if (cancellation->cancelled.load(std::memory_order_acquire)) {
        return ON1X_SDK_CANCELLED;
    }
    if (cancellation->deadline_ns &&
        on1x_sdk_monotonic_now_ns() >= cancellation->deadline_ns) {
        return ON1X_SDK_DEADLINE_EXCEEDED;
    }
    return ON1X_SDK_OK;
}

struct On1xSDK_Budget {
    std::atomic<std::uint32_t> references{1};
    std::array<std::uint64_t, ON1X_SDK_BUDGET_KIND_COUNT> limits{};
    std::array<std::atomic<std::uint64_t>, ON1X_SDK_BUDGET_KIND_COUNT> used{};
};

extern "C" On1xSDK_Status on1x_sdk_budget_create(
    const On1xSDK_BudgetConfig *config, On1xSDK_Budget **out
) {
    if (!out || !config) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    const auto status = on1x_sdk_abi_header_validate(
        config, sizeof(On1xSDK_BudgetConfig),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    try {
        auto *budget = new On1xSDK_Budget;
        std::copy(std::begin(config->limits), std::end(config->limits),
                  budget->limits.begin());
        for (auto &used : budget->used) {
            used.store(0, std::memory_order_relaxed);
        }
        *out = budget;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_budget_retain(On1xSDK_Budget *budget) {
    if (budget) {
        budget->references.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void on1x_sdk_budget_release(On1xSDK_Budget *budget) {
    if (budget &&
        budget->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete budget;
    }
}

extern "C" On1xSDK_Status on1x_sdk_budget_consume(
    On1xSDK_Budget *budget, On1xSDK_BudgetKind kind, std::uint64_t amount
) {
    if (!budget || kind >= ON1X_SDK_BUDGET_KIND_COUNT) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto &used = budget->used[kind];
    auto current = used.load(std::memory_order_relaxed);
    for (;;) {
        const auto limit = budget->limits[kind];
        if (amount > std::numeric_limits<std::uint64_t>::max() - current ||
            (limit != UINT64_MAX && amount > limit - std::min(current, limit))) {
            return ON1X_SDK_LIMIT;
        }
        if (used.compare_exchange_weak(
                current, current + amount,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return ON1X_SDK_OK;
        }
    }
}

extern "C" On1xSDK_Status on1x_sdk_budget_snapshot(
    const On1xSDK_Budget *budget, On1xSDK_BudgetSnapshot *out
) {
    if (!budget || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    for (std::size_t i = 0; i < ON1X_SDK_BUDGET_KIND_COUNT; ++i) {
        out->limits[i] = budget->limits[i];
        out->used[i] = budget->used[i].load(std::memory_order_acquire);
    }
    return ON1X_SDK_OK;
}

extern "C" int on1x_sdk_lifecycle_transition_allowed(
    On1xSDK_LifecycleState from, On1xSDK_LifecycleState to
) {
    switch (from) {
    case ON1X_SDK_LIFECYCLE_NEW:
        return to == ON1X_SDK_LIFECYCLE_VALIDATED ||
               to == ON1X_SDK_LIFECYCLE_FAILED;
    case ON1X_SDK_LIFECYCLE_VALIDATED:
        return to == ON1X_SDK_LIFECYCLE_INITIALIZING ||
               to == ON1X_SDK_LIFECYCLE_FAILED;
    case ON1X_SDK_LIFECYCLE_INITIALIZING:
        return to == ON1X_SDK_LIFECYCLE_ACTIVE ||
               to == ON1X_SDK_LIFECYCLE_STOPPING ||
               to == ON1X_SDK_LIFECYCLE_FAILED;
    case ON1X_SDK_LIFECYCLE_ACTIVE:
        return to == ON1X_SDK_LIFECYCLE_STOPPING ||
               to == ON1X_SDK_LIFECYCLE_FAILED;
    case ON1X_SDK_LIFECYCLE_STOPPING:
        return to == ON1X_SDK_LIFECYCLE_CLOSED ||
               to == ON1X_SDK_LIFECYCLE_FAILED;
    default:
        return 0;
    }
}

extern "C" const char *on1x_sdk_lifecycle_state_name(
    On1xSDK_LifecycleState state
) {
    static constexpr const char *names[] = {
        "NEW", "VALIDATED", "INITIALIZING", "ACTIVE", "STOPPING", "CLOSED",
        "FAILED"
    };
    return state <= ON1X_SDK_LIFECYCLE_FAILED ? names[state] : "UNKNOWN";
}

extern "C" On1xSDK_Status on1x_sdk_event_emit(
    const On1xSDK_EventSink *sink, const On1xSDK_Event *event
) {
    if (!event) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto status = on1x_sdk_abi_header_validate(
        event, sizeof(On1xSDK_Event), ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK || event->reserved ||
        !on1x_sdk_status_is_known(event->status) ||
        event->kind < ON1X_SDK_EVENT_VALIDATED ||
        event->kind > ON1X_SDK_EVENT_FAILED ||
        on1x_sdk_identifier_validate(event->provider) != ON1X_SDK_OK) {
        return status != ON1X_SDK_OK ? status : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!sink) {
        return ON1X_SDK_OK;
    }
    if (on1x_sdk_abi_header_validate(
            sink, sizeof(On1xSDK_EventSink), ON1X_SDK_ABI_VERSION_V2, nullptr
        ) != ON1X_SDK_OK) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    if (!sink->emit) {
        return ON1X_SDK_OK;
    }
    try {
        return on1x::sdk::v2::normalize_status(sink->emit(sink->userdata, event));
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

extern "C" On1xSDK_Status on1x_sdk_metadata_validate(
    const On1xSDK_MetadataItem *items, std::size_t count
) {
    if (count > ON1X_SDK_MAX_METADATA_ITEMS) {
        return ON1X_SDK_LIMIT;
    }
    if (!items && count) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto &item = items[i];
        if (!metadata_key(item.key) || item.value.reserved) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(item.key, items[j].key)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        switch (item.value.kind) {
        case ON1X_SDK_METADATA_INTEGER:
            break;
        case ON1X_SDK_METADATA_BOOLEAN:
            if (item.value.integer != 0 && item.value.integer != 1) {
                return ON1X_SDK_INVALID_ARGUMENT;
            }
            break;
        case ON1X_SDK_METADATA_STRING: {
            const auto status = on1x_sdk_string_validate_utf8(
                item.value.string, ON1X_SDK_MAX_TEXT_BYTES
            );
            if (status != ON1X_SDK_OK) {
                return status;
            }
            break;
        }
        case ON1X_SDK_METADATA_BYTES:
            if (!view_shape(item.value.bytes.data, item.value.bytes.size)) {
                return ON1X_SDK_INVALID_ARGUMENT;
            }
            if (item.value.bytes.size > ON1X_SDK_MAX_TEXT_BYTES) {
                return ON1X_SDK_LIMIT;
            }
            break;
        default:
            return ON1X_SDK_INVALID_ARGUMENT;
        }
    }
    return ON1X_SDK_OK;
}

extern "C" const On1xSDK_MetadataItem *on1x_sdk_metadata_find(
    const On1xSDK_MetadataItem *items, std::size_t count,
    On1xSDK_StringView key
) {
    if (on1x_sdk_metadata_validate(items, count) != ON1X_SDK_OK ||
        !metadata_key(key)) {
        return nullptr;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (on1x_sdk_string_equal(items[i].key, key)) {
            return items + i;
        }
    }
    return nullptr;
}

extern "C" On1xSDK_Status on1x_sdk_configuration_validate(
    const On1xSDK_Configuration *configuration
) {
    const auto status = on1x_sdk_abi_header_validate(
        configuration, sizeof(On1xSDK_Configuration),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    return status == ON1X_SDK_OK
        ? on1x_sdk_metadata_validate(
              configuration->items, configuration->item_count)
        : status;
}

extern "C" On1xSDK_Status on1x_sdk_provenance_validate(
    const On1xSDK_Provenance *provenance
) {
    const auto status = on1x_sdk_abi_header_validate(
        provenance, sizeof(On1xSDK_Provenance),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (on1x_sdk_identifier_validate(provenance->provider) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(provenance->artifact_kind) != ON1X_SDK_OK ||
        !provenance->source_id || !provenance->source_revision ||
        (provenance->has_digest != 0 && provenance->has_digest != 1) ||
        provenance->reserved) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!provenance->has_digest) {
        std::uint8_t digest_bits = 0;
        for (const auto byte : provenance->digest_sha256) {
            digest_bits |= byte;
        }
        if (digest_bits) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_capability_request_validate(
    const On1xSDK_CapabilityRequest *request
) {
    if (!request ||
        on1x_sdk_identifier_validate(request->name) != ON1X_SDK_OK ||
        !request->min_revision ||
        (request->max_revision &&
         request->max_revision < request->min_revision) ||
        (request->flags & ~ON1X_SDK_CAPABILITY_REQUIRED) ||
        request->reserved) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_capability_grant_validate(
    const On1xSDK_CapabilityGrant *grant
) {
    if (!grant ||
        on1x_sdk_identifier_validate(grant->name) != ON1X_SDK_OK ||
        !grant->revision || grant->flags) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_capabilities_negotiate(
    const On1xSDK_CapabilityRequest *requests, std::size_t request_count,
    const On1xSDK_CapabilityGrant *host_grants, std::size_t host_grant_count,
    On1xSDK_CapabilitySet **out
) {
    if (!out || (!requests && request_count) ||
        (!host_grants && host_grant_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    if (request_count > ON1X_SDK_MAX_CAPABILITIES ||
        host_grant_count > ON1X_SDK_MAX_CAPABILITIES) {
        return ON1X_SDK_LIMIT;
    }
    for (std::size_t i = 0; i < host_grant_count; ++i) {
        if (on1x_sdk_capability_grant_validate(host_grants + i) !=
            ON1X_SDK_OK) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    host_grants[i].name, host_grants[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
    }
    try {
        auto value = std::make_unique<On1xSDK_CapabilitySet>();
        for (std::size_t i = 0; i < request_count; ++i) {
            const auto &request = requests[i];
            if (on1x_sdk_capability_request_validate(&request) != ON1X_SDK_OK) {
                return ON1X_SDK_INVALID_ARGUMENT;
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (on1x_sdk_string_equal(request.name, requests[j].name)) {
                    return ON1X_SDK_DUPLICATE;
                }
            }
            const On1xSDK_CapabilityGrant *match = nullptr;
            for (std::size_t j = 0; j < host_grant_count; ++j) {
                const auto &grant = host_grants[j];
                if (on1x_sdk_string_equal(request.name, grant.name) &&
                    grant.revision >= request.min_revision &&
                    (!request.max_revision ||
                     grant.revision <= request.max_revision)) {
                    match = &grant;
                    break;
                }
            }
            if (!match) {
                if (request.flags & ON1X_SDK_CAPABILITY_REQUIRED) {
                    return ON1X_SDK_CAPABILITY_DENIED;
                }
                continue;
            }
            value->grants.push_back({
                std::string(request.name.data, request.name.size),
                match->revision, 0
            });
        }
        std::sort(value->grants.begin(), value->grants.end(),
                  [](const auto &left, const auto &right) {
                      return left.name < right.name;
                  });
        *out = value.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_capability_set_destroy(
    On1xSDK_CapabilitySet *set
) {
    delete set;
}

extern "C" std::size_t on1x_sdk_capability_set_count(
    const On1xSDK_CapabilitySet *set
) {
    return set ? set->grants.size() : 0;
}

extern "C" On1xSDK_Status on1x_sdk_capability_set_at(
    const On1xSDK_CapabilitySet *set, std::size_t index,
    On1xSDK_CapabilityGrant *out
) {
    if (!set || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = {};
    if (index >= set->grants.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    const auto &grant = set->grants[index];
    *out = {{grant.name.data(), grant.name.size()}, grant.revision, grant.flags};
    return ON1X_SDK_OK;
}

extern "C" int on1x_sdk_capability_set_contains(
    const On1xSDK_CapabilitySet *set, On1xSDK_StringView name,
    std::uint32_t minimum_revision
) {
    if (!set || !minimum_revision ||
        on1x_sdk_identifier_validate(name) != ON1X_SDK_OK) {
        return 0;
    }
    return std::any_of(set->grants.begin(), set->grants.end(),
        [&](const auto &grant) {
            return grant.revision >= minimum_revision &&
                   grant.name.size() == name.size &&
                   std::memcmp(grant.name.data(), name.data, name.size) == 0;
        });
}

extern "C" On1xSDK_Status on1x_sdk_context_create(
    const On1xSDK_ContextConfig *config, On1xSDK_Context **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    On1xSDK_ContextConfig value{};
    if (config) {
        On1xSDK_ABIHeader header{};
        const auto status = on1x_sdk_abi_header_validate(
            config, sizeof(On1xSDK_ABIHeader),
            ON1X_SDK_ABI_VERSION_V2, &header
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        value.abi_version = header.abi_version;
        value.struct_size = header.struct_size;
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, allocator
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, diagnostics
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, log
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, log_userdata
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, submit_candidate
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, submit_userdata
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, max_handles
        );
        ON1X_SDK_COPY_COVERED(
            value, config, header.struct_size,
            On1xSDK_ContextConfig, reserved
        );
        if (value.reserved) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
    }
    const auto *allocator = value.allocator
        ? value.allocator : on1x_sdk_default_allocator();
    const auto allocator_status = on1x_sdk_allocator_validate(allocator);
    if (allocator_status != ON1X_SDK_OK) {
        return allocator_status;
    }
    const auto max_handles = value.max_handles
        ? value.max_handles : default_max_handles;
    if (max_handles > absolute_max_handles) {
        return ON1X_SDK_LIMIT;
    }
    if (value.diagnostics.emit) {
        const auto status = on1x_sdk_abi_header_validate(
            &value.diagnostics, sizeof(On1xSDK_DiagnosticSinkV2),
            ON1X_SDK_ABI_VERSION_V2, nullptr
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
    } else {
        value.diagnostics = {
            ON1X_SDK_ABI_VERSION_V2,
            sizeof(On1xSDK_DiagnosticSinkV2),
            nullptr,
            nullptr
        };
    }
    try {
        auto context = std::make_unique<On1xSDK_Context>();
        context->owner = acquire_owner();
        if (!context->owner) {
            return ON1X_SDK_LIMIT;
        }
        context->allocator = *allocator;
        context->allocator.struct_size = sizeof(On1xSDK_Allocator);
        context->diagnostics = value.diagnostics;
        context->log = value.log;
        context->log_userdata = value.log_userdata;
        context->submit_candidate = value.submit_candidate;
        context->submit_userdata = value.submit_userdata;
        context->max_handles = max_handles;
        context->handles.reserve(std::min<std::uint32_t>(max_handles, 64));
        on1x::sdk::v2::make_host(
            context.get(), nullptr, context->base_host_token, context->host
        );
        *out = context.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_context_retain(On1xSDK_Context *context) {
    if (context) {
        context->references.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void on1x_sdk_context_release(On1xSDK_Context *context) {
    if (!context ||
        context->references.fetch_sub(1, std::memory_order_acq_rel) != 1) {
        return;
    }
    /* Final ownership excludes concurrent use. Retire every slot before any
     * callback so a destructor cannot resolve another object by re-entering. */
    for (auto &slot : context->handles) {
        slot.references = 0;
        slot.state = ON1X_SDK_HANDLE_STATE_CLOSED;
        slot.retired = true;
    }
    for (auto &slot : context->handles) {
        const auto callback = slot.drop;
        void *const userdata = slot.drop_userdata;
        void *const object = slot.object;
        slot.object = nullptr;
        slot.drop = nullptr;
        slot.drop_userdata = nullptr;
        if (callback && object) {
            try {
                callback(userdata, object);
            } catch (...) {
                /* C callbacks may not throw through the boundary. */
            }
        }
    }
    delete context;
}

extern "C" std::uint64_t on1x_sdk_context_owner(
    const On1xSDK_Context *context
) {
    return context ? context->owner : 0;
}

extern "C" const On1xSDK_Allocator *on1x_sdk_context_allocator(
    const On1xSDK_Context *context
) {
    return context ? &context->allocator : nullptr;
}

extern "C" const On1xSDK_Host *on1x_sdk_context_host(
    const On1xSDK_Context *context
) {
    return context ? &context->host : nullptr;
}

extern "C" On1xSDK_Status on1x_sdk_host_negotiate(
    const On1xSDK_Host *offered, std::uint32_t requested_abi,
    std::uint32_t required_size, const On1xSDK_Host **selected
) {
    if (!selected) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *selected = nullptr;
    if (!offered || requested_abi != ON1X_SDK_ABI_VERSION_V2 ||
        required_size < sizeof(On1xSDK_ABIHeader)) {
        return requested_abi == ON1X_SDK_ABI_VERSION_V2
            ? ON1X_SDK_INVALID_ARGUMENT : ON1X_SDK_ABI_MISMATCH;
    }
    if (offered->abi_version != ON1X_SDK_ABI_VERSION_V2 ||
        offered->struct_size < required_size) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    *selected = offered;
    return ON1X_SDK_OK;
}

#undef ON1X_SDK_COPY_COVERED

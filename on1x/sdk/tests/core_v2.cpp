#include "On1xSDK-Allocator.h"
#include "On1xSDK-Budget.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-Host.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-Serialization.h"
#include "On1xSDK-SharedMemory.h"
#include "On1xSDK-Status.h"
#include "On1xSDK-Transport.h"
#include "On1xSDK-Version.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

struct Counts {
    unsigned allocations = 0;
    unsigned releases = 0;
    unsigned logs = 0;
    unsigned diagnostics = 0;
    unsigned drops = 0;
};

struct ReentrantRelease {
    On1xSDK_OwnedBytes *bytes = nullptr;
    unsigned releases = 0;
};

void *allocate(void *opaque, std::size_t size, std::size_t) {
    auto &counts = *static_cast<Counts *>(opaque);
    ++counts.allocations;
    return std::malloc(size);
}

void *reallocate(
    void *opaque, void *memory, std::size_t, std::size_t size, std::size_t
) {
    auto &counts = *static_cast<Counts *>(opaque);
    if (!memory) ++counts.allocations;
    if (!size && memory) ++counts.releases;
    return std::realloc(memory, size);
}

void release(
    void *opaque, void *memory, std::size_t, std::size_t
) {
    auto &counts = *static_cast<Counts *>(opaque);
    ++counts.releases;
    std::free(memory);
}

void *reentrant_allocate(void *, std::size_t size, std::size_t) {
    return std::malloc(size);
}

void *reentrant_reallocate(
    void *, void *memory, std::size_t, std::size_t size, std::size_t
) {
    return std::realloc(memory, size);
}

void reentrant_release(
    void *opaque, void *memory, std::size_t, std::size_t
) {
    auto &state = *static_cast<ReentrantRelease *>(opaque);
    ++state.releases;
    if (state.releases == 1) {
        on1x_sdk_owned_bytes_release(state.bytes);
    }
    std::free(memory);
}

On1xSDK_Status log_message(
    void *opaque, On1xSDK_LogLevel,
    On1xSDK_StringView, On1xSDK_StringView
) {
    ++static_cast<Counts *>(opaque)->logs;
    return ON1X_SDK_OK;
}

On1xSDK_Status diagnostic(
    void *opaque, const On1xSDK_DiagnosticV2 *
) {
    ++static_cast<Counts *>(opaque)->diagnostics;
    return ON1X_SDK_OK;
}

void drop(void *opaque, void *object) {
    CHECK(object != nullptr);
    ++static_cast<Counts *>(opaque)->drops;
}

On1xSDK_ProtocolMessage message(
    std::uint16_t kind, std::uint64_t id = 0,
    std::uint16_t flags = 0
) {
    return {
        ON1X_SDK_PLUGIN_PROTOCOL_MAJOR,
        ON1X_SDK_PLUGIN_PROTOCOL_MINOR,
        kind, flags, ON1X_SDK_OK, id, 0, 0, 0, {nullptr, 0}
    };
}

} // namespace

int main() {
    static_assert(ON1X_SDK_ABI_VERSION == 1);
    static_assert(ON1X_SDK_ABI_VERSION_V2 == 2);
    static_assert(ON1X_SDK_OK == 0 && ON1X_SDK_BUSY == 7);
    static_assert(ON1X_SDK_CANCELLED == 8);

    std::uint32_t selected = 0;
    CHECK(on1x_sdk_negotiate_abi(1, 2, &selected) == ON1X_SDK_OK);
    CHECK(selected == ON1X_SDK_ABI_VERSION_V2);
    CHECK(on1x_sdk_negotiate_abi(3, 4, &selected) ==
          ON1X_SDK_ABI_MISMATCH);
    CHECK(std::strcmp(
        on1x_sdk_status_name(ON1X_SDK_VERIFICATION_FAILED),
        "VERIFICATION_FAILED") == 0);

    On1xSDK_Version partial_version;
    std::memset(&partial_version, 0xa5, sizeof partial_version);
    partial_version.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_version.struct_size =
        offsetof(On1xSDK_Version, major) + 1;
    CHECK(on1x_sdk_version(&partial_version) == ON1X_SDK_OK);
    CHECK(partial_version.struct_size == sizeof(On1xSDK_Version));
    CHECK(reinterpret_cast<const unsigned char *>(&partial_version)
              [offsetof(On1xSDK_Version, major)] == 0xa5);

    On1xSDK_Version sdk_version{};
    sdk_version.abi_version = ON1X_SDK_ABI_VERSION_V2;
    sdk_version.struct_size = sizeof(On1xSDK_Version);
    CHECK(on1x_sdk_version(&sdk_version) == ON1X_SDK_OK);
    CHECK(sdk_version.major == 2);

    Counts counts;
    const On1xSDK_Allocator allocator{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_Allocator),
        &counts, allocate, reallocate, release
    };
    void *small = nullptr;
    CHECK(on1x_sdk_allocate(&allocator, 8, 1, &small) == ON1X_SDK_OK);
    CHECK(small);
    CHECK(on1x_sdk_release(&allocator, small, 8, 1) == ON1X_SDK_OK);

    On1xSDK_ContextConfig partial_context_config{};
    partial_context_config.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_context_config.struct_size =
        offsetof(On1xSDK_ContextConfig, allocator) + 1;
    reinterpret_cast<unsigned char *>(&partial_context_config)
        [offsetof(On1xSDK_ContextConfig, allocator)] = 1;
    On1xSDK_Context *partial_context = nullptr;
    CHECK(on1x_sdk_context_create(
        &partial_context_config, &partial_context) == ON1X_SDK_OK);
    on1x_sdk_context_release(partial_context);

    On1xSDK_ContextConfig context_config{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_ContextConfig),
        &allocator,
        {ON1X_SDK_ABI_VERSION_V2,
         sizeof(On1xSDK_DiagnosticSinkV2), diagnostic, &counts},
        log_message, &counts,
        nullptr, nullptr,
        32, 0
    };
    On1xSDK_Context *context = nullptr;
    CHECK(on1x_sdk_context_create(&context_config, &context) ==
          ON1X_SDK_OK);
    CHECK(context && on1x_sdk_context_owner(context));
    const auto *host = on1x_sdk_context_host(context);
    CHECK(host && host->abi_version == ON1X_SDK_ABI_VERSION_V2);
    CHECK((host->feature_bits & ON1X_SDK_HOST_FEATURE_LOGGING) != 0);
    CHECK(host->log(
        host->context_token, ON1X_SDK_LOG_INFO,
        {"core", 4}, {"ready", 5}) == ON1X_SDK_OK);
    CHECK(counts.logs == 1);

    const auto unlocated = on1x_sdk_span_unlocated();
    On1xSDK_DiagnosticV2 record{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_DiagnosticV2),
        ON1X_SDK_NOTE, 0, {"SDK0001", 7}, {"message", 7},
        unlocated, nullptr, 0, nullptr, 0,
        {"core", 4}, {"test", 4}, {"fixture", 7}
    };
    CHECK(on1x_sdk_diagnostic_emit_v2(
        &context_config.diagnostics, &record, nullptr) == ON1X_SDK_OK);
    CHECK(counts.diagnostics == 1);

    On1xSDK_BudgetConfig budget_config{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_BudgetConfig),
        {3, UINT64_MAX, UINT64_MAX, UINT64_MAX,
         UINT64_MAX, UINT64_MAX, UINT64_MAX}
    };
    On1xSDK_Budget *budget = nullptr;
    CHECK(on1x_sdk_budget_create(&budget_config, &budget) == ON1X_SDK_OK);
    CHECK(on1x_sdk_budget_consume(
        budget, ON1X_SDK_BUDGET_WORK, 2) == ON1X_SDK_OK);
    CHECK(on1x_sdk_budget_consume(
        budget, ON1X_SDK_BUDGET_WORK, 2) == ON1X_SDK_LIMIT);
    On1xSDK_BudgetSnapshot budget_snapshot{};
    CHECK(on1x_sdk_budget_snapshot(budget, &budget_snapshot) ==
          ON1X_SDK_OK);
    CHECK(budget_snapshot.used[ON1X_SDK_BUDGET_WORK] == 2);
    on1x_sdk_budget_release(budget);

    On1xSDK_Cancellation *cancellation = nullptr;
    CHECK(on1x_sdk_cancellation_create(nullptr, &cancellation) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_cancellation_poll(cancellation) == ON1X_SDK_OK);
    CHECK(on1x_sdk_cancellation_cancel(cancellation) == ON1X_SDK_OK);
    CHECK(on1x_sdk_cancellation_poll(cancellation) ==
          ON1X_SDK_CANCELLED);

    int object = 7;
    On1xSDK_HandleDescriptor handle_descriptor{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_HandleDescriptor),
        ON1X_SDK_HANDLE_KIND_REQUEST, ON1X_SDK_HANDLE_STATE_CREATED,
        &object, drop, &counts
    };
    On1xSDK_HandleRef reference{};
    CHECK(on1x_sdk_handle_insert(
        context, &handle_descriptor, &reference) == ON1X_SDK_OK);
    auto alias = reference;
    CHECK(on1x_sdk_handle_retain(context, &alias) == ON1X_SDK_OK);
    CHECK(on1x_sdk_handle_transition(
        context, &reference, ON1X_SDK_HANDLE_STATE_CREATED,
        ON1X_SDK_HANDLE_STATE_ACTIVE) == ON1X_SDK_OK);
    void *resolved = nullptr;
    CHECK(on1x_sdk_handle_resolve(
        context, &alias, ON1X_SDK_HANDLE_KIND_REQUEST,
        ON1X_SDK_HANDLE_STATE_ACTIVE, &resolved) == ON1X_SDK_OK);
    CHECK(resolved == &object);
    CHECK(on1x_sdk_handle_release(context, &alias) == ON1X_SDK_OK);
    CHECK(on1x_sdk_handle_release(context, &reference) == ON1X_SDK_OK);
    CHECK(counts.drops == 1);
    CHECK(on1x_sdk_handle_resolve(
        context, &reference, ON1X_SDK_HANDLE_KIND_REQUEST,
        ON1X_SDK_HANDLE_STATE_ACTIVE, &resolved) == ON1X_SDK_STALE);

    const char payload[] = "control";
    auto request = message(ON1X_SDK_PROTOCOL_REQUEST, 42);
    request.payload = {
        reinterpret_cast<const std::uint8_t *>(payload),
        sizeof(payload) - 1
    };
    On1xSDK_OwnedBytes frame{};
    CHECK(on1x_sdk_protocol_encode(
        &request, &allocator, &frame) == ON1X_SDK_OK);
    On1xSDK_ProtocolMessage decoded{};
    CHECK(on1x_sdk_protocol_decode(
        {frame.data, frame.size}, 128, &decoded) == ON1X_SDK_OK);
    CHECK(decoded.kind == ON1X_SDK_PROTOCOL_REQUEST &&
          decoded.request_id == 42 && decoded.payload.size == 7);
    frame.data[0] ^= 1;
    CHECK(on1x_sdk_protocol_decode(
        {frame.data, frame.size}, 128, &decoded) ==
           ON1X_SDK_PROTOCOL_ERROR);
    frame.data[0] ^= 1;
    CHECK(on1x_sdk_protocol_decode(
        {frame.data, 4}, 128, &decoded) == ON1X_SDK_PROTOCOL_ERROR);
    frame.data[8] = 0;
    frame.data[9] = ON1X_SDK_PROTOCOL_HELLO;
    CHECK(on1x_sdk_protocol_decode(
        {frame.data, frame.size}, 128, &decoded) ==
          ON1X_SDK_PROTOCOL_ERROR);
    frame.data[9] = ON1X_SDK_PROTOCOL_REQUEST;
    frame.data[11] = ON1X_SDK_PROTOCOL_FLAG_FINAL;
    CHECK(on1x_sdk_protocol_decode(
        {frame.data, frame.size}, 128, &decoded) ==
          ON1X_SDK_PROTOCOL_ERROR);
    frame.data[11] = 0;
    on1x_sdk_owned_bytes_release(&frame);

    auto invalid_request = message(
        ON1X_SDK_PROTOCOL_REQUEST, 1,
        ON1X_SDK_PROTOCOL_FLAG_FINAL
    );
    CHECK(on1x_sdk_protocol_encode(
        &invalid_request, &allocator, &frame) ==
          ON1X_SDK_INVALID_ARGUMENT);
    auto invalid_shared_response = message(
        ON1X_SDK_PROTOCOL_RESPONSE, 1,
        ON1X_SDK_PROTOCOL_FLAG_FINAL |
            ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD
    );
    CHECK(on1x_sdk_protocol_encode(
        &invalid_shared_response, &allocator, &frame) ==
          ON1X_SDK_INVALID_ARGUMENT);

    ReentrantRelease reentrant{&frame};
    const On1xSDK_Allocator reentrant_allocator{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_Allocator),
        &reentrant, reentrant_allocate, reentrant_reallocate,
        reentrant_release
    };
    request = message(ON1X_SDK_PROTOCOL_REQUEST, 77);
    CHECK(on1x_sdk_protocol_encode(
        &request, &reentrant_allocator, &frame) == ON1X_SDK_OK);
    on1x_sdk_owned_bytes_release(&frame);
    CHECK(reentrant.releases == 1 && frame.data == nullptr);

    On1xSDK_ProtocolSessionConfig session_config{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_ProtocolSessionConfig),
        ON1X_SDK_PROTOCOL_ROLE_HOST, 4, 4096, 0
    };
    On1xSDK_ProtocolSession *session = nullptr;
    CHECK(on1x_sdk_protocol_session_create(
        &session_config, &session) == ON1X_SDK_OK);
    auto hello = message(ON1X_SDK_PROTOCOL_HELLO);
    auto hello_ack = message(ON1X_SDK_PROTOCOL_HELLO_ACK);
    auto activate = message(ON1X_SDK_PROTOCOL_ACTIVATE);
    auto active = message(ON1X_SDK_PROTOCOL_ACTIVE);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_OUTGOING, &hello) == ON1X_SDK_OK);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_INCOMING, &hello_ack) == ON1X_SDK_OK);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_INCOMING, &active) ==
          ON1X_SDK_INVALID_STATE);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_OUTGOING, &activate) == ON1X_SDK_OK);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_INCOMING, &active) == ON1X_SDK_OK);
    request = message(ON1X_SDK_PROTOCOL_REQUEST, 9);
    auto response = message(
        ON1X_SDK_PROTOCOL_RESPONSE, 9,
        ON1X_SDK_PROTOCOL_FLAG_FINAL
    );
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_OUTGOING, &request) == ON1X_SDK_OK);
    auto cancel = message(ON1X_SDK_PROTOCOL_CANCEL, 9);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_INCOMING, &cancel) ==
          ON1X_SDK_NOT_FOUND);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_OUTGOING, &cancel) == ON1X_SDK_OK);
    CHECK(on1x_sdk_protocol_session_accept(
        session, ON1X_SDK_PROTOCOL_INCOMING, &response) == ON1X_SDK_OK);
    on1x_sdk_protocol_session_destroy(session);

    On1xSDK_SerializationEnvelope envelope{
        ON1X_SDK_SERIAL_ENVELOPE_MAGIC, 3, 1, 0, 8, 10, {}
    };
    unsigned char envelope_wire[
        ON1X_SDK_SERIALIZATION_ENVELOPE_WIRE_SIZE]{};
    CHECK(on1x_sdk_serialization_envelope_encode(
        &envelope, envelope_wire, sizeof envelope_wire) == ON1X_SDK_OK);
    On1xSDK_SerializationEnvelope decoded_envelope{};
    CHECK(on1x_sdk_serialization_envelope_decode(
        envelope_wire, sizeof envelope_wire, 3, 10,
        &decoded_envelope) == ON1X_SDK_OK);
    CHECK(decoded_envelope.revision == 8);

    On1xSDK_SharedMemory *thread_staging = nullptr;
    CHECK(on1x_sdk_shared_memory_create_staging(
        context, 16, &thread_staging) == ON1X_SDK_OK);
    On1xSDK_Status cross_thread_freeze = ON1X_SDK_OK;
    std::thread wrong_thread([&] {
        cross_thread_freeze =
            on1x_sdk_shared_memory_freeze(thread_staging, 0);
    });
    wrong_thread.join();
    CHECK(cross_thread_freeze == ON1X_SDK_BUSY);
    std::uint8_t *staging_bytes = nullptr;
    std::size_t staging_capacity = 0;
    CHECK(on1x_sdk_shared_memory_mutable_bytes(
        thread_staging, &staging_bytes, &staging_capacity) ==
          ON1X_SDK_OK);
    CHECK(staging_bytes && staging_capacity == 16);
    on1x_sdk_shared_memory_release(thread_staging);

    On1xSDK_SharedMemory *frozen = nullptr;
    CHECK(on1x_sdk_shared_memory_create_frozen(
        context,
        {reinterpret_cast<const std::uint8_t *>("abc"), 3},
        &frozen) == ON1X_SDK_OK);
    On1xSDK_SharedMemoryDescriptor frozen_descriptor{};
    CHECK(on1x_sdk_shared_memory_export(
        frozen, &frozen_descriptor) == ON1X_SDK_OK);
    CHECK(on1x_sdk_shared_memory_validate_local(
        context, &frozen_descriptor) == ON1X_SDK_OK);
    auto altered_local_descriptor = frozen_descriptor;
    ++altered_local_descriptor.capacity;
    CHECK(on1x_sdk_shared_memory_validate_local(
        context, &altered_local_descriptor) ==
          ON1X_SDK_VERIFICATION_FAILED);
    unsigned char descriptor_wire[
        ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE]{};
    CHECK(on1x_sdk_shared_memory_descriptor_encode(
        &frozen_descriptor, descriptor_wire, sizeof descriptor_wire) ==
          ON1X_SDK_OK);
    On1xSDK_SharedMemoryDescriptor decoded_descriptor{};
    CHECK(on1x_sdk_shared_memory_descriptor_decode(
        descriptor_wire, sizeof descriptor_wire,
        &decoded_descriptor) == ON1X_SDK_OK);
    On1xSDK_SharedMemory *imported = nullptr;
    CHECK(on1x_sdk_shared_memory_import_authorized(
        &decoded_descriptor, ON1X_SDK_SHARED_MEMORY_FROZEN,
        &imported) == ON1X_SDK_OK);
    const std::uint8_t *shared_bytes = nullptr;
    std::size_t shared_size = 0;
    CHECK(on1x_sdk_shared_memory_bytes(
        imported, &shared_bytes, &shared_size) == ON1X_SDK_OK);
    CHECK(shared_size == 3 && std::memcmp(shared_bytes, "abc", 3) == 0);
#if defined(__unix__) || defined(__APPLE__)
    const auto child = fork();
    CHECK(child >= 0);
    if (!child) {
        const rlimit no_core{0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        /*
         * Sanitizer runtimes install fatal-signal handlers that translate the
         * expected protection fault into SIGABRT. Restore the default handlers
         * so the parent can verify the operating-system page protection itself.
         */
        (void)signal(SIGSEGV, SIG_DFL);
#ifdef SIGBUS
        (void)signal(SIGBUS, SIG_DFL);
#endif
        const_cast<std::uint8_t *>(shared_bytes)[0] = 'x';
        _exit(0);
    }
    int child_status = 0;
    CHECK(waitpid(child, &child_status, 0) == child);
    CHECK(WIFSIGNALED(child_status));
    CHECK(WTERMSIG(child_status) == SIGSEGV ||
          WTERMSIG(child_status) == SIGBUS);
#endif
    on1x_sdk_shared_memory_release(imported);
    decoded_descriptor.digest_sha256[0] ^= 1;
    CHECK(on1x_sdk_shared_memory_import_authorized(
        &decoded_descriptor, ON1X_SDK_SHARED_MEMORY_FROZEN,
        &imported) == ON1X_SDK_VERIFICATION_FAILED);
    on1x_sdk_shared_memory_release(frozen);

#if defined(__unix__) || defined(__APPLE__)
    int sockets[2]{};
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    On1xSDK_TransportConfig first_config{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_TransportConfig),
        context, sockets[0], 4096
    };
    On1xSDK_TransportConfig second_config{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_TransportConfig),
        context, sockets[1], 4096
    };
    On1xSDK_Transport *first = nullptr, *second = nullptr;
    CHECK(on1x_sdk_transport_create(&first_config, &first) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_transport_create(&second_config, &second) ==
          ON1X_SDK_OK);
    const auto deadline = on1x_sdk_monotonic_now_ns() + UINT64_C(1000000000);
    CHECK(on1x_sdk_transport_send(
        first,
        {reinterpret_cast<const std::uint8_t *>("frame"), 5},
        deadline, nullptr, nullptr) == ON1X_SDK_OK);
    On1xSDK_OwnedBytes received{};
    CHECK(on1x_sdk_transport_receive(
        second, deadline, nullptr, nullptr, &received) == ON1X_SDK_OK);
    CHECK(received.size == 5 &&
          std::memcmp(received.data, "frame", 5) == 0);
    CHECK(on1x_sdk_transport_send(
        first, {nullptr, 0}, deadline, cancellation, nullptr) ==
          ON1X_SDK_CANCELLED);
    on1x_sdk_transport_destroy(first);
    on1x_sdk_transport_destroy(second);
    close(sockets[0]);
    close(sockets[1]);
    on1x_sdk_context_release(context);
    context = nullptr;
    on1x_sdk_owned_bytes_release(&received);
#endif

    on1x_sdk_cancellation_release(cancellation);
    if (context) on1x_sdk_context_release(context);
    CHECK(counts.allocations == counts.releases);
    return 0;
}

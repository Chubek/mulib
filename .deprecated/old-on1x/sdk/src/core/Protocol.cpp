#include "On1xSDK-Lifecycle.h"
#include "On1xSDK-Allocator.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-Serialization.h"
#include "On1xSDK-Status.h"

#include "Wire.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <set>

namespace {

bool protocol_kind(std::uint16_t kind) noexcept {
    return kind >= ON1X_SDK_PROTOCOL_HELLO &&
           kind <= ON1X_SDK_PROTOCOL_EVENT;
}

On1xSDK_Status validate_message(
    const On1xSDK_ProtocolMessage &message, std::size_t max_payload
) noexcept {
    if (message.protocol_major != ON1X_SDK_PLUGIN_PROTOCOL_MAJOR ||
        message.protocol_minor > ON1X_SDK_PLUGIN_PROTOCOL_MINOR) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    if (!protocol_kind(message.kind) ||
        (message.flags & ~(ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD |
                           ON1X_SDK_PROTOCOL_FLAG_FINAL)) ||
        !on1x_sdk_status_is_known(message.status) ||
        (!message.payload.data && message.payload.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (message.payload.size > max_payload ||
        message.payload.size > UINT32_MAX) {
        return ON1X_SDK_LIMIT;
    }
    switch (message.kind) {
    case ON1X_SDK_PROTOCOL_REQUEST:
    case ON1X_SDK_PROTOCOL_RESPONSE:
    case ON1X_SDK_PROTOCOL_CANCEL:
        if (!message.request_id) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
        break;
    default:
        if (message.request_id) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
        break;
    }
    if (message.kind == ON1X_SDK_PROTOCOL_RESPONSE &&
        !(message.flags & ON1X_SDK_PROTOCOL_FLAG_FINAL)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (message.kind != ON1X_SDK_PROTOCOL_RESPONSE &&
        (message.flags & ON1X_SDK_PROTOCOL_FLAG_FINAL)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if ((message.flags & ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD) &&
        (message.kind != ON1X_SDK_PROTOCOL_REQUEST &&
         message.kind != ON1X_SDK_PROTOCOL_RESPONSE)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if ((message.flags & ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD) &&
        !message.payload.size) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (message.kind == ON1X_SDK_PROTOCOL_CANCEL &&
        (message.payload.size || message.flags)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (message.kind != ON1X_SDK_PROTOCOL_RESPONSE &&
        message.kind != ON1X_SDK_PROTOCOL_EVENT &&
        message.status != ON1X_SDK_OK) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_serialization_envelope_encode(
    const On1xSDK_SerializationEnvelope *envelope, void *output,
    std::size_t output_size
) {
    if (!envelope || !output ||
        output_size != ON1X_SDK_SERIALIZATION_ENVELOPE_WIRE_SIZE ||
        envelope->magic != ON1X_SDK_SERIAL_ENVELOPE_MAGIC ||
        !envelope->schema_major) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto *cursor = static_cast<std::uint8_t *>(output);
    on1x::sdk::wire::put32(cursor, envelope->magic);
    on1x::sdk::wire::put16(cursor, envelope->schema_major);
    on1x::sdk::wire::put16(cursor, envelope->schema_minor);
    on1x::sdk::wire::put32(cursor, envelope->flags);
    on1x::sdk::wire::put64(cursor, envelope->revision);
    on1x::sdk::wire::put64(cursor, envelope->payload_size);
    on1x::sdk::wire::put_bytes(cursor, envelope->digest_sha256, 32);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_serialization_envelope_decode(
    const void *input, std::size_t input_size, std::uint16_t supported_major,
    std::uint64_t maximum_payload, On1xSDK_SerializationEnvelope *out
) {
    if (out) {
        *out = {};
    }
    if (!input || !out ||
        input_size != ON1X_SDK_SERIALIZATION_ENVELOPE_WIRE_SIZE ||
        !supported_major) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto *cursor = static_cast<const std::uint8_t *>(input);
    out->magic = on1x::sdk::wire::get32(cursor);
    out->schema_major = on1x::sdk::wire::get16(cursor);
    out->schema_minor = on1x::sdk::wire::get16(cursor);
    out->flags = on1x::sdk::wire::get32(cursor);
    out->revision = on1x::sdk::wire::get64(cursor);
    out->payload_size = on1x::sdk::wire::get64(cursor);
    std::memcpy(out->digest_sha256, cursor, 32);
    if (out->magic != ON1X_SDK_SERIAL_ENVELOPE_MAGIC) {
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    if (out->schema_major != supported_major) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    if (out->payload_size > maximum_payload) {
        return ON1X_SDK_LIMIT;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_protocol_encode(
    const On1xSDK_ProtocolMessage *message,
    const On1xSDK_Allocator *allocator,
    On1xSDK_OwnedBytes *out
) {
    if (out) {
        *out = {};
    }
    if (!message || !allocator || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = validate_message(
        *message, ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES -
            ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    const auto total =
        ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE + message->payload.size;
    void *allocation = nullptr;
    status = on1x_sdk_allocate(
        allocator, total, alignof(std::max_align_t), &allocation
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    auto *cursor = static_cast<std::uint8_t *>(allocation);
    on1x::sdk::wire::put32(cursor, ON1X_SDK_PLUGIN_PROTOCOL_MAGIC);
    on1x::sdk::wire::put16(cursor, message->protocol_major);
    on1x::sdk::wire::put16(cursor, message->protocol_minor);
    on1x::sdk::wire::put16(cursor, message->kind);
    on1x::sdk::wire::put16(cursor, message->flags);
    on1x::sdk::wire::put32(
        cursor, static_cast<std::uint32_t>(message->status)
    );
    on1x::sdk::wire::put64(cursor, message->request_id);
    on1x::sdk::wire::put64(cursor, message->owner_context);
    on1x::sdk::wire::put64(cursor, message->object_id);
    on1x::sdk::wire::put64(cursor, message->revision);
    on1x::sdk::wire::put32(
        cursor, static_cast<std::uint32_t>(message->payload.size)
    );
    on1x::sdk::wire::put32(cursor, 0);
    on1x::sdk::wire::put_bytes(
        cursor, message->payload.data, message->payload.size
    );
    out->data = static_cast<std::uint8_t *>(allocation);
    out->size = total;
    out->capacity = total;
    out->allocator = allocator;
    out->owner_context = 0;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_protocol_decode(
    On1xSDK_ByteView frame, std::size_t maximum_payload,
    On1xSDK_ProtocolMessage *out
) {
    if (out) {
        *out = {};
    }
    if (!out || (!frame.data && frame.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (frame.size < ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE) {
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    if (frame.size > ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES) {
        return ON1X_SDK_LIMIT;
    }
    const auto *cursor = frame.data;
    if (on1x::sdk::wire::get32(cursor) !=
        ON1X_SDK_PLUGIN_PROTOCOL_MAGIC) {
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    out->protocol_major = on1x::sdk::wire::get16(cursor);
    out->protocol_minor = on1x::sdk::wire::get16(cursor);
    out->kind = on1x::sdk::wire::get16(cursor);
    out->flags = on1x::sdk::wire::get16(cursor);
    out->status = static_cast<On1xSDK_Status>(
        on1x::sdk::wire::get32(cursor)
    );
    out->request_id = on1x::sdk::wire::get64(cursor);
    out->owner_context = on1x::sdk::wire::get64(cursor);
    out->object_id = on1x::sdk::wire::get64(cursor);
    out->revision = on1x::sdk::wire::get64(cursor);
    const auto payload_size = on1x::sdk::wire::get32(cursor);
    const auto reserved = on1x::sdk::wire::get32(cursor);
    if (reserved || payload_size != frame.size -
            ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE) {
        *out = {};
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    out->payload = {cursor, payload_size};
    const auto status = validate_message(
        *out, std::min<std::size_t>(
            maximum_payload,
            ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES -
                ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE
        )
    );
    if (status != ON1X_SDK_OK) {
        *out = {};
        return status == ON1X_SDK_ABI_MISMATCH ||
                      status == ON1X_SDK_LIMIT
            ? status : ON1X_SDK_PROTOCOL_ERROR;
    }
    return ON1X_SDK_OK;
}

struct On1xSDK_ProtocolSession {
    std::uint32_t role = 0;
    std::uint32_t max_inflight = 0;
    std::uint32_t max_control = 0;
    On1xSDK_LifecycleState state = ON1X_SDK_LIFECYCLE_NEW;
    bool activation_seen = false;
    std::set<std::uint64_t> outgoing;
    std::set<std::uint64_t> incoming;
};

extern "C" On1xSDK_Status on1x_sdk_protocol_session_create(
    const On1xSDK_ProtocolSessionConfig *config,
    On1xSDK_ProtocolSession **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!config || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto status = on1x_sdk_abi_header_validate(
        config, sizeof(On1xSDK_ProtocolSessionConfig),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if ((config->role != ON1X_SDK_PROTOCOL_ROLE_HOST &&
         config->role != ON1X_SDK_PROTOCOL_ROLE_WORKER) ||
        !config->max_inflight || config->max_inflight > 4096 ||
        config->max_control_bytes <
            ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE ||
        config->max_control_bytes >
            ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES ||
        config->reserved) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        auto *session = new On1xSDK_ProtocolSession;
        session->role = config->role;
        session->max_inflight = config->max_inflight;
        session->max_control = config->max_control_bytes;
        *out = session;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_protocol_session_destroy(
    On1xSDK_ProtocolSession *session
) {
    delete session;
}

extern "C" On1xSDK_Status on1x_sdk_protocol_session_accept(
    On1xSDK_ProtocolSession *session, On1xSDK_ProtocolDirection direction,
    const On1xSDK_ProtocolMessage *message
) {
    if (!session || !message ||
        (direction != ON1X_SDK_PROTOCOL_OUTGOING &&
         direction != ON1X_SDK_PROTOCOL_INCOMING)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = validate_message(
        *message,
        session->max_control - ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    const bool outgoing = direction == ON1X_SDK_PROTOCOL_OUTGOING;
    const bool host = session->role == ON1X_SDK_PROTOCOL_ROLE_HOST;
    switch (message->kind) {
    case ON1X_SDK_PROTOCOL_HELLO:
        if (session->state != ON1X_SDK_LIFECYCLE_NEW ||
            outgoing != host) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->state = ON1X_SDK_LIFECYCLE_VALIDATED;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_HELLO_ACK:
        if (session->state != ON1X_SDK_LIFECYCLE_VALIDATED ||
            outgoing == host) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->state = ON1X_SDK_LIFECYCLE_INITIALIZING;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_ACTIVATE:
        if (session->state != ON1X_SDK_LIFECYCLE_INITIALIZING ||
            outgoing != host || session->activation_seen) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->activation_seen = true;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_ACTIVE:
        if (session->state != ON1X_SDK_LIFECYCLE_INITIALIZING ||
            outgoing == host || !session->activation_seen) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->state = ON1X_SDK_LIFECYCLE_ACTIVE;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_REQUEST: {
        if (session->state != ON1X_SDK_LIFECYCLE_ACTIVE) {
            return ON1X_SDK_INVALID_STATE;
        }
        auto &set = outgoing ? session->outgoing : session->incoming;
        if (set.contains(message->request_id)) {
            return ON1X_SDK_DUPLICATE;
        }
        if (set.size() >= session->max_inflight) {
            return ON1X_SDK_LIMIT;
        }
        try {
            (void)set.insert(message->request_id);
        } catch (const std::bad_alloc &) {
            return ON1X_SDK_LIMIT;
        } catch (...) {
            return ON1X_SDK_ERROR;
        }
        return ON1X_SDK_OK;
    }
    case ON1X_SDK_PROTOCOL_RESPONSE: {
        if (session->state != ON1X_SDK_LIFECYCLE_ACTIVE) {
            return ON1X_SDK_INVALID_STATE;
        }
        auto &set = outgoing ? session->incoming : session->outgoing;
        const auto found = set.find(message->request_id);
        if (found == set.end()) {
            return ON1X_SDK_NOT_FOUND;
        }
        set.erase(found);
        return ON1X_SDK_OK;
    }
    case ON1X_SDK_PROTOCOL_CANCEL:
        if (session->state != ON1X_SDK_LIFECYCLE_ACTIVE) {
            return ON1X_SDK_INVALID_STATE;
        }
        if (!(outgoing ? session->outgoing : session->incoming)
                 .contains(message->request_id)) {
            return ON1X_SDK_NOT_FOUND;
        }
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_SHUTDOWN:
        if (session->state != ON1X_SDK_LIFECYCLE_ACTIVE ||
            outgoing != host || !session->incoming.empty() ||
            !session->outgoing.empty()) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->state = ON1X_SDK_LIFECYCLE_STOPPING;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_SHUTDOWN_ACK:
        if (session->state != ON1X_SDK_LIFECYCLE_STOPPING ||
            outgoing == host) {
            return ON1X_SDK_INVALID_STATE;
        }
        session->state = ON1X_SDK_LIFECYCLE_CLOSED;
        return ON1X_SDK_OK;
    case ON1X_SDK_PROTOCOL_EVENT:
        return session->state == ON1X_SDK_LIFECYCLE_ACTIVE
            ? ON1X_SDK_OK : ON1X_SDK_INVALID_STATE;
    default:
        return ON1X_SDK_PROTOCOL_ERROR;
    }
}

extern "C" std::uint32_t on1x_sdk_protocol_session_state(
    const On1xSDK_ProtocolSession *session
) {
    return session
        ? static_cast<std::uint32_t>(session->state)
        : static_cast<std::uint32_t>(ON1X_SDK_LIFECYCLE_FAILED);
}

#ifndef ON1X_SDK_PLUGIN_PROTOCOL_H
#define ON1X_SDK_PLUGIN_PROTOCOL_H

#include "On1xSDK-Allocator.h"
#include "On1xSDK-String.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PLUGIN_PROTOCOL_MAJOR 2u
#define ON1X_SDK_PLUGIN_PROTOCOL_MINOR 0u
#define ON1X_SDK_PLUGIN_PROTOCOL_MAGIC UINT32_C(0x4f315850) /* O1XP */
#define ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE 56u
#define ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES (1024u * 1024u)

typedef uint32_t On1xSDK_ProtocolMessageKind;
enum {
    ON1X_SDK_PROTOCOL_HELLO = 1,
    ON1X_SDK_PROTOCOL_HELLO_ACK = 2,
    ON1X_SDK_PROTOCOL_ACTIVATE = 3,
    ON1X_SDK_PROTOCOL_ACTIVE = 4,
    ON1X_SDK_PROTOCOL_REQUEST = 5,
    ON1X_SDK_PROTOCOL_RESPONSE = 6,
    ON1X_SDK_PROTOCOL_CANCEL = 7,
    ON1X_SDK_PROTOCOL_SHUTDOWN = 8,
    ON1X_SDK_PROTOCOL_SHUTDOWN_ACK = 9,
    ON1X_SDK_PROTOCOL_EVENT = 10
};

typedef uint32_t On1xSDK_ProtocolFlags;
enum {
    ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD = 1u << 0,
    ON1X_SDK_PROTOCOL_FLAG_FINAL = 1u << 1
};

typedef struct On1xSDK_ProtocolMessage {
    uint16_t protocol_major;
    uint16_t protocol_minor;
    uint16_t kind;
    uint16_t flags;
    On1xSDK_Status status;
    uint64_t request_id;
    uint64_t owner_context;
    uint64_t object_id;
    uint64_t revision;
    On1xSDK_ByteView payload;
} On1xSDK_ProtocolMessage;

typedef uint32_t On1xSDK_ProtocolRole;
enum {
    ON1X_SDK_PROTOCOL_ROLE_HOST = 1,
    ON1X_SDK_PROTOCOL_ROLE_WORKER = 2
};

typedef uint32_t On1xSDK_ProtocolDirection;
enum {
    ON1X_SDK_PROTOCOL_OUTGOING = 1,
    ON1X_SDK_PROTOCOL_INCOMING = 2
};

typedef struct On1xSDK_ProtocolSessionConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t role;
    uint32_t max_inflight;
    uint32_t max_control_bytes;
    uint32_t reserved;
} On1xSDK_ProtocolSessionConfig;

On1xSDK_Status on1x_sdk_protocol_encode(
    const On1xSDK_ProtocolMessage *message,
    const On1xSDK_Allocator *allocator,
    On1xSDK_OwnedBytes *out
);
On1xSDK_Status on1x_sdk_protocol_decode(
    On1xSDK_ByteView frame,
    size_t maximum_payload,
    On1xSDK_ProtocolMessage *out
);
On1xSDK_Status on1x_sdk_protocol_session_create(
    const On1xSDK_ProtocolSessionConfig *config,
    On1xSDK_ProtocolSession **out
);
void on1x_sdk_protocol_session_destroy(On1xSDK_ProtocolSession *session);
On1xSDK_Status on1x_sdk_protocol_session_accept(
    On1xSDK_ProtocolSession *session,
    On1xSDK_ProtocolDirection direction,
    const On1xSDK_ProtocolMessage *message
);
uint32_t on1x_sdk_protocol_session_state(
    const On1xSDK_ProtocolSession *session
);

#ifdef __cplusplus
}
#endif

#endif

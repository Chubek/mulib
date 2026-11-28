#ifndef ON1X_SDK_TRANSPORT_H
#define ON1X_SDK_TRANSPORT_H

#include "On1xSDK-Budget.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-SharedMemory.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_TransportConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_Context *context;
    int connected_stream_fd; /* Borrowed; never closed by the transport. */
    uint32_t max_control_bytes;
} On1xSDK_TransportConfig;

On1xSDK_Status on1x_sdk_transport_create(
    const On1xSDK_TransportConfig *config, On1xSDK_Transport **out
);
void on1x_sdk_transport_destroy(On1xSDK_Transport *transport);
On1xSDK_Status on1x_sdk_transport_send(
    On1xSDK_Transport *transport,
    On1xSDK_ByteView frame,
    uint64_t absolute_deadline_ns,
    const On1xSDK_Cancellation *cancellation,
    On1xSDK_Budget *budget
);
On1xSDK_Status on1x_sdk_transport_receive(
    On1xSDK_Transport *transport,
    uint64_t absolute_deadline_ns,
    const On1xSDK_Cancellation *cancellation,
    On1xSDK_Budget *budget,
    On1xSDK_OwnedBytes *out
);

#ifdef __cplusplus
}
#endif

#endif

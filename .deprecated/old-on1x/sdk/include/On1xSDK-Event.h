#ifndef ON1X_SDK_EVENT_H
#define ON1X_SDK_EVENT_H

#include "On1xSDK-String.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_EventKind;
enum {
    ON1X_SDK_EVENT_VALIDATED = 1,
    ON1X_SDK_EVENT_INITIALIZING = 2,
    ON1X_SDK_EVENT_ACTIVE = 3,
    ON1X_SDK_EVENT_COMPENSATING = 4,
    ON1X_SDK_EVENT_CLOSED = 5,
    ON1X_SDK_EVENT_FAILED = 6
};

typedef struct On1xSDK_Event {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t kind;
    uint32_t reserved;
    uint64_t sequence;
    On1xSDK_StringView provider;
    On1xSDK_Status status;
} On1xSDK_Event;

typedef On1xSDK_Status (*On1xSDK_EventFn)(
    void *userdata, const On1xSDK_Event *event
);

typedef struct On1xSDK_EventSink {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_EventFn emit;
    void *userdata;
} On1xSDK_EventSink;

On1xSDK_Status on1x_sdk_event_emit(
    const On1xSDK_EventSink *sink, const On1xSDK_Event *event
);

#ifdef __cplusplus
}
#endif

#endif

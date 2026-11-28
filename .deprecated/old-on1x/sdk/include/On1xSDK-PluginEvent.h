#ifndef ON1X_SDK_PLUGIN_EVENT_H
#define ON1X_SDK_PLUGIN_EVENT_H

#include "On1xSDK-Event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef On1xSDK_Event On1xSDK_PluginEvent;
typedef On1xSDK_EventSink On1xSDK_PluginEventSink;

On1xSDK_Status on1x_sdk_plugin_event_emit(
    const On1xSDK_PluginEventSink *sink,
    const On1xSDK_PluginEvent *event
);

#ifdef __cplusplus
}
#endif

#endif

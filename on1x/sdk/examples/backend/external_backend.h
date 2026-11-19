#ifndef ON1X_SDK_EXAMPLE_EXTERNAL_BACKEND_H
#define ON1X_SDK_EXAMPLE_EXTERNAL_BACKEND_H

#include "On1xSDK-Backend.h"
#include "On1xSDK-Registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers a deterministic serialized-MIR receipt backend named example.receipt. */
On1xSDK_Status on1x_sdk_example_backend_register(On1xSDK_Registry *registry);

#ifdef __cplusplus
}
#endif

#endif

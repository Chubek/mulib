#ifndef ON1X_SDK_CONFIGURATION_H
#define ON1X_SDK_CONFIGURATION_H

#include "On1xSDK-Metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_Configuration {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_MetadataItem *items;
    size_t item_count;
} On1xSDK_Configuration;

On1xSDK_Status on1x_sdk_configuration_validate(
    const On1xSDK_Configuration *configuration
);

#ifdef __cplusplus
}
#endif

#endif

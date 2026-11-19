#ifndef ON1X_SDK_PROVENANCE_H
#define ON1X_SDK_PROVENANCE_H

#include "On1xSDK-String.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_Provenance {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_StringView provider;
    On1xSDK_StringView artifact_kind;
    uint64_t source_id;
    uint64_t source_revision;
    uint8_t digest_sha256[32];
    uint32_t has_digest;
    uint32_t reserved;
} On1xSDK_Provenance;

On1xSDK_Status on1x_sdk_provenance_validate(
    const On1xSDK_Provenance *provenance
);

#ifdef __cplusplus
}
#endif

#endif

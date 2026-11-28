/* Deterministic build-graph provenance, without paths or ambient host state. */
#ifndef ON1X_SDK_REPRODUCIBILITY_H
#define ON1X_SDK_REPRODUCIBILITY_H

#include "On1xSDK-BuildGraph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_ReproducibilityRecord {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString schema;       /* "on1x.build-graph/v1" */
    On1xSDK_ProjectString graph_sha256; /* Borrowed from graph. */
    size_t input_count;
} On1xSDK_ReproducibilityRecord;

/*
 * Describes the immutable graph's canonical schema, digest and explicit input
 * count. The record and its borrowed views remain valid only while graph lives.
 * Caller initializes abi_version and struct_size; unknown trailing fields are
 * ignored and the known prefix is written on success.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_reproducibility_describe(
    const On1xSDK_BuildGraph *graph,
    On1xSDK_ReproducibilityRecord *record);

#ifdef __cplusplus
}
#endif

#endif

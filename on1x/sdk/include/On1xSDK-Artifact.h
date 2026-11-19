/* Immutable staged backend artifacts. */
#ifndef ON1X_SDK_ARTIFACT_H
#define ON1X_SDK_ARTIFACT_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_ARTIFACT_BYTES (16u * 1024u * 1024u)
#define ON1X_SDK_ARTIFACT_DIGEST_BYTES 32u
#define ON1X_SDK_ARTIFACT_ABI_VERSION 2u

typedef struct On1xSDK_Artifact On1xSDK_Artifact;

typedef uint32_t On1xSDK_ArtifactKind;
enum {
    ON1X_SDK_ARTIFACT_UNKNOWN = 0u,
    ON1X_SDK_ARTIFACT_C11_SOURCE = 1u,
    ON1X_SDK_ARTIFACT_MIR_TEXT = 2u,
    ON1X_SDK_ARTIFACT_CFG_DOT = 3u,
    ON1X_SDK_ARTIFACT_PROGRAM_OUTPUT = 4u,
    ON1X_SDK_ARTIFACT_ASSEMBLY = 5u,
    ON1X_SDK_ARTIFACT_OBJECT = 6u,
    ON1X_SDK_ARTIFACT_EXECUTABLE = 7u
};

typedef uint32_t On1xSDK_ArtifactState;
enum {
    /* Backend output is complete but has not been published by the host. */
    ON1X_SDK_ARTIFACT_STAGED = 1u
};

enum {
    /* A provider declaration, not an independent proof of reproducibility. */
    ON1X_SDK_ARTIFACT_DECLARED_DETERMINISTIC = 1u << 0,
    /* The host invoked the provider twice and compared complete byte output. */
    ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED = 1u << 1
};

typedef struct On1xSDK_ArtifactInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ArtifactKind kind;
    On1xSDK_ArtifactState state;
    uint32_t flags;
    uint32_t input_schema;
    const char *provider_name;
    size_t provider_name_size;
    const char *target_triple;
    size_t target_triple_size;
    uint8_t sha256[ON1X_SDK_ARTIFACT_DIGEST_BYTES];
} On1xSDK_ArtifactInfo;

/* Borrowed until artifact destruction; empty buffers return NULL and zero. */
On1xSDK_Status on1x_sdk_artifact_bytes(const On1xSDK_Artifact *artifact,
                                       const uint8_t **bytes, size_t *size);
/* The returned metadata and its strings are borrowed from artifact. */
On1xSDK_Status on1x_sdk_artifact_info(const On1xSDK_Artifact *artifact,
                                      const On1xSDK_ArtifactInfo **out_info);
/* Discards the stage. No SDK backend API publishes or executes it. */
void on1x_sdk_artifact_destroy(On1xSDK_Artifact *artifact);

#ifdef __cplusplus
}
#endif

#endif

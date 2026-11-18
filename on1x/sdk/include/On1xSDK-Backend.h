/* Explicitly selected artifact callbacks for host-verified MIR. */
#ifndef ON1X_SDK_BACKEND_H
#define ON1X_SDK_BACKEND_H

#include "On1xSDK-Transformer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_ARTIFACT_BYTES (16u * 1024u * 1024u)

typedef struct On1xSDK_Artifact On1xSDK_Artifact;
typedef On1xSDK_Status (*On1xSDK_BackendFn)(void *userdata,
                                         const On1xSDK_Program *program,
                                         On1xSDK_OutputSink *artifact);

typedef struct On1xSDK_Backend {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    On1xSDK_Phase phase; /* VERIFIED_MIR only. */
    On1xSDK_BackendFn run;
    void *userdata;      /* Borrowed for the registration's lifetime. */
} On1xSDK_Backend;

/* Uses the same ABI/ownership rules as on1x_sdk_analyzer_register. */
On1xSDK_Status on1x_sdk_backend_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Backend *backend);
/*
 * Runs one named backend on host-verified MIR, exposed as canonical
 * serialized MIR bytes. The callback borrows program; it must not mutate,
 * destroy, or retain it. Missing name or mismatched phase is UNSUPPORTED.
 * The byte limit cannot exceed ON1X_SDK_MAX_ARTIFACT_BYTES. No partial
 * artifact is returned; *out is NULL on any failure. Success transfers
 * ownership of an opaque byte artifact (it may be empty). The SDK makes no
 * claim about its format or target and does not execute or publish it.
 */
On1xSDK_Status on1x_sdk_backend_run(const On1xSDK_Registry *registry,
                                   const char *name,
                                   const On1xSDK_Program *program,
                                   size_t byte_limit, On1xSDK_Artifact **out);
/* Borrowed until artifact destruction; empty buffers return NULL and zero. */
On1xSDK_Status on1x_sdk_artifact_bytes(const On1xSDK_Artifact *artifact,
                                      const uint8_t **bytes, size_t *size);
void on1x_sdk_artifact_destroy(On1xSDK_Artifact *artifact);

#ifdef __cplusplus
}
#endif

#endif

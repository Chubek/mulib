/* Bounded pre-parse source replacement and the shared byte output sink. */
#ifndef ON1X_SDK_TRANSFORMER_H
#define ON1X_SDK_TRANSFORMER_H

#include "On1xSDK-Analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_OutputSink On1xSDK_OutputSink;

/*
 * Appends exactly size bytes, copying them before return. NULL bytes
 * requires size == 0. The first write failure invalidates the entire
 * run even if the extension callback ignores it. The borrowed sink
 * is usable only during its callback, on the calling thread.
 */
On1xSDK_Status on1x_sdk_output_write(On1xSDK_OutputSink *sink,
                                    const void *bytes, size_t size);

typedef On1xSDK_Status (*On1xSDK_TransformerFn)(void *userdata,
                                             const On1xSDK_Program *program,
                                             On1xSDK_OutputSink *replacement);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    On1xSDK_Phase phase; /* SOURCE only. */
    On1xSDK_TransformerFn run;
    void *userdata;      /* Borrowed for the registration's lifetime. */
} On1xSDK_Transformer;

/* Uses the same ABI/ownership rules as on1x_sdk_analyzer_register. */
On1xSDK_Status on1x_sdk_transformer_register(On1xSDK_Registry *registry,
                                             const On1xSDK_Transformer *transformer);
/*
 * Runs one named SOURCE transformer. Result is a newly owned SOURCE
 * buffer, never a checked program; it must pass the host's normal
 * parsing/checking before use. The SDK alters no files or imports.
 * The callback borrows program; it must not mutate, destroy or retain it.
 * Missing name or mismatched phase is UNSUPPORTED. The limit is a maximum
 * byte count (0 permits an empty buffer); the hard cap is
 * ON1X_SDK_MAX_PROGRAM_BYTES.
 * Any failure leaves *out NULL and the input unchanged.
 */
On1xSDK_Status on1x_sdk_transformer_run(const On1xSDK_Registry *registry,
                                       const char *name,
                                       const On1xSDK_Program *program,
                                       size_t byte_limit, On1xSDK_Program **out);

#ifdef __cplusplus
}
#endif

#endif

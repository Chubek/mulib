/* Read-only source and verified-MIR analysis by explicitly registered code. */
#ifndef ON1X_SDK_ANALYZER_H
#define ON1X_SDK_ANALYZER_H

#include "On1xSDK-Diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_PROGRAM_BYTES (16u * 1024u * 1024u)

/*
 * Copies exactly size bytes, including embedded NULs. No parsing or checking
 * is performed; the result is always SOURCE. NULL bytes requires size == 0.
 * The caller owns the result. Handle outputs are set to NULL on failure.
 */
On1xSDK_Status on1x_sdk_program_create_source(const void *bytes, size_t size,
                                              On1xSDK_Program **out);
void on1x_sdk_program_destroy(On1xSDK_Program *program);
On1xSDK_Status on1x_sdk_program_phase(const On1xSDK_Program *program,
                                     On1xSDK_Phase *phase);
/* Borrowed until program destruction; empty buffers return NULL and zero. */
On1xSDK_Status on1x_sdk_program_bytes(const On1xSDK_Program *program,
                                     const uint8_t **bytes, size_t *size);

typedef struct On1xSDK_AnalyzerSink On1xSDK_AnalyzerSink;

#define ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS 128u
#define ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES 4096u

/*
 * Copies a record into the run's staging buffer. Codes obey registration
 * name syntax; spans refer to the input bytes (SOURCE or serialized MIR).
 * Empty messages, malformed spans/codes and unknown severities are errors.
 * The first emit failure invalidates the run, even if ignored by the callback.
 */
On1xSDK_Status on1x_sdk_analyzer_emit(On1xSDK_AnalyzerSink *sink,
                                     const On1xSDK_Diagnostic *diagnostic);

/*
 * Program and sink are borrowed for this call only. Do not destroy, mutate,
 * or retain them. Use sinks only synchronously, on the calling thread.
 */
typedef On1xSDK_Status (*On1xSDK_AnalyzerFn)(void *userdata,
                                          const On1xSDK_Program *program,
                                          On1xSDK_AnalyzerSink *sink);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    On1xSDK_Phase phase; /* SOURCE or VERIFIED_MIR; CHECKED_HIR is not exposed. */
    On1xSDK_AnalyzerFn run;
    void *userdata;      /* Borrowed for the registration's lifetime. */
} On1xSDK_Analyzer;

/* Registry rules apply: initialize abi_version and struct_size, and supply
 * trusted callbacks only. Descriptor data is copied; userdata is borrowed. */
On1xSDK_Status on1x_sdk_analyzer_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Analyzer *analyzer);
/*
 * Runs one named analyzer, and only when its registered phase matches.
 * Missing names or mismatched phases return UNSUPPORTED. After success,
 * records are delivered in emit order to diagnostics (optional). A reported
 * diagnostic severity does not change the run status. Host diagnostic
 * callbacks must not throw or retain borrowed records or strings.
 */
On1xSDK_Status on1x_sdk_analyzer_run(const On1xSDK_Registry *registry,
                                    const char *name,
                                    const On1xSDK_Program *program,
                                    const On1xSDK_DiagnosticSink *diagnostics);

#ifdef __cplusplus
}
#endif

#endif

/* Read-only source and verified-MIR analysis by explicitly registered code. */
#ifndef ON1X_SDK_ANALYZER_H
#define ON1X_SDK_ANALYZER_H

#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-MIR.h"

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

/* ABI 2: read-only analysis of a canonical VERIFIED_MIR snapshot. */
#define ON1X_SDK_MIR_ANALYZE_CAPABILITY "mir.analyze"
#define ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION 1u

/*
 * The snapshot and sink are borrowed for a single synchronous call. Do not
 * retain or release them, share the sink across threads, or re-enter/mutate
 * the active host. Release any owned SDK objects derived from the snapshot
 * before returning. Use sink->emit(sink->userdata, &record) to submit a record.
 * The host associates that sink with this exact snapshot invocation.
 *
 * This profile admits only unlocated diagnostics (use
 * on1x_sdk_span_unlocated()), with no related spans, fix-its, category, phase,
 * or provider fields. The host supplies provenance. Message text must be
 * UTF-8 with no NUL and at most ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES bytes.
 * A callback failure or the first failed sink submission discards all
 * staged diagnostics, up to ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS.
 */
typedef On1xSDK_Status (*On1xSDK_AnalyzerFnV2)(
    void *userdata, const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_DiagnosticSinkV2 *sink);

typedef struct On1xSDK_AnalyzerV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_StringView name;
    uint32_t input_schema; /* ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION, not MIR wire version. */
    uint32_t reserved;
    On1xSDK_AnalyzerFnV2 run;
    void *userdata; /* Optional; borrowed until shutdown completes. */
} On1xSDK_AnalyzerV2;

/*
 * Only available during ABI-2 register_all with a negotiated mir.analyze
 * grant. Copies the descriptor and name into the existing registry
 * transaction; a failed registration/initialization rolls it back. Names
 * are scoped separately from ABI 1. This call does not schedule analysis or
 * provide any means to promote a candidate snapshot.
 */
On1xSDK_Status on1x_sdk_analyzer_register_v2(On1xSDK_Registry *registry,
                                            const On1xSDK_AnalyzerV2 *analyzer);

#ifdef __cplusplus
}
#endif

#endif

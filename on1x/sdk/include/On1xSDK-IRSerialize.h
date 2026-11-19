/* Bounded deterministic pointer-free HIR and MIR wire encodings. */
#ifndef ON1X_SDK_IR_SERIALIZE_H
#define ON1X_SDK_IR_SERIALIZE_H

#include "On1xSDK-HIR.h"
#include "On1xSDK-MIR.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_IRBlob On1xSDK_IRBlob;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_IR_WIRE_HIR_V1 = 1,
    ON1X_SDK_IR_WIRE_MIR_CANONICAL = 2,
    ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1 = 3
} On1xSDK_IRWireFormat;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_IRIdentity identity;
    On1xSDK_IRStringView filename;
    size_t max_bytes;
    size_t max_nodes;
    size_t max_work;
    size_t max_depth;
} On1xSDK_IRDecodeOptions;

/* Immutable blobs may be retained and read concurrently. */
void on1x_sdk_ir_blob_retain(On1xSDK_IRBlob *blob);
void on1x_sdk_ir_blob_release(On1xSDK_IRBlob *blob);
On1xSDK_Status on1x_sdk_ir_blob_bytes(
    const On1xSDK_IRBlob *blob,
    On1xSDK_IRBytesView *out_bytes);

/*
 * HIR-v1 is an inspection schema for monomorphic, elaborated function trees.
 * A checked snapshot containing declarations or metadata outside that schema
 * returns UNSUPPORTED rather than erasing them.
 */
On1xSDK_Status on1x_sdk_hir_serialize(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRBlob **out_blob);
On1xSDK_Status on1x_sdk_hir_deserialize(
    const void *bytes,
    size_t size,
    const On1xSDK_IRDecodeOptions *options,
    On1xSDK_HIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

/*
 * Canonical MIR is the existing On1x S-expression. Envelope-v1 adds only
 * revision, generation and provenance; it does not serialize owner pointers or
 * validation authority. Every deserialized snapshot is CANDIDATE even when the
 * payload passes structural verification.
 */
On1xSDK_Status on1x_sdk_mir_serialize(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRWireFormat format,
    On1xSDK_IRBlob **out_blob);
On1xSDK_Status on1x_sdk_mir_deserialize(
    const void *bytes,
    size_t size,
    On1xSDK_IRWireFormat format,
    const On1xSDK_IRDecodeOptions *options,
    On1xSDK_MIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_IRWireFormat) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif

/*
 * Shared C11 types for the SDK 2 type, semantic, HIR and MIR adapters.
 *
 * This header deliberately does not implement an SDK context or allocator.
 * The owner pointer is a borrowed identity token supplied by the ABI-2 host.
 */
#ifndef ON1X_SDK_SEMANTIC_TYPES_H
#define ON1X_SDK_SEMANTIC_TYPES_H

#include "On1xSDK-Diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_SEMANTICS_IR_ABI_VERSION 2u
#define ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION 1u

#define ON1X_SDK_IR_MAX_PROVENANCE_BYTES 4096u
#define ON1X_SDK_IR_MAX_STRING_BYTES (1024u * 1024u)
#define ON1X_SDK_IR_MAX_WIRE_BYTES (16u * 1024u * 1024u)
#define ON1X_SDK_IR_MAX_NODES 262144u
#define ON1X_SDK_IR_MAX_DEPTH 256u

typedef struct {
    const char *data;
    size_t size;
} On1xSDK_IRStringView;

typedef struct {
    const uint8_t *data;
    size_t size;
} On1xSDK_IRBytesView;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_IR_CANDIDATE = 1,
    ON1X_SDK_IR_CHECKED_HIR = 2,
    ON1X_SDK_IR_VERIFIED_MIR = 3
} On1xSDK_IRValidationState;

/*
 * Identity is process-local. owner is borrowed and never dereferenced by this
 * module; the host must keep the context alive while any related object lives.
 * generation and revision are nonzero. External construction APIs require
 * state == CANDIDATE and cannot manufacture checked/verified objects.
 */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Context *owner;
    uint64_t generation;
    uint64_t revision;
    On1xSDK_IRValidationState state;
    uint32_t schema_version;
} On1xSDK_IRIdentity;

typedef struct {
    On1xSDK_IRStringView source;
    uint64_t byte_begin;
    uint64_t byte_end;
    uint64_t line;
    uint64_t column;
} On1xSDK_IRSourceSpan;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_IR_FAILURE_NONE = 0,
    ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION = 1,
    ON1X_SDK_IR_FAILURE_MALFORMED = 2,
    ON1X_SDK_IR_FAILURE_SEMANTIC = 3,
    ON1X_SDK_IR_FAILURE_UNSUPPORTED = 4,
    ON1X_SDK_IR_FAILURE_LIMIT = 5,
    ON1X_SDK_IR_FAILURE_STALE_REVISION = 6,
    ON1X_SDK_IR_FAILURE_CROSS_CONTEXT = 7,
    ON1X_SDK_IR_FAILURE_CALLBACK = 8,
    ON1X_SDK_IR_FAILURE_LIFECYCLE = 9
} On1xSDK_IRFailure;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t max_nodes;
    size_t max_work;
    size_t max_depth;
} On1xSDK_IRLimits;

/*
 * Checks only the ABI prefix and process-local identity fields. It does not
 * validate that owner denotes a live host context.
 */
On1xSDK_Status on1x_sdk_ir_identity_validate(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_IRValidationState required_state);

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_IRValidationState) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_IRFailure) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif

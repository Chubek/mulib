#ifndef ON1X_SDK_PPX_H
#define ON1X_SDK_PPX_H

#include "On1xSDK-AST.h"
#include "On1xSDK-Options.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_PPX_SCHEMA_FORMAT "on1x.sexpr-ast/v1"
#define ON1X_SDK_PPX_SCHEMA_VERSION 1u

typedef struct On1xSDK_PPXLimits {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t max_wire_bytes;
    size_t max_wire_nodes;
    size_t max_depth;
} On1xSDK_PPXLimits;

On1xSDK_Status on1x_sdk_ppx_limits(On1xSDK_PPXLimits *out_limits);
On1xSDK_Status on1x_sdk_ppx_validate_schema(
    const char *format,
    size_t format_size,
    uint32_t version);

/*
 * Encodes the canonical Unit-v1 profile into caller-owned storage. Passing a
 * NULL buffer with capacity zero reports the required size (including newline)
 * in out_size. Unsupported source constructs reject rather than disappear.
 */
On1xSDK_Status on1x_sdk_ppx_encode(
    const On1xSDK_AST *ast,
    char *buffer,
    size_t capacity,
    size_t *out_size);

/*
 * Bounded schema validation and canonical AST reconstruction. The result is a
 * candidate only; this operation does not claim semantic equivalence or publish
 * a checked representation.
 */
On1xSDK_Status on1x_sdk_ppx_readmit(
    const On1xSDK_AST *base,
    const char *format,
    size_t format_size,
    uint32_t version,
    const void *wire,
    size_t wire_size,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_candidate);

#ifdef __cplusplus
}
#endif

#endif

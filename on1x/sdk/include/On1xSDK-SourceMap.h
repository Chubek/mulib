#ifndef ON1X_SDK_SOURCE_MAP_H
#define ON1X_SDK_SOURCE_MAP_H

#include "On1xSDK-Source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_SourceLocation {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t source_id;
    uint64_t revision;
    size_t byte_offset;
    size_t line;   /* one-based display line */
    size_t column; /* one-based byte column */
    const char *path;
    size_t path_size;
} On1xSDK_SourceLocation;

typedef struct On1xSDK_SourceSpan {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t source_id;
    uint64_t revision;
    size_t byte_begin;
    size_t byte_end;
} On1xSDK_SourceSpan;

On1xSDK_Status on1x_sdk_source_location_at(
    const On1xSDK_SourceRevision *revision,
    size_t byte_offset,
    On1xSDK_SourceLocation *out_location);
On1xSDK_Status on1x_sdk_source_validate_span(
    const On1xSDK_SourceRevision *revision,
    const On1xSDK_SourceSpan *span);

#ifdef __cplusplus
}
#endif

#endif

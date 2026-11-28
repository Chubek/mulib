#ifndef ON1X_SDK_SPAN_H
#define ON1X_SDK_SPAN_H

#include "On1xSDK-ABI.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_SourceRevision {
    uint64_t source_id;
    uint64_t revision;
    uint64_t byte_size;
} On1xSDK_SourceRevision;

/* Zero-based half-open byte offsets. UINT32_MAX display positions are unknown. */
typedef struct On1xSDK_SourceSpan {
    uint64_t source_id;
    uint64_t revision;
    uint64_t byte_begin;
    uint64_t byte_end;
    uint32_t line_begin;
    uint32_t column_begin;
    uint32_t line_end;
    uint32_t column_end;
} On1xSDK_SourceSpan;

On1xSDK_SourceSpan on1x_sdk_span_unlocated(void);
On1xSDK_Status on1x_sdk_span_validate(
    const On1xSDK_SourceSpan *span,
    const On1xSDK_SourceRevision *source,
    int allow_unlocated
);

#ifdef __cplusplus
}
#endif

#endif

/* Revision-bound candidate edit construction.  This does not certify a repair. */
#ifndef ON1X_SDK_CODE_ACTION_H
#define ON1X_SDK_CODE_ACTION_H

#include "On1xSDK-Project.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_CODE_ACTION_MAX_EDITS 64u
#define ON1X_SDK_CODE_ACTION_MAX_OUTPUT (2u * 1024u * 1024u)

typedef struct On1xSDK_CodeActionEdit {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t begin;
    size_t end;
    On1xSDK_ProjectString replacement;
} On1xSDK_CodeActionEdit;

/*
 * Applies sorted, nonoverlapping candidate edits only when source_sha256
 * exactly names source.  Output is an unverified candidate and must pass the
 * authoritative compiler checks before publication.  Stale revisions,
 * malformed spans, duplicate/overlapping edits, and output limits fail without
 * producing a buffer.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status
on1x_sdk_code_action_apply_candidate(
    On1xSDK_ProjectBytes source,
    On1xSDK_ProjectString source_sha256,
    const On1xSDK_CodeActionEdit *edits, size_t edit_count,
    const On1xSDK_ProjectAllocator *allocator,
    const On1xSDK_ProjectDiagnosticSink *sink,
    On1xSDK_ProjectBuffer **out_candidate);

#ifdef __cplusplus
}
#endif

#endif

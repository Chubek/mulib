#ifndef ON1X_SDK_SOURCE_H
#define ON1X_SDK_SOURCE_H

#include "On1xSDK-Compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_SourceEncoding;
enum {
    /*
     * Canonical byte-source policy. Non-UTF-8 bytes remain legal where the
     * canonical scanner accepts them; NUL and malformed tokens still reject.
     */
    ON1X_SDK_SOURCE_BYTES = 1,
    /* Validate the complete source as shortest-form Unicode UTF-8. */
    ON1X_SDK_SOURCE_UTF8 = 2
};

typedef struct On1xSDK_SourceInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t source_id;
    uint64_t revision;
    uint32_t encoding;
    uint32_t is_current;
    const char *path;
    size_t path_size;
    size_t byte_size;
} On1xSDK_SourceInfo;

/*
 * path is normalized to an absolute lexical/canonical path and copied.
 * bytes may be NULL only when byte_size is zero.
 */
On1xSDK_Status on1x_sdk_source_create(
    On1xSDK_Session *session,
    const char *path,
    size_t path_size,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source);
On1xSDK_Status on1x_sdk_source_from_file(
    On1xSDK_Session *session,
    const char *path,
    size_t path_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source);
On1xSDK_Status on1x_sdk_source_retain(On1xSDK_Source *source);
void on1x_sdk_source_release(On1xSDK_Source *source);

/* expected_revision must name the current source revision exactly. */
On1xSDK_Status on1x_sdk_source_replace(
    On1xSDK_Source *source,
    uint64_t expected_revision,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_SourceSnapshot **out_revision);

On1xSDK_Status on1x_sdk_source_snapshot(
    const On1xSDK_Source *source,
    On1xSDK_SourceSnapshot **out_revision);
On1xSDK_Status on1x_sdk_source_info(
    const On1xSDK_Source *source,
    On1xSDK_SourceInfo *out_info);

On1xSDK_Status on1x_sdk_source_revision_retain(
    On1xSDK_SourceSnapshot *revision);
void on1x_sdk_source_revision_release(On1xSDK_SourceSnapshot *revision);
On1xSDK_Status on1x_sdk_source_revision_info(
    const On1xSDK_SourceSnapshot *revision,
    On1xSDK_SourceInfo *out_info);
On1xSDK_Status on1x_sdk_source_revision_bytes(
    const On1xSDK_SourceSnapshot *revision,
    const void **out_bytes,
    size_t *out_size);
On1xSDK_Status on1x_sdk_source_revision_is_current(
    const On1xSDK_SourceSnapshot *revision,
    uint32_t *out_current);

#ifdef __cplusplus
}
#endif

#endif

/** @file mulib-initsysutils.h
 * @brief Linux initramfs inspection/building and explicit initialization steps.
 * Define MULIB_IMPL_INITSYSUTILS and MULIB_IMPL_CPIO once.
 */
#ifndef MULIB_INITSYSUTILS_H
#define MULIB_INITSYSUTILS_H
#include "mulib-cpio.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_INITSYSUTILS_OK = 0, MULIB_INITSYSUTILS_END = 1,
       MULIB_INITSYSUTILS_INVALID = -10, MULIB_INITSYSUTILS_UNSUPPORTED = -11,
       MULIB_INITSYSUTILS_PATH = -12 };
/** Borrowed uncompressed initramfs cursor. archive increments after trailers,
 * making hardlink identity scoped to its archive. Initialize with {0}. */
typedef struct { size_t offset, archive; } mulib_initsysutils_cursor_t;
/** @brief Iterate Linux newc/crc initramfs, skipping zero padding and trailers.
 * Concatenated archives and missing final trailer accepted. Empty is valid.
 * Compressed members return UNSUPPORTED. Output borrows input. */
int mulib_initsysutils_next(const void *data, size_t bytes, mulib_initsysutils_cursor_t *cursor, mulib_cpio_entry_t *entry);
/** @brief Check root-relative extraction name. Allows leading ./ and directory
 * '.', rejects absolute paths, '..' components, empty components and NUL.
 * This lexical check does not protect a filesystem walk through symlinks. */
int mulib_initsysutils_path(const char *name, size_t bytes);
/** @brief Append initramfs entry after Linux file-type/data and path validation.
 * Hardlink metadata preserved. Caller orders parents before children. */
int mulib_initsysutils_add(mulib_cpio_writer_t *writer, const mulib_cpio_entry_t *entry);
/** @brief Validate every uncompressed entry without extracting. Returns record
 * count and whether a regular executable 'init' exists (not execution proof).
 * Results unchanged on error. Last matching init entry determines the flag. */
int mulib_initsysutils_validate(const void *data, size_t bytes, size_t *entries, int *has_init);
/** A step is a caller-defined operation, e.g. mount or device setup. Failed
 * apply must clean up itself. undo is called in reverse for completed steps. */
typedef struct {
    const char *name; void *context;
    int (*apply)(void *); int (*undo)(void *);
} mulib_initsysutils_step_t;
/** @brief Execute ordered steps. On failure attempt all rollbacks. Returns
 * original apply error; failed_index and first_undo_error report both outcomes.
 * On success failed_index=SIZE_MAX. No process-global side effects of its own. */
int mulib_initsysutils_run(const mulib_initsysutils_step_t *steps, size_t count,
                          size_t *failed_index, int *first_undo_error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INITSYSUTILS) && !defined(MULIB_INITSYSUTILS_IMPLEMENTED)
#define MULIB_INITSYSUTILS_IMPLEMENTED
#include <string.h>
int mulib_initsysutils_path(const char *name, size_t bytes) {
    size_t at = 0, start;
    if (!name || !bytes || name[0] == '/') return MULIB_INITSYSUTILS_PATH;
    while (at < bytes) {
        start = at;
        while (at < bytes && name[at] != '/') { if (!name[at]) return MULIB_INITSYSUTILS_PATH; ++at; }
        if (at == start || (at - start == 2 && name[start] == '.' && name[start + 1] == '.')) return MULIB_INITSYSUTILS_PATH;
        if (at < bytes && ++at == bytes) return MULIB_INITSYSUTILS_PATH;
    }
    return 0;
}
static int mulib_initsysutils_entry_(const mulib_cpio_entry_t *e) {
    unsigned type;
    if (!e || (!e->data && e->size) || e->name_bytes >= 4096) return MULIB_INITSYSUTILS_INVALID;
    if (mulib_initsysutils_path(e->name, e->name_bytes)) return MULIB_INITSYSUTILS_PATH;
    type = e->mode & MULIB_CPIO_TYPE_MASK;
    if (type != MULIB_CPIO_REGULAR && type != MULIB_CPIO_SYMLINK && type != MULIB_CPIO_DIRECTORY &&
        type != MULIB_CPIO_CHAR && type != MULIB_CPIO_BLOCK && type != MULIB_CPIO_FIFO && type != MULIB_CPIO_SOCKET)
        return MULIB_INITSYSUTILS_UNSUPPORTED;
    if (!e->nlink || (type == MULIB_CPIO_SYMLINK && (!e->size || e->size > 4096 || memchr(e->data, 0, e->size))) ||
        (type != MULIB_CPIO_REGULAR && type != MULIB_CPIO_SYMLINK && e->size)) return MULIB_INITSYSUTILS_INVALID;
    return 0;
}
int mulib_initsysutils_next(const void *data, size_t bytes, mulib_initsysutils_cursor_t *cursor, mulib_cpio_entry_t *entry) {
    const uint8_t *p = (const uint8_t *)data; mulib_initsysutils_cursor_t c;
    mulib_cpio_entry_t e; size_t at; int rc;
    if ((!p && bytes) || !cursor || !entry || cursor->offset > bytes) return MULIB_INITSYSUTILS_INVALID;
    c = *cursor;
    for (;;) {
        while (c.offset < bytes && !p[c.offset]) ++c.offset;
        if (c.offset == bytes) { *cursor = c; return MULIB_INITSYSUTILS_END; }
        /* Linux requires each uncompressed header at a four-byte boundary. */
        if (c.offset & 3) return MULIB_CPIO_FORMAT;
        if (bytes - c.offset < 6 || (memcmp(p + c.offset, "070701", 6) && memcmp(p + c.offset, "070702", 6)))
            return MULIB_INITSYSUTILS_UNSUPPORTED;
        at = 0; rc = mulib_cpio_next(p + c.offset, bytes - c.offset, &at, &e);
        if (rc < 0) return rc;
        c.offset += at;
        if (rc == MULIB_CPIO_TRAILER) {
            if (c.archive == SIZE_MAX) return MULIB_INITSYSUTILS_INVALID;
            ++c.archive; continue;
        }
        if (e.name_storage > 4096) return MULIB_INITSYSUTILS_INVALID;
        rc = mulib_initsysutils_entry_(&e); if (rc) return rc;
        *cursor = c; *entry = e; return 0;
    }
}
int mulib_initsysutils_add(mulib_cpio_writer_t *w, const mulib_cpio_entry_t *e) {
    int rc = mulib_initsysutils_entry_(e); return rc ? rc : mulib_cpio_add(w, e);
}
int mulib_initsysutils_validate(const void *data, size_t bytes, size_t *entries, int *has_init) {
    mulib_initsysutils_cursor_t c = {0, 0}; mulib_cpio_entry_t e;
    size_t n = 0, at; int init = 0, rc;
    if (!entries || !has_init) return MULIB_INITSYSUTILS_INVALID;
    while ((rc = mulib_initsysutils_next(data, bytes, &c, &e)) == 0) {
        ++n; at = 0;
        while (e.name_bytes - at >= 2 && e.name[at] == '.' && e.name[at + 1] == '/') at += 2;
        if (e.name_bytes - at == 4 && !memcmp(e.name + at, "init", 4))
            init = (e.mode & MULIB_CPIO_TYPE_MASK) == MULIB_CPIO_REGULAR && (e.mode & 0111) != 0;
    }
    if (rc != MULIB_INITSYSUTILS_END) return rc;
    *entries = n; *has_init = init; return 0;
}
int mulib_initsysutils_run(const mulib_initsysutils_step_t *steps, size_t count,
                          size_t *failed_index, int *first_undo_error) {
    size_t i; int rc, undo;
    if (!failed_index || !first_undo_error || (!steps && count)) return MULIB_INITSYSUTILS_INVALID;
    *failed_index = SIZE_MAX; *first_undo_error = 0;
    for (i = 0; i < count; ++i) if (!steps[i].apply || !steps[i].undo) return MULIB_INITSYSUTILS_INVALID;
    for (i = 0; i < count; ++i) {
        rc = steps[i].apply(steps[i].context);
        if (rc) {
            *failed_index = i;
            while (i) { --i; undo = steps[i].undo(steps[i].context); if (undo && !*first_undo_error) *first_undo_error = undo; }
            return rc;
        }
    }
    return 0;
}
#endif

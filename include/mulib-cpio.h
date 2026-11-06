/** @file mulib-cpio.h
 * @brief Bounded newc/crc CPIO reading and deterministic archive construction.
 * Define MULIB_IMPL_CPIO once. Caller-owned buffers; no filesystem access.
 */
#ifndef MULIB_CPIO_H
#define MULIB_CPIO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_CPIO_OK = 0, MULIB_CPIO_TRAILER = 1, MULIB_CPIO_INVALID = -1,
       MULIB_CPIO_FORMAT = -2, MULIB_CPIO_BOUNDS = -3, MULIB_CPIO_CHECKSUM = -4,
       MULIB_CPIO_UNSUPPORTED = -5, MULIB_CPIO_FINISHED = -6 };
enum { MULIB_CPIO_NEWC = 1, MULIB_CPIO_CRC = 2 };
enum { MULIB_CPIO_TYPE_MASK = 0170000, MULIB_CPIO_REGULAR = 0100000,
       MULIB_CPIO_DIRECTORY = 0040000, MULIB_CPIO_SYMLINK = 0120000,
       MULIB_CPIO_CHAR = 0020000, MULIB_CPIO_BLOCK = 0060000,
       MULIB_CPIO_FIFO = 0010000, MULIB_CPIO_SOCKET = 0140000 };
/** Names exclude the terminating NUL in name_bytes. Parsed views borrow input.
 * All metadata is preserved, including hardlink identity (dev_major/minor, ino).
 * checksum/name_storage are read-only metadata; the writer computes them. */
typedef struct {
    const char *name; size_t name_bytes, name_storage;
    const void *data; size_t size;
    uint32_t ino, mode, uid, gid, nlink, mtime;
    uint32_t dev_major, dev_minor, rdev_major, rdev_minor, checksum;
    unsigned format;
} mulib_cpio_entry_t;
typedef struct { uint8_t *data; size_t capacity, size; unsigned format, finished; } mulib_cpio_writer_t;
/** @brief Decode one record at *cursor (initially zero). Advances on record or
 * trailer only; output unchanged on error. Requires 4-byte relative alignment.
 * Checks hex fields, NUL padding, lengths and crc byte-sum. EOF without a header
 * is BOUNDS, not a trailer. Old binary/odc formats return UNSUPPORTED. */
int mulib_cpio_next(const void *archive, size_t bytes, size_t *cursor, mulib_cpio_entry_t *out);
/** @brief Compute the crc-format unsigned byte sum modulo 2^32 (not CRC32).
 * data must point to bytes readable bytes, or be NULL when bytes is zero. */
uint32_t mulib_cpio_sum(const void *data, size_t bytes);
/** @brief Initialize a caller-owned buffer; NULL is permitted for zero capacity. */
int mulib_cpio_writer_init(mulib_cpio_writer_t *writer, void *data, size_t capacity, unsigned format);
/** @brief Size a single newc/crc record including padding. 0 or negative error. */
int mulib_cpio_record_size(size_t name_bytes, size_t data_bytes, size_t *out);
/** @brief Append record atomically. Input must not overlap writer storage.
 * TRAILER!!! is reserved for finish(). No filesystem policy is imposed on names. */
int mulib_cpio_add(mulib_cpio_writer_t *writer, const mulib_cpio_entry_t *entry);
/** @brief Append TRAILER!!! and zero padding to a power-of-two block size >=4.
 * Use 512 for conventional cpio files, 4 for initramfs. Atomic; no later adds. */
int mulib_cpio_finish(mulib_cpio_writer_t *writer, size_t block_size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CPIO) && !defined(MULIB_CPIO_IMPLEMENTED)
#define MULIB_CPIO_IMPLEMENTED
#include <string.h>
static int mulib_cpio_hex_(const uint8_t *p, uint32_t *out) {
    uint32_t v = 0; unsigned i, d;
    for (i = 0; i < 8; ++i) {
        d = p[i] >= '0' && p[i] <= '9' ? p[i] - '0' :
            p[i] >= 'a' && p[i] <= 'f' ? p[i] - 'a' + 10 :
            p[i] >= 'A' && p[i] <= 'F' ? p[i] - 'A' + 10 : 16;
        if (d == 16) return MULIB_CPIO_FORMAT;
        v = (v << 4) | d;
    }
    *out = v; return 0;
}
uint32_t mulib_cpio_sum(const void *data, size_t bytes) {
    const uint8_t *p = (const uint8_t *)data; uint32_t sum = 0; size_t i;
    for (i = 0; i < bytes; ++i) sum += p[i];
    return sum;
}
int mulib_cpio_record_size(size_t name_bytes, size_t data_bytes, size_t *out) {
    size_t n, d;
    if (!out || !name_bytes || name_bytes >= UINT32_MAX || data_bytes > UINT32_MAX ||
        name_bytes > SIZE_MAX - 114 || data_bytes > SIZE_MAX - 3) return MULIB_CPIO_INVALID;
    n = (110 + name_bytes + 1 + 3) & ~(size_t)3;
    d = (data_bytes + 3) & ~(size_t)3;
    if (d > SIZE_MAX - n) return MULIB_CPIO_INVALID;
    *out = n + d; return 0;
}
int mulib_cpio_next(const void *archive, size_t bytes, size_t *cursor, mulib_cpio_entry_t *out) {
    const uint8_t *p = (const uint8_t *)archive, *name; uint32_t f[13];
    size_t at, i, name_len, data_at, next; mulib_cpio_entry_t v;
    if (!p || !cursor || !out || (*cursor & 3) || *cursor > bytes) return MULIB_CPIO_INVALID;
    at = *cursor;
    if (bytes - at < 6) return MULIB_CPIO_BOUNDS;
    if (!memcmp(p + at, "070701", 6)) v.format = MULIB_CPIO_NEWC;
    else if (!memcmp(p + at, "070702", 6)) v.format = MULIB_CPIO_CRC;
    else if (!memcmp(p + at, "070707", 6) ||
             (p[at] == 0xc7 && p[at + 1] == 0x71) || (p[at] == 0x71 && p[at + 1] == 0xc7)) return MULIB_CPIO_UNSUPPORTED;
    else return MULIB_CPIO_FORMAT;
    if (bytes - at < 110) return MULIB_CPIO_BOUNDS;
    for (i = 0; i < 13; ++i) if (mulib_cpio_hex_(p + at + 6 + i * 8, f + i)) return MULIB_CPIO_FORMAT;
    at += 110;
    if (!f[11]) return MULIB_CPIO_FORMAT;
    if (f[11] > bytes - at) return MULIB_CPIO_BOUNDS;
    name = p + at; name_len = 0;
    while (name_len < f[11] && name[name_len]) ++name_len;
    if (!name_len || name_len == f[11]) return MULIB_CPIO_FORMAT;
    for (i = name_len; i < f[11]; ++i) if (name[i]) return MULIB_CPIO_FORMAT;
    at += f[11];
    if (at > SIZE_MAX - 3) return MULIB_CPIO_BOUNDS;
    data_at = (at + 3) & ~(size_t)3;
    if (data_at > bytes || f[6] > bytes - data_at) return MULIB_CPIO_BOUNDS;
    next = data_at + f[6];
    if (next > SIZE_MAX - 3) return MULIB_CPIO_BOUNDS;
    next = (next + 3) & ~(size_t)3;
    if (next > bytes) return MULIB_CPIO_BOUNDS;
    if (v.format == MULIB_CPIO_CRC && mulib_cpio_sum(p + data_at, f[6]) != f[12]) return MULIB_CPIO_CHECKSUM;
    v.name = (const char *)name; v.name_bytes = name_len; v.name_storage = f[11]; v.data = p + data_at; v.size = f[6];
    v.ino = f[0]; v.mode = f[1]; v.uid = f[2]; v.gid = f[3]; v.nlink = f[4]; v.mtime = f[5];
    v.dev_major = f[7]; v.dev_minor = f[8]; v.rdev_major = f[9]; v.rdev_minor = f[10]; v.checksum = f[12];
    if (name_len == 10 && !memcmp(name, "TRAILER!!!", 10)) {
        if (v.size) return MULIB_CPIO_FORMAT;
        *out = v; *cursor = next; return MULIB_CPIO_TRAILER;
    }
    *out = v; *cursor = next; return 0;
}
int mulib_cpio_writer_init(mulib_cpio_writer_t *w, void *data, size_t capacity, unsigned format) {
    if (!w || (!data && capacity) || (format != MULIB_CPIO_NEWC && format != MULIB_CPIO_CRC)) return MULIB_CPIO_INVALID;
    w->data = (uint8_t *)data; w->capacity = capacity; w->size = 0; w->format = format; w->finished = 0; return 0;
}
static void mulib_cpio_put_(uint8_t *p, uint32_t v) {
    static const char hex[] = "0123456789abcdef"; unsigned i;
    for (i = 0; i < 8; ++i) p[i] = (uint8_t)hex[(v >> (28 - 4 * i)) & 15];
}
static int mulib_cpio_add_(mulib_cpio_writer_t *w, const mulib_cpio_entry_t *e, int trailer) {
    size_t n, i, offset; uint32_t f[13]; uint8_t *p;
    if (!w || !e || !e->name || (!e->data && e->size) || w->size > w->capacity ||
        (!w->data && w->capacity) || (w->size & 3) ||
        (w->format != MULIB_CPIO_NEWC && w->format != MULIB_CPIO_CRC)) return MULIB_CPIO_INVALID;
    if (w->finished) return MULIB_CPIO_FINISHED;
    if (mulib_cpio_record_size(e->name_bytes, e->size, &n)) return MULIB_CPIO_INVALID;
    for (i = 0; i < e->name_bytes; ++i) if (!e->name[i]) return MULIB_CPIO_FORMAT;
    if (!trailer && e->name_bytes == 10 && !memcmp(e->name, "TRAILER!!!", 10)) return MULIB_CPIO_INVALID;
    if (n > w->capacity - w->size) return MULIB_CPIO_BOUNDS;
    f[0] = e->ino; f[1] = e->mode; f[2] = e->uid; f[3] = e->gid; f[4] = e->nlink; f[5] = e->mtime;
    f[6] = (uint32_t)e->size; f[7] = e->dev_major; f[8] = e->dev_minor;
    f[9] = e->rdev_major; f[10] = e->rdev_minor; f[11] = (uint32_t)e->name_bytes + 1;
    f[12] = w->format == MULIB_CPIO_CRC ? mulib_cpio_sum(e->data, e->size) : 0;
    p = w->data + w->size; memset(p, 0, n); memcpy(p, w->format == MULIB_CPIO_CRC ? "070702" : "070701", 6);
    for (i = 0; i < 13; ++i) mulib_cpio_put_(p + 6 + i * 8, f[i]);
    memcpy(p + 110, e->name, e->name_bytes); offset = (110 + e->name_bytes + 4) & ~(size_t)3;
    if (e->size) memcpy(p + offset, e->data, e->size);
    w->size += n; return 0;
}
int mulib_cpio_add(mulib_cpio_writer_t *w, const mulib_cpio_entry_t *e) { return mulib_cpio_add_(w, e, 0); }
int mulib_cpio_finish(mulib_cpio_writer_t *w, size_t block) {
    mulib_cpio_entry_t e; size_t n, end; int rc;
    if (!w || block < 4 || (block & (block - 1)) || w->size > w->capacity) return MULIB_CPIO_INVALID;
    if (w->finished) return MULIB_CPIO_FINISHED;
    if (mulib_cpio_record_size(10, 0, &n) || n > SIZE_MAX - w->size ||
        w->size + n > SIZE_MAX - (block - 1)) return MULIB_CPIO_INVALID;
    end = (w->size + n + block - 1) & ~(block - 1);
    if (end > w->capacity) return MULIB_CPIO_BOUNDS;
    memset(&e, 0, sizeof(e)); e.name = "TRAILER!!!"; e.name_bytes = 10; e.nlink = 1;
    rc = mulib_cpio_add_(w, &e, 1); if (rc) return rc;
    memset(w->data + w->size, 0, end - w->size); w->size = end; w->finished = 1; return 0;
}
#endif

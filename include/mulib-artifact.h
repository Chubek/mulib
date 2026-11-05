/**
 * @file mulib-artifact.h
 * @brief Immutable, reference-counted values, references and application objects.
 * Define MULIB_IMPL_ARTIFACT once. No build-system or filesystem assumptions.
 */
#ifndef MULIB_ARTIFACT_H
#define MULIB_ARTIFACT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_artifact mulib_artifact_t;
typedef enum mulib_artifact_kind {
  MULIB_ARTIFACT_BYTES, MULIB_ARTIFACT_REFERENCE, MULIB_ARTIFACT_OBJECT
} mulib_artifact_kind_t;
/** Optional object destructor, called exactly once after the last release. */
typedef void (*mulib_artifact_destroy_fn) (void *object);
typedef struct mulib_artifact_desc {
  const char *id;                 /**< Required nonempty application identity. */
  const char *type;               /**< Required nonempty application type. */
  const char *media_type;         /**< Optional MIME type; no interpretation. */
  const char *locator;            /**< Optional URI; required for REFERENCE. */
  mulib_artifact_kind_t kind;
  const void *data;               /**< BYTES only; copied, including embedded NULs. */
  size_t length;
  void *object;                  /**< OBJECT only; borrowed if destroy is NULL. */
  mulib_artifact_destroy_fn destroy;
} mulib_artifact_desc_t;
/** Copy strings/bytes. Adopt an object only on success. NULL on invalid/OOM. */
mulib_artifact_t *mulib_artifact_create (const mulib_artifact_desc_t *desc);
/** Add an atomic reference; NULL on NULL or reference-count overflow. */
mulib_artifact_t *mulib_artifact_retain (mulib_artifact_t *artifact);
/** Release one atomic reference; NULL safe. Each caller must own a reference. */
void mulib_artifact_release (mulib_artifact_t *artifact);
/** Borrow the immutable descriptor until the last release. NULL safe. */
const mulib_artifact_desc_t *mulib_artifact_view (const mulib_artifact_t *artifact);
typedef enum mulib_artifact_file_kind {
  MULIB_ARTIFACT_FILE, MULIB_ARTIFACT_DIFF
} mulib_artifact_file_kind_t;
/** A binary file/diff and portable metadata. Path is a canonical relative slash
 * path; mode contains permission bits only (0..0777); mtime is Unix seconds.
 * DIFF payload format is application-defined, e.g. VCDIFF. */
typedef struct mulib_artifact_file {
  const char *path;
  const void *data;
  size_t length;
  uint64_t mtime;
  unsigned mode;
  mulib_artifact_file_kind_t kind;
} mulib_artifact_file_t;
/** Copy 0..65536 files into a deterministic versioned binary archive. The TOC
 * gives absolute offsets and lengths for each name and payload. NULL on invalid
 * paths, duplicates, overflow or allocation failure. No host filesystem I/O. */
mulib_artifact_t *mulib_artifact_pack (const char *id, const mulib_artifact_file_t *, size_t count);
/** Validate/copy an archive, rejecting malformed offsets, metadata and names.
 * id is application identity, not an implicit content hash. */
mulib_artifact_t *mulib_artifact_unpack (const char *id, const void *, size_t length);
/** Validate an archive BYTES artifact and return its entry count; 0 success,
 * -1 invalid. Count changes only on success. */
int mulib_artifact_file_count (const mulib_artifact_t *, size_t *count);
/** Borrow one file's bytes/path and copy its metadata, until artifact release.
 * 0 success, -1 invalid/index out of range. out changes only on success. */
int mulib_artifact_file_at (const mulib_artifact_t *, size_t index, mulib_artifact_file_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ARTIFACT) && !defined(MULIB_ARTIFACT_IMPLEMENTED)
#define MULIB_ARTIFACT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
struct mulib_artifact {
  mulib_artifact_desc_t value;
  atomic_size_t references;
};
static char *
mulib_artifact_copy_ (const char *s)
{
  size_t n;
  char *p;
  if (!s) return NULL;
  n = strlen (s);
  if (n == SIZE_MAX) return NULL;
  p = (char *)malloc (n + 1);
  if (p) memcpy (p, s, n + 1);
  return p;
}
mulib_artifact_t *
mulib_artifact_create (const mulib_artifact_desc_t *d)
{
  mulib_artifact_t *a;
  if (!d || !d->id || !*d->id || !d->type || !*d->type
      || d->kind < MULIB_ARTIFACT_BYTES || d->kind > MULIB_ARTIFACT_OBJECT
      || (!d->data && d->length) || d->length > (size_t)PTRDIFF_MAX
      || (d->kind != MULIB_ARTIFACT_BYTES && (d->data || d->length))
      || (d->kind != MULIB_ARTIFACT_OBJECT && (d->object || d->destroy))
      || (d->kind == MULIB_ARTIFACT_OBJECT && !d->object)
      || (d->kind == MULIB_ARTIFACT_REFERENCE && (!d->locator || !*d->locator)))
    return NULL;
  a = (mulib_artifact_t *)calloc (1, sizeof (*a));
  if (!a) return NULL;
  atomic_init (&a->references, 1);
  a->value.kind = d->kind;
  a->value.id = mulib_artifact_copy_ (d->id);
  a->value.type = mulib_artifact_copy_ (d->type);
  a->value.media_type = mulib_artifact_copy_ (d->media_type);
  a->value.locator = mulib_artifact_copy_ (d->locator);
  if (d->length) {
    void *bytes = malloc (d->length);
    if (bytes) memcpy (bytes, d->data, d->length);
    a->value.data = bytes;
  }
  if (!a->value.id || !a->value.type || (d->media_type && !a->value.media_type)
      || (d->locator && !a->value.locator) || (d->length && !a->value.data)) {
    mulib_artifact_release (a);
    return NULL;
  }
  a->value.length = d->length;
  a->value.object = d->object;
  a->value.destroy = d->destroy;
  return a;
}
mulib_artifact_t *
mulib_artifact_retain (mulib_artifact_t *a)
{
  size_t n;
  if (!a) return NULL;
  n = atomic_load (&a->references);
  do { if (n == SIZE_MAX) return NULL; }
  while (!atomic_compare_exchange_weak (&a->references, &n, n + 1));
  return a;
}
void
mulib_artifact_release (mulib_artifact_t *a)
{
  if (!a || atomic_fetch_sub (&a->references, 1) != 1) return;
  if (a->value.destroy) a->value.destroy (a->value.object);
  free ((void *)a->value.id);
  free ((void *)a->value.type);
  free ((void *)a->value.media_type);
  free ((void *)a->value.locator);
  free ((void *)a->value.data);
  free (a);
}
const mulib_artifact_desc_t *
mulib_artifact_view (const mulib_artifact_t *a)
{
  return a ? &a->value : NULL;
}
static void mulib_artifact_put64_ (unsigned char *p, uint64_t n)
{ unsigned i; for (i = 0; i < 8; ++i) { p[i] = (unsigned char)n; n >>= 8; } }
static uint64_t mulib_artifact_get64_ (const unsigned char *p)
{ uint64_t n = 0; unsigned i; for (i = 0; i < 8; ++i) n |= (uint64_t)p[i] << (i * 8); return n; }
static int mulib_artifact_path_ (const char *s, size_t n)
{
  size_t i, start = 0;
  if (!n || n > 4095) return 0;
  for (i = 0; i <= n; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (i < n && (!c || c < 32 || c == 127 || c == '\\' || c == ':')) return 0;
    if (i == n || c == '/') {
      size_t len = i - start;
      if (!len || (len == 1 && s[start] == '.') || (len == 2 && s[start] == '.' && s[start+1] == '.')) return 0;
      start = i + 1;
    }
  }
  return 1;
}
/* Canonical layout: header, fixed 48-byte TOC, then (name NUL,payload) per
 * entry. Canonical offsets disallow overlap, hidden bytes and trailing data. */
static int mulib_artifact_archive_ (const void *data, size_t length, size_t *count)
{
  const unsigned char *p = (const unsigned char *)data;
  size_t n, i, j, pos;
  if (!p || length < 24 || memcmp (p, "MUART\0\1\0", 8)
      || mulib_artifact_get64_ (p + 16) != length || mulib_artifact_get64_ (p + 8) > 65536) return -1;
  n = (size_t)mulib_artifact_get64_ (p + 8);
  if (n > (length - 24) / 48) return -1;
  pos = 24 + n * 48;
  for (i = 0; i < n; ++i) {
    const unsigned char *e = p + 24 + i * 48;
    uint64_t nl = mulib_artifact_get64_ (e + 8), dl = mulib_artifact_get64_ (e + 24);
    uint64_t flags = mulib_artifact_get64_ (e + 40);
    if (mulib_artifact_get64_ (e) != pos || nl > 4095 || nl >= length - pos
        || p[pos + (size_t)nl] || !mulib_artifact_path_ ((const char *)p + pos, (size_t)nl)
        || (flags & ~UINT64_C(1023))) return -1;
    for (j = 0; j < i; ++j) {
      size_t offset = (size_t)mulib_artifact_get64_ (p + 24 + j * 48);
      if (!strcmp ((const char *)p + offset, (const char *)p + pos)) return -1;
    }
    pos += (size_t)nl + 1;
    if (mulib_artifact_get64_ (e + 16) != pos || dl > length - pos) return -1;
    pos += (size_t)dl;
  }
  if (pos != length) return -1;
  *count = n; return 0;
}
mulib_artifact_t *mulib_artifact_unpack (const char *id, const void *data, size_t length)
{
  mulib_artifact_desc_t d = {0};
  size_t count;
  if (mulib_artifact_archive_ (data, length, &count)) return NULL;
  d.id = id; d.type = "mulib.archive.v1"; d.media_type = "application/x-mulib-artifact";
  d.data = data; d.length = length;
  return mulib_artifact_create (&d);
}
mulib_artifact_t *mulib_artifact_pack (const char *id, const mulib_artifact_file_t *files, size_t n)
{
  size_t i, j, total, pos;
  unsigned char *p;
  mulib_artifact_t *a;
  if ((!files && n) || n > 65536) return NULL;
  total = 24 + n * 48;
  for (i = 0; i < n; ++i) {
    size_t len;
    if (!files[i].path || (!files[i].data && files[i].length) || files[i].mode > 0777
        || files[i].kind < MULIB_ARTIFACT_FILE || files[i].kind > MULIB_ARTIFACT_DIFF) return NULL;
    len = strlen (files[i].path);
    if (!mulib_artifact_path_ (files[i].path, len) || len + 1 > SIZE_MAX - total) return NULL;
    total += len + 1;
    if (files[i].length > SIZE_MAX - total) return NULL;
    total += files[i].length;
    for (j = 0; j < i; ++j) if (!strcmp (files[i].path, files[j].path)) return NULL;
  }
  if (total > (size_t)PTRDIFF_MAX) return NULL;
  p = (unsigned char *)calloc (total, 1);
  if (!p) return NULL;
  memcpy (p, "MUART\0\1\0", 8); mulib_artifact_put64_ (p + 8, n); mulib_artifact_put64_ (p + 16, total);
  pos = 24 + n * 48;
  for (i = 0; i < n; ++i) {
    unsigned char *e = p + 24 + i * 48;
    size_t len = strlen (files[i].path);
    mulib_artifact_put64_ (e, pos); mulib_artifact_put64_ (e + 8, len);
    memcpy (p + pos, files[i].path, len + 1); pos += len + 1;
    mulib_artifact_put64_ (e + 16, pos); mulib_artifact_put64_ (e + 24, files[i].length);
    mulib_artifact_put64_ (e + 32, files[i].mtime);
    mulib_artifact_put64_ (e + 40, files[i].mode | ((uint64_t)files[i].kind << 9));
    if (files[i].length) memcpy (p + pos, files[i].data, files[i].length);
    pos += files[i].length;
  }
  a = mulib_artifact_unpack (id, p, total); free (p); return a;
}
int mulib_artifact_file_count (const mulib_artifact_t *a, size_t *n)
{
  if (!a || !n || a->value.kind != MULIB_ARTIFACT_BYTES) return -1;
  return mulib_artifact_archive_ (a->value.data, a->value.length, n);
}
int mulib_artifact_file_at (const mulib_artifact_t *a, size_t index, mulib_artifact_file_t *out)
{
  size_t n;
  const unsigned char *p, *e;
  uint64_t flags;
  mulib_artifact_file_t f;
  if (!out || mulib_artifact_file_count (a, &n) || index >= n) return -1;
  p = (const unsigned char *)a->value.data; e = p + 24 + index * 48;
  f.path = (const char *)p + (size_t)mulib_artifact_get64_ (e);
  f.data = p + (size_t)mulib_artifact_get64_ (e + 16);
  f.length = (size_t)mulib_artifact_get64_ (e + 24); f.mtime = mulib_artifact_get64_ (e + 32);
  flags = mulib_artifact_get64_ (e + 40); f.mode = (unsigned)(flags & 0777);
  f.kind = (mulib_artifact_file_kind_t)(flags >> 9); *out = f; return 0;
}
#endif

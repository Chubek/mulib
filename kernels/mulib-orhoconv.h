/** @file mulib-orhoconv.h
 * @brief Configurable UTF-8 orthographic boundary conventions backed by KVDB.
 * Spelling retained as requested. Define MULIB_IMPL_ORHOCONV, UTF8, KVDB,
 * BUFFER, ENDIAN, CRC32, VARINT. No implicit normalization or case folding.
 */
#ifndef MULIB_ORHOCONV_H
#define MULIB_ORHOCONV_H
#include "mulib-kvdb.h"
#include "mulib-utf8.h"
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_orhoconv mulib_orhoconv_t;
typedef enum mulib_orhoconv_token { MULIB_ORHOCONV_ONSET, MULIB_ORHOCONV_PREFIX, MULIB_ORHOCONV_SUFFIX } mulib_orhoconv_token_t;
/** Create a convention set with explicit scalar-count minima and ASCII vowels.
 * Zero minima are allowed. All mutators return KVDB result codes. */
mulib_orhoconv_t *mulib_orhoconv_create(size_t left_min, size_t right_min);
void mulib_orhoconv_destroy(mulib_orhoconv_t *);
int mulib_orhoconv_set_minima(mulib_orhoconv_t *, size_t, size_t);
int mulib_orhoconv_set_vowels(mulib_orhoconv_t *, const char *utf8);
int mulib_orhoconv_add_token(mulib_orhoconv_t *, mulib_orhoconv_token_t, const char *);
/** Forbid the given UTF-8 byte boundary in every occurrence of a literal group. */
int mulib_orhoconv_forbid(mulib_orhoconv_t *, const char *group, size_t byte_boundary);
int mulib_orhoconv_is_vowel(const mulib_orhoconv_t *, uint32_t);
int mulib_orhoconv_has_token(const mulib_orhoconv_t *, mulib_orhoconv_token_t, const char *, size_t);
/** Borrow the portable rules store, read-only. Keys: V vowels, L minima,
 * O/P/S + token, F + group with a LE64 forbidden byte offset. */
const mulib_kvdb_t *mulib_orhoconv_database(const mulib_orhoconv_t *);
/** Filter a candidate byte mask of length word_bytes+1, atomically in-place.
 * Reject malformed UTF-8, NUL, words >4096 bytes. Endpoints and UTF-8 interiors
 * are always cleared. Also excludes splits before common combining marks,
 * variation selectors and adjacent to ZWJ (not full grapheme segmentation). */
int mulib_orhoconv_filter(const mulib_orhoconv_t *, const char *, size_t, unsigned char *mask);
int mulib_orhoconv_serialize(const mulib_orhoconv_t *, mulib_buffer_t *);
int mulib_orhoconv_deserialize(const void *, size_t, mulib_orhoconv_t **);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ORHOCONV) && !defined(MULIB_ORHOCONV_IMPLEMENTED)
#define MULIB_ORHOCONV_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_orhoconv { mulib_kvdb_t *db; };
static int mulib_orhoconv_store_(mulib_orhoconv_t *o, const void *k, size_t n, const void *v, size_t vn) {
  const void *old; size_t old_n;
  if (mulib_kvdb_size(o->db) >= 4096 && mulib_kvdb_get(o->db, k, n, &old, &old_n)) return MULIB_KVDB_EOVERFLOW;
  return mulib_kvdb_set(o->db, k, n, v, vn);
}
static int mulib_orhoconv_text_(const char *s, size_t n) {
  return s && n <= 4096 && !memchr(s, 0, n) && mulib_utf8_validate((const unsigned char *)s, n);
}
void mulib_orhoconv_destroy(mulib_orhoconv_t *o) { if (o) { mulib_kvdb_close(o->db); free(o); } }
int mulib_orhoconv_set_minima(mulib_orhoconv_t *o, size_t l, size_t r) {
  unsigned char b[16];
  if (!o || l > 4096 || r > 4096) return MULIB_KVDB_EINVAL;
  mulib_endian_store_le64(b, (uint64_t)l); mulib_endian_store_le64(b + 8, (uint64_t)r);
  return mulib_orhoconv_store_(o, "L", 1, b, 16);
}
int mulib_orhoconv_set_vowels(mulib_orhoconv_t *o, const char *s) {
  if (!o || !s || strlen(s) > 256 || !mulib_orhoconv_text_(s, strlen(s))) return MULIB_KVDB_EINVAL;
  return mulib_orhoconv_store_(o, "V", 1, s, strlen(s));
}
mulib_orhoconv_t *mulib_orhoconv_create(size_t l, size_t r) {
  mulib_orhoconv_t *o = (mulib_orhoconv_t *)calloc(1, sizeof *o);
  if (!o) return NULL;
  o->db = mulib_kvdb_create();
  if (!o->db || mulib_orhoconv_set_minima(o, l, r) || mulib_orhoconv_set_vowels(o, "aeiouyAEIOUY")) {
    mulib_orhoconv_destroy(o); return NULL;
  }
  return o;
}
static unsigned char mulib_orhoconv_kind_(mulib_orhoconv_token_t t) {
  return t == MULIB_ORHOCONV_ONSET ? 'O' : t == MULIB_ORHOCONV_PREFIX ? 'P' : 'S';
}
int mulib_orhoconv_add_token(mulib_orhoconv_t *o, mulib_orhoconv_token_t t, const char *s) {
  unsigned char key[4097]; size_t n;
  if (!o || !s || t < MULIB_ORHOCONV_ONSET || t > MULIB_ORHOCONV_SUFFIX) return MULIB_KVDB_EINVAL;
  n = strlen(s); if (!n || !mulib_orhoconv_text_(s, n)) return MULIB_KVDB_EINVAL;
  key[0] = mulib_orhoconv_kind_(t); memcpy(key + 1, s, n);
  return mulib_orhoconv_store_(o, key, n + 1, NULL, 0);
}
int mulib_orhoconv_forbid(mulib_orhoconv_t *o, const char *s, size_t boundary) {
  unsigned char key[4097], value[8]; size_t n;
  if (!o || !s) return MULIB_KVDB_EINVAL;
  n = strlen(s);
  if (!mulib_orhoconv_text_(s, n) || !boundary || boundary >= n || ((unsigned char)s[boundary] & 0xc0) == 0x80)
    return MULIB_KVDB_EINVAL;
  key[0] = 'F'; memcpy(key + 1, s, n); mulib_endian_store_le64(value, (uint64_t)boundary);
  return mulib_orhoconv_store_(o, key, n + 1, value, 8);
}
const mulib_kvdb_t *mulib_orhoconv_database(const mulib_orhoconv_t *o) { return o ? o->db : NULL; }
int mulib_orhoconv_is_vowel(const mulib_orhoconv_t *o, uint32_t c) {
  const void *v; size_t n, i = 0, used; uint32_t cp;
  if (!o || mulib_kvdb_get(o->db, "V", 1, &v, &n)) return 0;
  while (i < n) {
    if (!mulib_utf8_decode((const unsigned char *)v + i, n - i, &cp, &used)) return 0;
    if (cp == c) return 1;
    i += used;
  }
  return 0;
}
int mulib_orhoconv_has_token(const mulib_orhoconv_t *o, mulib_orhoconv_token_t t, const char *s, size_t n) {
  unsigned char key[4097]; const void *v; size_t vn;
  if (!o || t < MULIB_ORHOCONV_ONSET || t > MULIB_ORHOCONV_SUFFIX || !mulib_orhoconv_text_(s, n)) return 0;
  key[0] = mulib_orhoconv_kind_(t); if (n) memcpy(key + 1, s, n);
  return mulib_kvdb_get(o->db, key, n + 1, &v, &vn) == MULIB_KVDB_OK;
}
static int mulib_orhoconv_combining_(uint32_t c) {
  return (c >= 0x300 && c <= 0x36f) || (c >= 0x1ab0 && c <= 0x1aff) ||
    (c >= 0x1dc0 && c <= 0x1dff) || (c >= 0xfe00 && c <= 0xfe0f) ||
    (c >= 0x20d0 && c <= 0x20ff) || (c >= 0xfe20 && c <= 0xfe2f) || (c >= 0xe0100 && c <= 0xe01ef);
}
int mulib_orhoconv_filter(const mulib_orhoconv_t *o, const char *s, size_t n, unsigned char *mask) {
  unsigned char *tmp;
  size_t offsets[4097], count = 0, i = 0, used, j, l, r, work = 0;
  uint32_t cps[4096]; const void *v; size_t vn;
  if (!o || !mask || !mulib_orhoconv_text_(s, n) || mulib_kvdb_get(o->db, "L", 1, &v, &vn) || vn != 16) return MULIB_KVDB_EINVAL;
  l = (size_t)mulib_endian_load_le64(v); r = (size_t)mulib_endian_load_le64((const unsigned char *)v + 8);
  while (i < n) {
    offsets[count] = i; mulib_utf8_decode((const unsigned char *)s + i, n - i, &cps[count], &used); ++count; i += used;
  }
  offsets[count] = n;
  tmp = (unsigned char *)calloc(n + 1, 1); if (!tmp) return MULIB_KVDB_ENOMEM;
  for (i = 1; i < count; ++i)
    if (i >= l && count - i >= r && !mulib_orhoconv_combining_(cps[i]) && cps[i] != 0x200d && cps[i - 1] != 0x200d)
      tmp[offsets[i]] = mask[offsets[i]] != 0;
  for (i = 0; i < mulib_kvdb_size(o->db); ++i) {
    const void *k; size_t kn;
    mulib_kvdb_at(o->db, i, &k, &kn, &v, &vn);
    if (kn > 1 && *(const unsigned char *)k == 'F') {
      size_t boundary = (size_t)mulib_endian_load_le64(v);
      for (j = 0; kn - 1 <= n - j; ++j) {
        if (kn - 1 > 10000000u - work) { free(tmp); return MULIB_KVDB_EOVERFLOW; }
        work += kn - 1;
        if (!memcmp(s + j, (const unsigned char *)k + 1, kn - 1)) tmp[j + boundary] = 0;
      }
    }
  }
  memcpy(mask, tmp, n + 1); free(tmp); return MULIB_KVDB_OK;
}
int mulib_orhoconv_serialize(const mulib_orhoconv_t *o, mulib_buffer_t *b) {
  return o ? mulib_kvdb_serialize(o->db, b) : MULIB_KVDB_EINVAL;
}
int mulib_orhoconv_deserialize(const void *p, size_t n, mulib_orhoconv_t **out) {
  mulib_kvdb_t *db; mulib_orhoconv_t *o; size_t i; int rc, have_l = 0, have_v = 0;
  if (!out) return MULIB_KVDB_EINVAL;
  rc = mulib_kvdb_deserialize(p, n, &db); if (rc) return rc;
  if (mulib_kvdb_size(db) > 4096) goto corrupt;
  for (i = 0; i < mulib_kvdb_size(db); ++i) {
    const void *k, *v; size_t kn, vn; unsigned char kind;
    mulib_kvdb_at(db, i, &k, &kn, &v, &vn);
    if (!kn) goto corrupt;
    kind = *(const unsigned char *)k;
    if (kind == 'L' && kn == 1 && vn == 16 && mulib_endian_load_le64(v) <= 4096 &&
        mulib_endian_load_le64((const unsigned char *)v + 8) <= 4096) have_l = 1;
    else if (kind == 'V' && kn == 1 && vn <= 256 && mulib_orhoconv_text_((const char *)v, vn)) have_v = 1;
    else if ((kind == 'F' || kind == 'P' || kind == 'S' || kind == 'O') && kn > 1 &&
        mulib_orhoconv_text_((const char *)k + 1, kn - 1)) {
      if (kind == 'F') {
        uint64_t b;
        if (vn != 8) goto corrupt;
        b = mulib_endian_load_le64(v);
        if (!b || b >= kn - 1 || (((const unsigned char *)k)[1 + (size_t)b] & 0xc0) == 0x80) goto corrupt;
      } else if (vn) goto corrupt;
    } else goto corrupt;
  }
  if (!have_l || !have_v) goto corrupt;
  o = (mulib_orhoconv_t *)malloc(sizeof *o); if (!o) { mulib_kvdb_close(db); return MULIB_KVDB_ENOMEM; }
  o->db = db; *out = o; return MULIB_KVDB_OK;
corrupt:
  mulib_kvdb_close(db); return MULIB_KVDB_ECORRUPT;
}
#ifdef __cplusplus
}
#endif
#endif

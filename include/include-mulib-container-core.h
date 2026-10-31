/**
 * @file include-mulib-container-core.h
 * @brief Shared checked storage, wire primitives and errors for containers.
 * All functions are static inline; no implementation switch is necessary.
 * Define MULIB_CT_MALLOC/REALLOC/FREE together to inject an allocator. Objects
 * must be released by the same allocator configuration that created them.
 */
#ifndef INCLUDE_MULIB_CONTAINER_CORE_H
#define INCLUDE_MULIB_CONTAINER_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef MULIB_CT_MALLOC
#define MULIB_CT_MALLOC malloc
#endif
#ifndef MULIB_CT_REALLOC
#define MULIB_CT_REALLOC realloc
#endif
#ifndef MULIB_CT_FREE
#define MULIB_CT_FREE free
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Common results. AGAIN and DONE are successful control-flow results. */
typedef enum mulib_ct_result
{
  MULIB_CT_OK = 0, MULIB_CT_AGAIN = 1, MULIB_CT_DONE = 2,
  MULIB_CT_EINVAL = -1, MULIB_CT_ENOMEM = -2, MULIB_CT_ELIMIT = -3,
  MULIB_CT_EFORMAT = -4, MULIB_CT_ESTATE = -5, MULIB_CT_ENOENT = -6,
  MULIB_CT_EEXIST = -7, MULIB_CT_ENOTSUP = -8, MULIB_CT_EIO = -9,
  MULIB_CT_EACCESS = -10, MULIB_CT_EBUSY = -11, MULIB_CT_ESTALE = -12,
  MULIB_CT_ECANCELED = -13, MULIB_CT_ECHECKSUM = -14
} mulib_ct_result;
enum { MULIB_CT_ID_CAP = 64, MULIB_CT_TEXT_CAP = 4096 };
/** @brief Owned byte array. Initialize to {0}; do not shallow-copy live blobs. */
typedef struct mulib_ct_blob
{
  unsigned char *data;
  size_t length, capacity;
} mulib_ct_blob;
/** @brief Static explanation; returned storage is never owned by the caller. */
static inline const char *
mulib_ct_strerror (int result)
{
  switch (result)
    {
    case MULIB_CT_OK: return "success";
    case MULIB_CT_AGAIN: return "more work required";
    case MULIB_CT_DONE: return "complete";
    case MULIB_CT_EINVAL: return "invalid argument";
    case MULIB_CT_ENOMEM: return "allocation failed";
    case MULIB_CT_ELIMIT: return "resource limit";
    case MULIB_CT_EFORMAT: return "malformed input";
    case MULIB_CT_ESTATE: return "invalid lifecycle state";
    case MULIB_CT_ENOENT: return "not found";
    case MULIB_CT_EEXIST: return "already exists";
    case MULIB_CT_ENOTSUP: return "unsupported operation";
    case MULIB_CT_EIO: return "backend failure";
    case MULIB_CT_EACCESS: return "access denied";
    case MULIB_CT_EBUSY: return "resource busy";
    case MULIB_CT_ESTALE: return "stale generation";
    case MULIB_CT_ECANCELED: return "canceled";
    case MULIB_CT_ECHECKSUM: return "checksum mismatch";
    default: return "unknown result";
    }
}
/** @brief Checked size addition; output is unchanged on overflow. */
static inline int
mulib_ct_add_size (size_t a, size_t b, size_t *out)
{
  if (!out || b > SIZE_MAX - a) return MULIB_CT_ELIMIT;
  *out = a + b;
  return 0;
}
/** @brief Bounded C string length, excluding NUL. */
static inline int
mulib_ct_text_length (const char *text, size_t maximum, size_t *out)
{
  size_t n;
  if (!text || !out) return MULIB_CT_EINVAL;
  for (n = 0; n <= maximum; ++n)
    {
      if (!text[n]) { *out = n; return 0; }
      if (n == maximum) break;
    }
  return MULIB_CT_ELIMIT;
}
/** @brief IDs contain 1..63 ASCII letters, digits, dots, underscores or dashes. */
static inline int
mulib_ct_id_valid (const char *id)
{
  size_t i, n;
  if (mulib_ct_text_length (id, MULIB_CT_ID_CAP - 1, &n) || !n) return 0;
  for (i = 0; i < n; ++i)
    if (!((id[i] >= 'a' && id[i] <= 'z')
          || (id[i] >= 'A' && id[i] <= 'Z')
          || (id[i] >= '0' && id[i] <= '9')
          || id[i] == '.' || id[i] == '_' || id[i] == '-')) return 0;
  return strcmp (id, ".") && strcmp (id, "..");
}
/** @brief Copy a bounded string; destination remains unchanged on failure. */
static inline int
mulib_ct_text_copy (char *out, size_t capacity, const char *text)
{
  size_t n;
  int rc;
  if (!out || !capacity) return MULIB_CT_EINVAL;
  rc = mulib_ct_text_length (text, capacity - 1, &n);
  if (rc) return rc;
  memmove (out, text, n + 1);
  return 0;
}
/** @brief Release/reset an owned blob; NULL and repeated frees are allowed. */
static inline void
mulib_ct_blob_free (mulib_ct_blob *blob)
{
  if (blob) { MULIB_CT_FREE (blob->data); memset (blob, 0, sizeof (*blob)); }
}
/** @brief Reserve exact capacity bounded by limit; length is unchanged. */
static inline int
mulib_ct_blob_reserve (mulib_ct_blob *blob, size_t capacity, size_t limit)
{
  void *next;
  if (!blob || blob->length > blob->capacity || (!blob->data && blob->capacity)) return MULIB_CT_EINVAL;
  if (capacity > limit) return MULIB_CT_ELIMIT;
  if (capacity <= blob->capacity) return 0;
  next = MULIB_CT_REALLOC (blob->data, capacity);
  if (!next) return MULIB_CT_ENOMEM;
  blob->data = (unsigned char *)next;
  blob->capacity = capacity;
  return 0;
}
/** @brief Replace bytes atomically. Source may alias the old blob. */
static inline int
mulib_ct_blob_assign (mulib_ct_blob *blob, const void *data, size_t n,
                      size_t limit)
{
  mulib_ct_blob next = { NULL, 0, 0 };
  int rc;
  if (!blob || (!data && n)) return MULIB_CT_EINVAL;
  rc = mulib_ct_blob_reserve (&next, n, limit);
  if (rc) return rc;
  if (n) memcpy (next.data, data, n);
  next.length = n;
  mulib_ct_blob_free (blob);
  *blob = next;
  return 0;
}
/** @brief Append nonaliasing bytes with an explicit maximum length. */
static inline int
mulib_ct_blob_append (mulib_ct_blob *blob, const void *data, size_t n,
                      size_t limit)
{
  size_t total, capacity;
  int rc;
  if (!blob || (!data && n)) return MULIB_CT_EINVAL;
  rc = mulib_ct_add_size (blob->length, n, &total);
  if (rc) return rc;
  if (total > limit) return MULIB_CT_ELIMIT;
  capacity = blob->capacity;
  if (capacity < total)
    {
      if (!capacity) capacity = limit < 64 ? limit : 64;
      while (capacity < total)
        {
          if (capacity > limit / 2) { capacity = limit; break; }
          capacity *= 2;
        }
    }
  rc = mulib_ct_blob_reserve (blob, capacity > limit ? total : capacity, limit);
  if (rc) return rc;
  if (n) memcpy (blob->data + blob->length, data, n);
  blob->length = total;
  return 0;
}
/** @brief Unaligned, endian-independent little-endian wire loads. */
static inline uint32_t mulib_ct_load32 (const unsigned char *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8
         | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline uint64_t mulib_ct_load64 (const unsigned char *p)
{
  return (uint64_t)mulib_ct_load32 (p) | (uint64_t)mulib_ct_load32 (p + 4) << 32;
}
/** @brief Unaligned little-endian wire stores. */
static inline void mulib_ct_store32 (unsigned char *p, uint32_t v)
{
  unsigned i;
  for (i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
static inline void mulib_ct_store64 (unsigned char *p, uint64_t v)
{
  mulib_ct_store32 (p, (uint32_t)v);
  mulib_ct_store32 (p + 4, (uint32_t)(v >> 32));
}
/** @brief Saturating counter addition for metrics, never for wire lengths. */
static inline uint64_t mulib_ct_counter_add (uint64_t a, uint64_t b)
{
  return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}
#ifdef __cplusplus
}
#endif
#endif

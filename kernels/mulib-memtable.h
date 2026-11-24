/**
 * @file mulib-memtable.h
 * @brief Owned sorted binary records with sequence numbers and tombstones.
 * Define MULIB_IMPL_MEMTABLE once. Sorted-vector insertion is O(n), lookup
 * O(log n).
 */
#ifndef MULIB_MEMTABLE_H
#define MULIB_MEMTABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_MEMTABLE_OK = 0,
    MULIB_MEMTABLE_NOTFOUND = 1,
    MULIB_MEMTABLE_EINVAL = -1,
    MULIB_MEMTABLE_ENOMEM = -2,
    MULIB_MEMTABLE_EOVERFLOW = -3
  };
  /** @brief Owned bytes; do not modify fields. Empty keys/values are valid. */
  typedef struct mulib_memtable_record
  {
    unsigned char *key, *value;
    size_t key_size, value_size;
    uint64_t sequence;
    int deleted;
  } mulib_memtable_record_t;
  /** @brief Initialize to zero; do not copy except through clone. Not
   * thread-safe. */
  typedef struct mulib_memtable
  {
    mulib_memtable_record_t *records;
    size_t count, capacity;
    uint64_t revision;
  } mulib_memtable_t;
  /** @brief Unsigned byte lexicographic comparison; NULL permitted for zero
   * size. */
  int mulib_memtable_compare (const void *a, size_t an, const void *b,
                              size_t bn);
  /** @brief First index whose key is >= key; invalid input returns SIZE_MAX.
   */
  size_t mulib_memtable_lower_bound (const mulib_memtable_t *t,
                                     const void *key, size_t n);
  /** @brief Borrow a record, including tombstones, until mutation/free. */
  int mulib_memtable_get (const mulib_memtable_t *t, const void *key, size_t n,
                          const mulib_memtable_record_t **out);
  /** @brief Copy/replace a record; older sequences are ignored, equal
   * replaces. deleted must be 0/1 and tombstones must have empty values.
   * Failure is atomic. Input bytes may alias existing records. Revision
   * advances only on mutation. */
  int mulib_memtable_put (mulib_memtable_t *t, const void *key, size_t kn,
                          const void *value, size_t vn, uint64_t sequence,
                          int deleted);
  /** @brief Deep copy into a zero/uninitialized destination; unchanged on
   * failure. */
  int mulib_memtable_clone (const mulib_memtable_t *src,
                            mulib_memtable_t *out);
  /** @brief Release and zero, accepting NULL. */
  void mulib_memtable_free (mulib_memtable_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MEMTABLE) && !defined(MULIB_MEMTABLE_IMPLEMENTED)
#define MULIB_MEMTABLE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_memtable_compare (const void *a, size_t an, const void *b, size_t bn)
{
  size_t n = an < bn ? an : bn;
  int c = n ? memcmp (a, b, n) : 0;
  return c ? c : (an > bn) - (an < bn);
}
size_t
mulib_memtable_lower_bound (const mulib_memtable_t *t, const void *k, size_t n)
{
  size_t l = 0, h;
  if (!t || (!k && n))
    return SIZE_MAX;
  h = t->count;
  while (l < h)
    {
      size_t m = l + (h - l) / 2;
      if (mulib_memtable_compare (t->records[m].key, t->records[m].key_size, k,
                                  n)
          < 0)
        l = m + 1;
      else
        h = m;
    }
  return l;
}
int
mulib_memtable_get (const mulib_memtable_t *t, const void *k, size_t n,
                    const mulib_memtable_record_t **out)
{
  size_t i = mulib_memtable_lower_bound (t, k, n);
  if (i == SIZE_MAX || !out)
    return MULIB_MEMTABLE_EINVAL;
  if (i == t->count
      || mulib_memtable_compare (t->records[i].key, t->records[i].key_size, k,
                                 n))
    return MULIB_MEMTABLE_NOTFOUND;
  *out = t->records + i;
  return 0;
}
int
mulib_memtable_put (mulib_memtable_t *t, const void *k, size_t kn,
                    const void *v, size_t vn, uint64_t seq, int del)
{
  size_t i, cap;
  int found;
  mulib_memtable_record_t r, *next;
  if (!t || (!k && kn) || (!v && vn) || (del != 0 && del != 1) || (del && vn))
    return -1;
  i = mulib_memtable_lower_bound (t, k, kn);
  found = i < t->count
          && !mulib_memtable_compare (t->records[i].key,
                                      t->records[i].key_size, k, kn);
  if (found && t->records[i].sequence > seq)
    return 0;
  if (t->revision == UINT64_MAX)
    return MULIB_MEMTABLE_EOVERFLOW;
  memset (&r, 0, sizeof (r));
  r.key = (unsigned char *)malloc (kn ? kn : 1);
  r.value = (unsigned char *)malloc (vn ? vn : 1);
  if (!r.key || !r.value)
    {
      free (r.key);
      free (r.value);
      return -2;
    }
  if (kn)
    memcpy (r.key, k, kn);
  if (vn)
    memcpy (r.value, v, vn);
  r.key_size = kn;
  r.value_size = vn;
  r.sequence = seq;
  r.deleted = del;
  if (!found && t->count == t->capacity)
    {
      cap = t->capacity ? t->capacity : 8;
      if (cap > SIZE_MAX / 2 || cap * 2 > SIZE_MAX / sizeof (*next))
        {
          free (r.key);
          free (r.value);
          return -3;
        }
      cap *= 2;
      next = (mulib_memtable_record_t *)realloc (t->records,
                                                 cap * sizeof (*next));
      if (!next)
        {
          free (r.key);
          free (r.value);
          return -2;
        }
      t->records = next;
      t->capacity = cap;
    }
  if (found)
    {
      free (t->records[i].key);
      free (t->records[i].value);
    }
  else
    {
      memmove (t->records + i + 1, t->records + i,
               (t->count - i) * sizeof (r));
      ++t->count;
    }
  t->records[i] = r;
  ++t->revision;
  return 0;
}
void
mulib_memtable_free (mulib_memtable_t *t)
{
  size_t i;
  if (!t)
    return;
  for (i = 0; i < t->count; ++i)
    {
      free (t->records[i].key);
      free (t->records[i].value);
    }
  free (t->records);
  memset (t, 0, sizeof (*t));
}
int
mulib_memtable_clone (const mulib_memtable_t *s, mulib_memtable_t *out)
{
  mulib_memtable_t t = { 0 };
  size_t i;
  int rc;
  if (!s || !out || s == out)
    return -1;
  for (i = 0; i < s->count; ++i)
    {
      const mulib_memtable_record_t *r = s->records + i;
      rc = mulib_memtable_put (&t, r->key, r->key_size, r->value,
                               r->value_size, r->sequence, r->deleted);
      if (rc)
        {
          mulib_memtable_free (&t);
          return rc;
        }
    }
  t.revision = s->revision;
  *out = t;
  return 0;
}
#endif

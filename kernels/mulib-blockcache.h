/** @file mulib-blockcache.h
 * @brief Bounded, owning LRU cache for binary blocks keyed by uint64_t.
 * Define MULIB_IMPL_BLOCKCACHE once. No locking, pinning or implicit I/O. */
#ifndef MULIB_BLOCKCACHE_H
#define MULIB_BLOCKCACHE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_blockcache_entry
  {
    uint64_t key;
    unsigned char *data;
    size_t size;
  } mulib_blockcache_entry_t;
  typedef struct mulib_blockcache
  {
    mulib_blockcache_entry_t *entries;
    size_t count, bytes, max_entries, max_bytes;
  } mulib_blockcache_t;
  /** @brief Initialize with nonzero entry and byte limits, no allocation. 1
   * succeeds. */
  int mulib_blockcache_init (mulib_blockcache_t *c, size_t entries,
                             size_t bytes);
  /** @brief Copy/replace, evicting least-recently used blocks only after
   * allocation. 1 succeeds, 0 invalid/oversize/OOM. Input may alias cached
   * data. */
  int mulib_blockcache_put (mulib_blockcache_t *c, uint64_t key,
                            const void *data, size_t size);
  /** @brief Promote and borrow until next mutation/free; 1 hit, 0 miss. */
  int mulib_blockcache_get (mulib_blockcache_t *c, uint64_t key,
                            const void **data, size_t *size);
  /** @brief Free/reset, accepting NULL. */
  void mulib_blockcache_free (mulib_blockcache_t *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BLOCKCACHE) && !defined(MULIB_BLOCKCACHE_IMPLEMENTED)
#define MULIB_BLOCKCACHE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_blockcache_init (mulib_blockcache_t *c, size_t n, size_t b)
{
  if (!c || !n || !b || n > SIZE_MAX / sizeof (*c->entries))
    return 0;
  memset (c, 0, sizeof (*c));
  c->max_entries = n;
  c->max_bytes = b;
  return 1;
}
static void
mulib_blockcache_remove_ (mulib_blockcache_t *c, size_t i)
{
  c->bytes -= c->entries[i].size;
  free (c->entries[i].data);
  memmove (c->entries + i, c->entries + i + 1,
           (c->count - i - 1) * sizeof (*c->entries));
  --c->count;
}
int
mulib_blockcache_put (mulib_blockcache_t *c, uint64_t k, const void *p,
                      size_t n)
{
  unsigned char *copy;
  size_t i;
  mulib_blockcache_entry_t *next;
  if (!c || !c->max_entries || n > c->max_bytes || (!p && n))
    return 0;
  copy = (unsigned char *)malloc (n ? n : 1);
  if (!copy)
    return 0;
  if (n)
    memcpy (copy, p, n);
  for (i = 0; i < c->count && c->entries[i].key != k; ++i)
    {
    }
  if (i == c->count && c->count < c->max_entries)
    {
      next = (mulib_blockcache_entry_t *)realloc (
          c->entries, (c->count + 1) * sizeof (*next));
      if (!next)
        {
          free (copy);
          return 0;
        }
      c->entries = next;
    }
  if (i < c->count)
    mulib_blockcache_remove_ (c, i);
  while (c->count
         && (c->count == c->max_entries || c->bytes > c->max_bytes - n))
    mulib_blockcache_remove_ (c, 0);
  c->entries[c->count].key = k;
  c->entries[c->count].data = copy;
  c->entries[c->count].size = n;
  ++c->count;
  c->bytes += n;
  return 1;
}
int
mulib_blockcache_get (mulib_blockcache_t *c, uint64_t k, const void **p,
                      size_t *n)
{
  size_t i;
  mulib_blockcache_entry_t e;
  if (!c || !p || !n)
    return 0;
  for (i = 0; i < c->count; ++i)
    if (c->entries[i].key == k)
      {
        e = c->entries[i];
        memmove (c->entries + i, c->entries + i + 1,
                 (c->count - i - 1) * sizeof (e));
        c->entries[c->count - 1] = e;
        *p = e.data;
        *n = e.size;
        return 1;
      }
  return 0;
}
void
mulib_blockcache_free (mulib_blockcache_t *c)
{
  if (c)
    {
      while (c->count)
        mulib_blockcache_remove_ (c, c->count - 1);
      free (c->entries);
      memset (c, 0, sizeof (*c));
    }
}
#endif

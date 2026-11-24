/** @file mulib-dbindex.h
 * @brief Binary database key to row-ID index, sharing the memtable model.
 * Define MULIB_IMPL_DBINDEX plus MEMTABLE and BYCUTILS switches. */
#ifndef MULIB_DBINDEX_H
#define MULIB_DBINDEX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbindex
  {
    mulib_memtable_t keys;
  } mulib_dbindex_t;
  /** @brief Copy/replace key->row mapping; 0 success, negative failure. Zero
   * init. */
  int mulib_dbindex_set (mulib_dbindex_t *t, const void *key, size_t size,
                         uint64_t row);
  /** @brief Lookup; 0 found, 1 absent/deleted, negative invalid. */
  int mulib_dbindex_get (const mulib_dbindex_t *t, const void *key,
                         size_t size, uint64_t *row);
  /** @brief Tombstone a key; 0 success, 1 already absent, negative error. */
  int mulib_dbindex_remove (mulib_dbindex_t *t, const void *key, size_t size);
  /** @brief Free/reset. */
  void mulib_dbindex_free (mulib_dbindex_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBINDEX) && !defined(MULIB_DBINDEX_IMPLEMENTED)
#define MULIB_DBINDEX_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_dbindex_set (mulib_dbindex_t *t, const void *k, size_t n, uint64_t row)
{
  unsigned char b[8];
  if (!t)
    return -1;
  mulib_bycutils_store (b, 8, 0, 8, row);
  return mulib_memtable_put (&t->keys, k, n, b, 8, 0, 0);
}
int
mulib_dbindex_get (const mulib_dbindex_t *t, const void *k, size_t n,
                   uint64_t *row)
{
  const mulib_memtable_record_t *r;
  int rc;
  if (!t || !row)
    return -1;
  rc = mulib_memtable_get (&t->keys, k, n, &r);
  if (rc)
    return rc;
  if (r->deleted)
    return 1;
  return mulib_bycutils_load (r->value, r->value_size, 0, 8, row) ? 0 : -1;
}
int
mulib_dbindex_remove (mulib_dbindex_t *t, const void *k, size_t n)
{
  uint64_t v;
  int rc = mulib_dbindex_get (t, k, n, &v);
  return rc ? rc : mulib_memtable_put (&t->keys, k, n, NULL, 0, 0, 1);
}
void
mulib_dbindex_free (mulib_dbindex_t *t)
{
  if (t)
    mulib_memtable_free (&t->keys);
}
#endif

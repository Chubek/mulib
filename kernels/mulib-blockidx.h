/** @file mulib-blockidx.h
 * @brief Sparse sorted binary fence keys to 64-bit block offsets.
 * Define MULIB_IMPL_BLOCKIDX plus MEMTABLE and BYCUTILS switches. */
#ifndef MULIB_BLOCKIDX_H
#define MULIB_BLOCKIDX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_blockidx
  {
    mulib_memtable_t fences;
  } mulib_blockidx_t;
  /** @brief Copy/replace a block's minimum key; zero initialize index. 0
   * success. */
  int mulib_blockidx_add (mulib_blockidx_t *idx, const void *key, size_t size,
                          uint64_t offset);
  /** @brief Find greatest fence <= key; 0 found, 1 below all fences, negative
   * invalid. */
  int mulib_blockidx_find (const mulib_blockidx_t *idx, const void *key,
                           size_t size, uint64_t *offset);
  /** @brief Free/reset all keys. */
  void mulib_blockidx_free (mulib_blockidx_t *idx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BLOCKIDX) && !defined(MULIB_BLOCKIDX_IMPLEMENTED)
#define MULIB_BLOCKIDX_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_blockidx_add (mulib_blockidx_t *t, const void *k, size_t n, uint64_t off)
{
  unsigned char b[8];
  if (!t)
    return -1;
  mulib_bycutils_store (b, 8, 0, 8, off);
  return mulib_memtable_put (&t->fences, k, n, b, 8, 0, 0);
}
int
mulib_blockidx_find (const mulib_blockidx_t *t, const void *k, size_t n,
                     uint64_t *off)
{
  size_t i;
  const mulib_memtable_record_t *r;
  if (!t || !off)
    return -1;
  i = mulib_memtable_lower_bound (&t->fences, k, n);
  if (i == SIZE_MAX)
    return -1;
  if (i == t->fences.count
      || mulib_memtable_compare (t->fences.records[i].key,
                                 t->fences.records[i].key_size, k, n))
    {
      if (!i)
        return 1;
      --i;
    }
  r = t->fences.records + i;
  return mulib_bycutils_load (r->value, r->value_size, 0, 8, off) ? 0 : -1;
}
void
mulib_blockidx_free (mulib_blockidx_t *t)
{
  if (t)
    mulib_memtable_free (&t->fences);
}
#endif

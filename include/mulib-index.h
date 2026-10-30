/** @file mulib-index.h
 * @brief Unique ordered integer-to-row-ID secondary index using the B+ tree.
 * Define MULIB_IMPL_INDEX and MULIB_IMPL_BPTREE once. */
#ifndef MULIB_INDEX_H
#define MULIB_INDEX_H
#include "mulib-b+tree.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_index
  {
    mulib_bptree_t tree;
  } mulib_index_t;
  /** @brief Zero init. Insert unique key; 1 success, 0 duplicate/invalid/OOM.
   */
  int mulib_index_insert (mulib_index_t *index, int64_t key, uint64_t row);
  /** @brief Lookup; 1 found, optional output unchanged on miss. */
  int mulib_index_find (const mulib_index_t *index, int64_t key,
                        uint64_t *row);
  /** @brief Remove; 1 found, 0 absent. */
  int mulib_index_remove (mulib_index_t *index, int64_t key);
  /** @brief Release/reset index. */
  void mulib_index_free (mulib_index_t *index);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INDEX) && !defined(MULIB_INDEX_IMPLEMENTED)
#define MULIB_INDEX_IMPLEMENTED
int
mulib_index_insert (mulib_index_t *t, int64_t k, uint64_t v)
{
  return t && !mulib_bptree_get (&t->tree, k, NULL)
         && mulib_bptree_set (&t->tree, k, v);
}
int
mulib_index_find (const mulib_index_t *t, int64_t k, uint64_t *v)
{
  return t && mulib_bptree_get (&t->tree, k, v);
}
int
mulib_index_remove (mulib_index_t *t, int64_t k)
{
  return t && mulib_bptree_remove (&t->tree, k, NULL);
}
void
mulib_index_free (mulib_index_t *t)
{
  if (t)
    mulib_bptree_free (&t->tree);
}
#endif

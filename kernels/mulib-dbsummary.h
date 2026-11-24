/** @file mulib-dbsummary.h
 * @brief Checked record/byte/sequence statistics for shared database tables.
 * Define MULIB_IMPL_DBSUMMARY; only memtable declarations are needed. */
#ifndef MULIB_DBSUMMARY_H
#define MULIB_DBSUMMARY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbsummary
  {
    size_t live, deleted, key_bytes, value_bytes;
    uint64_t max_sequence;
  } mulib_dbsummary_t;
  /** @brief Compute without mutation; 1 succeeds, 0 invalid/overflow. */
  int mulib_dbsummary_compute (const mulib_memtable_t *t,
                               mulib_dbsummary_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBSUMMARY) && !defined(MULIB_DBSUMMARY_IMPLEMENTED)
#define MULIB_DBSUMMARY_IMPLEMENTED
int
mulib_dbsummary_compute (const mulib_memtable_t *t, mulib_dbsummary_t *out)
{
  mulib_dbsummary_t s = { 0 };
  size_t i;
  if (!t || !out)
    return 0;
  for (i = 0; i < t->count; ++i)
    {
      const mulib_memtable_record_t *r = t->records + i;
      if (r->key_size > SIZE_MAX - s.key_bytes
          || r->value_size > SIZE_MAX - s.value_bytes)
        return 0;
      s.key_bytes += r->key_size;
      s.value_bytes += r->value_size;
      if (r->deleted)
        ++s.deleted;
      else
        ++s.live;
      if (r->sequence > s.max_sequence)
        s.max_sequence = r->sequence;
    }
  *out = s;
  return 1;
}
#endif

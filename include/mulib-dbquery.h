/** @file mulib-dbquery.h
 * @brief Ordered half-open binary key scans over live memtable records.
 * Define MULIB_IMPL_DBQUERY and MULIB_IMPL_MEMTABLE. */
#ifndef MULIB_DBQUERY_H
#define MULIB_DBQUERY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Callback borrows record; nonzero continues, zero stops. Never
   * mutate table. */
  typedef int (*mulib_dbquery_visit_fn) (const mulib_memtable_record_t *,
                                         void *);
  /** @brief Scan [low,high), skipping tombstones. NULL high means no upper
   * bound; NULL low with size zero is the empty key. limit=0 visits none.
   * Count includes the stopping callback. Invalid input returns SIZE_MAX. */
  size_t mulib_dbquery_scan (const mulib_memtable_t *t, const void *low,
                             size_t ln, const void *high, size_t hn,
                             size_t limit, mulib_dbquery_visit_fn visit,
                             void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBQUERY) && !defined(MULIB_DBQUERY_IMPLEMENTED)
#define MULIB_DBQUERY_IMPLEMENTED
size_t
mulib_dbquery_scan (const mulib_memtable_t *t, const void *lo, size_t ln,
                    const void *hi, size_t hn, size_t limit,
                    mulib_dbquery_visit_fn f, void *ctx)
{
  size_t i = mulib_memtable_lower_bound (t, lo, ln), n = 0;
  if (i == SIZE_MAX || (!hi && hn) || !f)
    return SIZE_MAX;
  for (; i < t->count && n < limit; ++i)
    {
      const mulib_memtable_record_t *r = t->records + i;
      if (hi && mulib_memtable_compare (r->key, r->key_size, hi, hn) >= 0)
        break;
      if (!r->deleted)
        {
          ++n;
          if (!f (r, ctx))
            break;
        }
    }
  return n;
}
#endif

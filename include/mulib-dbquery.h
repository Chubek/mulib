/** @file mulib-dbquery.h
 * @brief Patched ordered binary key scans with optional predicate pushdown.
 * Define MULIB_IMPL_DBQUERY and MULIB_IMPL_MEMTABLE in one translation unit. */
#ifndef MULIB_DBQUERY_H
#define MULIB_DBQUERY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_dbquery_visit_fn) (const mulib_memtable_record_t *, void *);
  typedef int (*mulib_dbquery_predicate_fn) (const mulib_memtable_record_t *, void *);
  size_t mulib_dbquery_scan (const mulib_memtable_t *table, const void *low,
                             size_t low_bytes, const void *high, size_t high_bytes,
                             size_t limit, mulib_dbquery_visit_fn visit, void *context);
  /* Predicate skips when it returns zero; NULL predicate accepts all nondeleted.
   * Count includes stopping visit. Does not mutate table. */
  size_t mulib_dbquery_scan_where (const mulib_memtable_t *table, const void *low,
                                   size_t low_bytes, const void *high,
                                   size_t high_bytes, size_t limit,
                                   mulib_dbquery_predicate_fn predicate,
                                   void *pred_context, mulib_dbquery_visit_fn visit,
                                   void *visit_context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBQUERY) && !defined(MULIB_DBQUERY_IMPLEMENTED)
#define MULIB_DBQUERY_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_dbquery_scan_where (const mulib_memtable_t *t, const void *lo, size_t ln,
                            const void *hi, size_t hn, size_t limit,
                            mulib_dbquery_predicate_fn pred, void *pctx,
                            mulib_dbquery_visit_fn visit, void *ctx)
  {
    if (!t || (!lo && ln) || (!hi && hn) || !visit)
      return SIZE_MAX;
    size_t i = mulib_memtable_lower_bound (t, lo, ln), n = 0;
    if (i == SIZE_MAX)
      return SIZE_MAX;
    for (; i < t->count && n < limit; i++)
      {
        const mulib_memtable_record_t *r = t->records + i;
        if (hi && mulib_memtable_compare (r->key, r->key_size, hi, hn) >= 0)
          break;
        if (!r->deleted && (!pred || pred (r, pctx)))
          {
            ++n;
            if (!visit (r, ctx))
              break;
          }
      }
    return n;
  }
  size_t
  mulib_dbquery_scan (const mulib_memtable_t *t, const void *lo, size_t ln,
                      const void *hi, size_t hn, size_t limit,
                      mulib_dbquery_visit_fn visit, void *ctx)
  {
    return mulib_dbquery_scan_where (t, lo, ln, hi, hn, limit, NULL, NULL, visit, ctx);
  }
#ifdef __cplusplus
}
#endif
#endif

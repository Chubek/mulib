#define MULIB_IMPL_CRAWLQUEUE
#define MULIB_IMPL_DBQUERY
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_INVINDEX
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_SCRAPE_SCHEMA
#define MULIB_IMPL_SEARCH_RANK
#include "mulib-crawlqueue.h"
#include "mulib-dbquery.h"
#include "mulib-invindex.h"
#include "mulib-scrape-schema.h"
#include "mulib-search-rank.h"
#include <assert.h>
#include <float.h>

static int
predicate (const mulib_memtable_record_t *r, void *ctx)
{
  ++*(size_t *)ctx;
  return r->value_size == 1 && r->value[0] == 'y';
}
static int
visit (const mulib_memtable_record_t *r, void *ctx)
{
  assert (!r->deleted);
  ++*(size_t *)ctx;
  return 1;
}
static int
stop (const mulib_memtable_record_t *r, void *ctx)
{
  (void)r;
  (void)ctx;
  return 0;
}
int
main (void)
{
  mulib_crawlqueue_t q = { 0 };
  assert (mulib_crawlqueue_push (&q, "a") == 1 && mulib_crawlqueue_push (&q, "b") == 1);
  const char *first = mulib_crawlqueue_next (&q);
  assert (!strcmp (first, "a"));
  assert (mulib_crawlqueue_position (&q) == 1 && mulib_crawlqueue_pending (&q) == 1);
  assert (mulib_crawlqueue_push (&q, first) == 2 && mulib_crawlqueue_seek (&q, 0));
  assert (mulib_crawlqueue_next (&q) == first && !mulib_crawlqueue_seek (&q, 3));
  assert (mulib_crawlqueue_position (&q) == 1);
  mulib_crawlqueue_free (&q);
  mulib_crawlqueue_free (&q);
  mulib_memtable_t table = { 0 };
  assert (!mulib_memtable_put (&table, "a", 1, "n", 1, 0, 0));
  assert (!mulib_memtable_put (&table, "b\0", 2, "y", 1, 0, 0));
  assert (!mulib_memtable_put (&table, "b\1", 2, NULL, 0, 0, 1));
  assert (!mulib_memtable_put (&table, "c", 1, "y", 1, 0, 0));
  size_t checked = 0, seen = 0;
  uint64_t revision = table.revision;
  assert (mulib_dbquery_scan_where (&table, NULL, 0, "c", 1, 1, predicate, &checked,
                                    visit, &seen)
              == 1
          && checked == 2 && seen == 1);
  assert (mulib_dbquery_scan (&table, NULL, 0, NULL, 0, 10, stop, NULL) == 1);
  assert (mulib_dbquery_scan (&table, NULL, 0, NULL, 0, 0, visit, &seen) == 0);
  assert (mulib_dbquery_scan (&table, NULL, 1, NULL, 0, 10, visit, &seen) == SIZE_MAX);
  assert (table.revision == revision);
  mulib_memtable_free (&table);
  mulib_invindex_t index = { 0 };
  uint64_t ids[3] = { 99 };
  for (uint64_t i = 10; i > 0; i--)
    {
      assert (!mulib_invindex_add (&index, "a", 1, i));
      if (i % 2 == 0)
        assert (!mulib_invindex_add (&index, "b", 1, i));
    }
  assert (!mulib_invindex_add (&index, "a", 1, 1)
          && mulib_invindex_count (&index, "a", 1) == 10);
  assert (mulib_invindex_intersect (&index, "a", 1, "b", 1, ids, 3) == SIZE_MAX
          && ids[0] == 99);
  assert (!mulib_invindex_remove (&index, "b", 1, 8)
          && !mulib_invindex_remove (&index, "b", 1, 10));
  assert (mulib_invindex_intersect (&index, "a", 1, "b", 1, ids, 3) == 3 && ids[0] == 2
          && ids[2] == 6);
  assert (mulib_invindex_count (&index, "absent", 6) == 0);
  assert (!mulib_memtable_put (&index.terms, "bad", 3, "x", 1, 0, 0));
  assert (mulib_invindex_count (&index, "bad", 3) == SIZE_MAX);
  mulib_invindex_free (&index);
  int64_t integer = 42;
  double real = 42;
  int boolean = 42;
  assert (mulib_scrape_schema_integer ("-9223372036854775808", INT64_MIN, INT64_MAX,
                                       &integer)
          && integer == INT64_MIN);
  assert (!mulib_scrape_schema_integer ("9223372036854775808", INT64_MIN, INT64_MAX,
                                        &integer)
          && integer == INT64_MIN);
  assert (!mulib_scrape_schema_integer ("1x", 0, 2, &integer));
  assert (mulib_scrape_schema_real ("1.5", 0, 2, &real) && real == 1.5);
  assert (!mulib_scrape_schema_real ("nan", 0, 2, &real) && real == 1.5);
  assert (!mulib_scrape_schema_real ("1e9999", 0, DBL_MAX, &real));
  assert (mulib_scrape_schema_boolean ("true", &boolean) && boolean == 1);
  assert (!mulib_scrape_schema_boolean ("True", &boolean) && boolean == 1);
  assert (mulib_scrape_schema_string (NULL, 0, 0)
          && !mulib_scrape_schema_string ("", 1, 2));
  assert (mulib_search_rank_count ("aaaa", "aa") == 3);
  assert (mulib_search_rank_words ("cat CAT cats _cat cat_ cat.", "cat") == 3);
  assert (mulib_search_rank_fields ("cat", "cat cat", "cat", 2, 0.5) == 3);
  assert (isnan (mulib_search_rank_fields ("cat cat", NULL, "cat", DBL_MAX, 1)));
  return 0;
}

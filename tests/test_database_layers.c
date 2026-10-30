#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_WAL
#define MULIB_IMPL_SSTFILE
#define MULIB_IMPL_LSMTREE
#define MULIB_IMPL_COWBPTREE
#define MULIB_IMPL_BPTREE
#define MULIB_IMPL_HEAPTABLE
#define MULIB_IMPL_BLOCKIDX
#define MULIB_IMPL_BLOCKCACHE
#define MULIB_IMPL_INDEX
#define MULIB_IMPL_DBINDEX
#define MULIB_IMPL_INVINDEX
#define MULIB_IMPL_SPATIALTREE
#define MULIB_IMPL_DBSUMMARY
#define MULIB_IMPL_DBQUERY
#define MULIB_IMPL_DBTRANSACTION
#define MULIB_IMPL_SQL
#define MULIB_IMPL_STRBUF
#include "mulib-blockcache.h"
#include "mulib-blockidx.h"
#include "mulib-cowb+tree.h"
#include "mulib-dbindex.h"
#include "mulib-dbquery.h"
#include "mulib-dbsummary.h"
#include "mulib-dbtransaction.h"
#include "mulib-heaptable.h"
#include "mulib-index.h"
#include "mulib-invindex.h"
#include "mulib-lsmtree.h"
#include "mulib-spatialtree.h"
#include "mulib-sql.h"
#include <assert.h>

static int
visit_record (const mulib_memtable_record_t *r, void *ctx)
{
  (void)r;
  ++*(size_t *)ctx;
  return 1;
}
static int
visit_point (const mulib_spatialtree_point_t *p, void *ctx)
{
  *(uint64_t *)ctx += p->id;
  return 1;
}
static void
tables (void)
{
  mulib_memtable_t t = { 0 }, copy = { 0 };
  const mulib_memtable_record_t *r;
  mulib_buffer_t bytes = { 0 };
  mulib_dbsummary_t summary;
  size_t count = 0, i;
  mulib_dbtransaction_t a = { 0 }, b = { 0 };
  assert (!mulib_memtable_put (&t, "b", 1, "x\0y", 3, 2, 0));
  assert (!mulib_memtable_put (&t, NULL, 0, NULL, 0, 0, 0));
  assert (!mulib_memtable_put (&t, "a", 1, NULL, 0, 1, 1));
  assert (!mulib_memtable_get (&t, "b", 1, &r) && r->value_size == 3);
  assert (!mulib_memtable_put (&t, "b", 1, "old", 3, 1, 0));
  assert (!mulib_memtable_get (&t, "b", 1, &r) && r->value[1] == 0);
  assert (!mulib_memtable_put (&t, r->key, 1, r->value, 3, 3, 0));
  assert (mulib_memtable_put (&t, NULL, 1, NULL, 0, 0, 0) < 0);
  assert (!mulib_memtable_clone (&t, &copy));
  assert (!mulib_sstfile_encode (&copy, &bytes));
  mulib_memtable_free (&copy);
  assert (!mulib_sstfile_decode (bytes.data, bytes.length, &copy));
  assert (copy.count == 3 && copy.records[1].deleted);
  for (i = 0; i < bytes.length; ++i)
    {
      mulib_memtable_t bad = { 0 };
      assert (mulib_sstfile_decode (bytes.data, i, &bad) < 0);
      assert (!bad.count);
    }
  bytes.data[17] ^= 1;
  assert (mulib_sstfile_decode (bytes.data, bytes.length, &copy) < 0);
  bytes.data[17] ^= 1;
  assert (mulib_dbsummary_compute (&t, &summary) && summary.live == 2
          && summary.deleted == 1 && summary.value_bytes == 3);
  assert (mulib_dbquery_scan (&t, NULL, 0, NULL, 0, 100, visit_record, &count)
              == 2
          && count == 2);
  assert (!mulib_dbtransaction_begin (&a, &t)
          && !mulib_dbtransaction_begin (&b, &t));
  assert (!mulib_memtable_put (&a.working, "z", 1, "1", 1, 5, 0));
  assert (!mulib_dbtransaction_commit (&a));
  assert (mulib_dbtransaction_commit (&b) == 1);
  mulib_dbtransaction_abort (&b);
  mulib_dbtransaction_abort (&a);
  mulib_memtable_free (&copy);
  mulib_memtable_free (&t);
  mulib_buffer_free (&bytes);
}
static void
lsm (void)
{
  mulib_lsmtree_t t = { 0 }, recovered = { 0 };
  mulib_buffer_t out = { 0 };
  size_t consumed;
  assert (!mulib_lsmtree_put (&t, "k", 1, "v1", 2, 0));
  assert (!mulib_lsmtree_flush (&t));
  assert (!mulib_lsmtree_put (&t, "k", 1, "v2", 2, 0));
  assert (!mulib_lsmtree_flush (&t));
  assert (!mulib_lsmtree_get (&t, "k", 1, &out) && out.length == 2
          && !memcmp (out.data, "v2", 2));
  out.length = 0;
  assert (!mulib_lsmtree_compact (&t) && t.run_count == 1);
  assert (!mulib_lsmtree_get (&t, "k", 1, &out)
          && !memcmp (out.data, "v2", 2));
  assert (!mulib_lsmtree_put (&t, "k", 1, NULL, 0, 1));
  assert (!mulib_lsmtree_flush (&t));
  assert (!mulib_lsmtree_compact (&t));
  assert (mulib_lsmtree_get (&t, "k", 1, &out) == 1);
  assert (!mulib_lsmtree_recover (t.wal.data, t.wal.length, 0, &recovered,
                                  &consumed));
  assert (consumed == t.wal.length && recovered.sequence == 3);
  assert (mulib_lsmtree_get (&recovered, "k", 1, &out) == 1);
  mulib_lsmtree_free (&recovered);
  assert (
      mulib_lsmtree_recover (t.wal.data, t.wal.length - 1, 0, &recovered, NULL)
      < 0);
  assert (!mulib_lsmtree_recover (t.wal.data, t.wal.length - 1, 1, &recovered,
                                  &consumed));
  assert (recovered.sequence == 2 && consumed < t.wal.length - 1);
  mulib_lsmtree_free (&recovered);
  t.wal.data[26] ^= 1;
  assert (mulib_lsmtree_recover (t.wal.data, t.wal.length, 1, &recovered, NULL)
          < 0);
  mulib_lsmtree_free (&t);
  mulib_buffer_free (&out);
}
static void
indexes (void)
{
  mulib_cowbptree_t a = { 0 }, b = { 0 };
  mulib_index_t idx = { 0 };
  mulib_dbindex_t dbidx = { 0 };
  mulib_invindex_t inv = { 0 };
  mulib_blockidx_t fences = { 0 };
  mulib_heaptable_t heap = { 0 };
  mulib_blockcache_t cache;
  const void *p;
  size_t n;
  uint64_t id, v;
  int i;
  for (i = 0; i < 100; ++i)
    assert (mulib_cowbptree_set (&a, i, (uint64_t)i));
  assert (mulib_cowbptree_snapshot (&a, &b));
  assert (mulib_cowbptree_set (&a, 50, 500));
  assert (mulib_cowbptree_get (&b, 50, &v) && v == 50);
  assert (mulib_cowbptree_remove (&b, 50));
  assert (mulib_cowbptree_get (&a, 50, &v) && v == 500);
  assert (mulib_index_insert (&idx, 1, 10)
          && !mulib_index_insert (&idx, 1, 11));
  assert (mulib_index_find (&idx, 1, &v) && v == 10);
  assert (mulib_index_remove (&idx, 1));
  assert (!mulib_dbindex_set (&dbidx, "a\0b", 3, 42));
  assert (!mulib_dbindex_get (&dbidx, "a\0b", 3, &v) && v == 42);
  assert (!mulib_dbindex_remove (&dbidx, "a\0b", 3));
  assert (mulib_dbindex_get (&dbidx, "a\0b", 3, &v) == 1);
  assert (!mulib_invindex_add (&inv, "t", 1, 9)
          && !mulib_invindex_add (&inv, "t", 1, 1));
  assert (!mulib_invindex_add (&inv, "t", 1, 9));
  assert (!mulib_invindex_at (&inv, "t", 1, 0, &v) && v == 1);
  assert (!mulib_invindex_remove (&inv, "t", 1, 1));
  assert (!mulib_invindex_at (&inv, "t", 1, 0, &v) && v == 9);
  assert (!mulib_blockidx_add (&fences, "b", 1, 22)
          && !mulib_blockidx_add (&fences, "d", 1, 44));
  assert (mulib_blockidx_find (&fences, "a", 1, &v) == 1);
  assert (!mulib_blockidx_find (&fences, "c", 1, &v) && v == 22);
  assert (!mulib_heaptable_insert (&heap, "row", 3, &id));
  assert (id == 1);
  assert (!mulib_heaptable_get (&heap, id, &p, &n) && n == 3);
  assert (!mulib_heaptable_update (&heap, id, NULL, 0, 1));
  assert (mulib_heaptable_get (&heap, id, &p, &n) == 1);
  assert (mulib_blockcache_init (&cache, 2, 6));
  assert (mulib_blockcache_put (&cache, 1, "aa", 2)
          && mulib_blockcache_put (&cache, 2, "bb", 2));
  assert (mulib_blockcache_get (&cache, 1, &p, &n));
  assert (mulib_blockcache_put (&cache, 3, "ccc", 3));
  assert (!mulib_blockcache_get (&cache, 2, &p, &n));
  assert (!mulib_blockcache_put (&cache, 4, "1234567", 7));
  assert (mulib_blockcache_get (&cache, 1, &p, &n)
          && mulib_blockcache_put (&cache, 1, p, n));
  mulib_blockcache_free (&cache);
  mulib_heaptable_free (&heap);
  mulib_blockidx_free (&fences);
  mulib_invindex_free (&inv);
  mulib_dbindex_free (&dbidx);
  mulib_index_free (&idx);
  mulib_cowbptree_free (&a);
  mulib_cowbptree_free (&b);
}
static void
queries (void)
{
  mulib_memtable_t t = { 0 };
  mulib_buffer_t b = { 0 };
  size_t e;
  uint64_t sum = 0;
  mulib_spatialtree_t tree = { 0 };
  const mulib_spatialtree_point_t points[]
      = { { 0, 0, 1 }, { 1, 1, 2 }, { 2, 2, 4 }, { 1, 1, 8 } };
  const char *insert = "INSERT INTO kv VALUES ('it''s','value');";
  const char *select = "select value from kv where key='it''s'";
  assert (!mulib_sql_execute (&t, insert, strlen (insert), NULL, &e)
          && e == SIZE_MAX);
  assert (!mulib_sql_execute (&t, select, strlen (select), &b, &e)
          && b.length == 5);
  assert (!mulib_memtable_put (&t, "it's", 4, "prior", 5, 99, 0));
  assert (!mulib_sql_execute (&t, insert, strlen (insert), NULL, &e));
  b.length = 0;
  assert (!mulib_sql_execute (&t, select, strlen (select), &b, &e)
          && !memcmp (b.data, "value", 5));
  assert (mulib_sql_execute (&t, "SELECT * FROM kv", 16, &b, &e) == -1
          && e != SIZE_MAX);
  assert (mulib_sql_execute (&t, "", 0, &b, &e) == -1);
  assert (mulib_spatialtree_build (points, 4, &tree));
  assert (mulib_spatialtree_query (&tree, 1, 1, 1, 1, visit_point, &sum) == 2
          && sum == 10);
  assert (mulib_spatialtree_query (&tree, 3, 0, 2, 1, visit_point, &sum)
          == SIZE_MAX);
  mulib_spatialtree_free (&tree);
  mulib_buffer_free (&b);
  mulib_memtable_free (&t);
}
int
main (void)
{
  tables ();
  lsm ();
  indexes ();
  queries ();
  return 0;
}

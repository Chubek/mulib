/* Fail each allocation in an ownership/composition scenario, then check cleanup. */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
static size_t calls, fail_at = SIZE_MAX, live;
static void *
checked_malloc (size_t n)
{
  if (calls++ == fail_at)
    return NULL;
  void *p = malloc (n);
  if (p)
    live++;
  return p;
}
static void *
checked_calloc (size_t n, size_t size)
{
  if (calls++ == fail_at)
    return NULL;
  void *p = calloc (n, size);
  if (p)
    live++;
  return p;
}
static void *
checked_realloc (void *old, size_t n)
{
  int fresh = old == NULL;
  if (calls++ == fail_at)
    return NULL;
  void *p = realloc (old, n);
  if (p && fresh)
    live++;
  return p;
}
static void
checked_free (void *p)
{
  if (p)
    {
      assert (live);
      live--;
    }
  free (p);
}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_DATUM
#define MULIB_IMPL_RECORD
#define MULIB_IMPL_RECORDBATCH
#define MULIB_IMPL_COLUMNAR
#define MULIB_IMPL_POSTINGS
#define MULIB_IMPL_BOOLEANQUERY
#define MULIB_IMPL_POSITIONS
#define MULIB_IMPL_RANGEINDEX
#define MULIB_IMPL_GEOINDEX
#define MULIB_IMPL_VECTORINDEX
#define MULIB_IMPL_EMBEDDINGSDB
#define MULIB_IMPL_COUNTMIN
#define MULIB_IMPL_JOINFILTER
#define MULIB_IMPL_LATTICE
#define MULIB_IMPL_CRAWLQUEUE
#define MULIB_IMPL_INGEST_STREAM
#define MULIB_IMPL_FUZZYQUERY
#define MULIB_IMPL_INVINDEX
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-recordbatch.h"
#include "mulib-columnar.h"
#include "mulib-booleanquery.h"
#include "mulib-positions.h"
#include "mulib-rangeindex.h"
#include "mulib-geoindex.h"
#include "mulib-vectorindex.h"
#include "mulib-embeddingsdb.h"
#include "mulib-countmin.h"
#include "mulib-joinfilter.h"
#include "mulib-lattice.h"
#include "mulib-crawlqueue.h"
#include "mulib-ingest-stream.h"
#include "mulib-fuzzyquery.h"
#include "mulib-invindex.h"
#include <stdio.h>

static int
emit (const void *p, size_t n, void *ctx)
{
  (void)p;
  (void)n;
  (void)ctx;
  return 1;
}
static size_t
exercise (void)
{
  calls = 0;
  mulib_datum_t value = { 0 };
  mulib_record_t row = { 0 }, copy = { 0 };
  mulib_recordbatch_t batch = { 0 };
  mulib_columnar_t col = { 0 };
  mulib_postings_t posts = { 0 }, result = { 0 };
  mulib_positions_t pos = { 0 };
  mulib_rangeindex_t ranges = { 0 };
  mulib_geoindex_t geo = { 0 };
  mulib_vectorindex_t vectors = { 0 };
  mulib_embeddingsdb_t db = { 1, 0, NULL };
  mulib_crawlqueue_t queue = { 0 };
  mulib_lattice_t lattice = { 0 };
  mulib_countmin_t cms = { 0 };
  mulib_joinfilter_t filter = { 0 };
  mulib_ingest_stream_t stream = { 0 };
  mulib_invindex_t inverted = { 0 };
  vectors.dimension = 1;
  lattice.attributes = 4;
  for (size_t i = 0; i < 20; i++)
    {
      char key[16];
      snprintf (key, sizeof (key), "key%zu", i);
      double x = (double)i;
      size_t old = row.count;
      if (mulib_datum_set_bytes (&value, key, strlen (key)))
        {
          if (!mulib_record_set (&row, key, &value))
            assert (row.count == old);
        }
      old = batch.count;
      if (!mulib_recordbatch_append (&batch, &row))
        assert (batch.count == old);
      old = copy.count;
      if (!mulib_record_clone (&copy, &row))
        assert (copy.count == old);
      old = col.count;
      if (!mulib_columnar_append (&col, x, 1))
        assert (col.count == old);
      old = posts.count;
      if (!mulib_postings_put (&posts, i, 1))
        assert (posts.count == old);
      old = pos.count;
      if (!mulib_positions_add (&pos, (uint32_t)i))
        assert (pos.count == old);
      old = ranges.count;
      if (!mulib_rangeindex_insert (&ranges, (int64_t)i, (int64_t)i, i))
        assert (ranges.count == old);
      old = geo.count;
      if (!mulib_geoindex_add (&geo, i, x, x))
        assert (geo.count == old);
      old = vectors.count;
      if (!mulib_vectorindex_insert (&vectors, i, &x, 1))
        assert (vectors.count == old);
      old = db.count;
      if (!mulib_embeddingsdb_put (&db, i, &x, 1))
        assert (db.count == old);
      old = queue.len;
      if (!mulib_crawlqueue_push (&queue, key))
        assert (queue.len == old);
      old = lattice.count;
      if (!mulib_lattice_add_rule (&lattice, 1, 2))
        assert (lattice.count == old);
      old = mulib_invindex_count (&inverted, "term", 4);
      if (mulib_invindex_add (&inverted, "term", 4, i) < 0)
        assert (mulib_invindex_count (&inverted, "term", 4) == old);
    }
  if (!mulib_booleanquery_apply (&posts, &posts, MULIB_BOOLEAN_OR, &result))
    assert (!result.items && !result.count);
  else
    assert (result.count == posts.count);
  mulib_embeddings_hit_t hits[2] = { { 99, 99 } };
  double q = 1;
  if (mulib_embeddingsdb_knn (&db, &q, 1, MULIB_EMBEDDINGS_EUCLIDEAN, 2, hits)
      == SIZE_MAX)
    assert (hits[0].id == 99);
  if (!mulib_countmin_init (&cms, 64, 4))
    assert (!cms.cells);
  if (!mulib_joinfilter_init (&filter, 512, 0))
    assert (!filter.bits);
  assert (mulib_ingest_stream_init (&stream, 512));
  char text[260];
  memset (text, 'x', sizeof (text));
  text[259] = '\n';
  size_t n = mulib_ingest_stream_push (&stream, text, sizeof (text), emit, NULL);
  assert (n == 1 || n == SIZE_MAX);
  n = mulib_fuzzyquery_distance ("kitten", 6, "sitting", 7, 3);
  assert (n == 3 || n == SIZE_MAX);
  mulib_datum_free (&value);
  mulib_record_free (&row);
  mulib_record_free (&copy);
  mulib_recordbatch_free (&batch);
  mulib_columnar_free (&col);
  mulib_postings_free (&posts);
  mulib_postings_free (&result);
  mulib_positions_free (&pos);
  mulib_rangeindex_free (&ranges);
  mulib_geoindex_free (&geo);
  mulib_vectorindex_free (&vectors);
  mulib_embeddingsdb_free (&db);
  mulib_crawlqueue_free (&queue);
  mulib_lattice_free (&lattice);
  mulib_countmin_free (&cms);
  mulib_joinfilter_free (&filter);
  mulib_ingest_stream_free (&stream);
  mulib_invindex_free (&inverted);
  assert (live == 0);
  return calls;
}
int
main (void)
{
  size_t n = exercise ();
  for (size_t i = 0; i < n; i++)
    {
      fail_at = i;
      exercise ();
    }
  printf ("Data allocation failures: %zu sites checked\n", n);
  return 0;
}

/** @file mulib-graphdb.h
 * @brief Directed labelled uint64 triples with memtable sequence/tombstones.
 * Define MULIB_IMPL_GRAPHDB and MEMTABLE. No implicit vertex objects, graph
 * query language or persistence. Zero initialize; callbacks must not mutate. */
#ifndef MULIB_GRAPHDB_H
#define MULIB_GRAPHDB_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_graphdb
  {
    mulib_memtable_t edges;
  } mulib_graphdb_t;
  typedef int (*mulib_graphdb_visit_fn) (void *context, uint64_t label,
                                         uint64_t target);
  int mulib_graphdb_edge (mulib_graphdb_t *db, uint64_t source, uint64_t label,
                          uint64_t target, uint64_t sequence, int deleted);
  /** Visit live outgoing edges in label,target order; zero callback stops.
   * Count includes stopping callback; SIZE_MAX invalid. */
  size_t mulib_graphdb_neighbors (const mulib_graphdb_t *db, uint64_t source,
                                  mulib_graphdb_visit_fn visit, void *context);
  void mulib_graphdb_free (mulib_graphdb_t *db);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GRAPHDB) && !defined(MULIB_GRAPHDB_IMPLEMENTED)
#define MULIB_GRAPHDB_IMPLEMENTED
#include <string.h>
static void
mulib_graphdb_key_ (unsigned char *p, uint64_t v)
{
  for (unsigned i = 0; i < 8; ++i)
    p[i] = (unsigned char)(v >> (56 - 8 * i));
}
static uint64_t
mulib_graphdb_load_ (const unsigned char *p)
{
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i)
    v = (v << 8) | p[i];
  return v;
}
int
mulib_graphdb_edge (mulib_graphdb_t *db, uint64_t s, uint64_t l, uint64_t t,
                    uint64_t seq, int deleted)
{
  unsigned char key[24];
  if (!db)
    return MULIB_MEMTABLE_EINVAL;
  mulib_graphdb_key_ (key, s);
  mulib_graphdb_key_ (key + 8, l);
  mulib_graphdb_key_ (key + 16, t);
  return mulib_memtable_put (&db->edges, key, 24, NULL, 0, seq, deleted);
}
size_t
mulib_graphdb_neighbors (const mulib_graphdb_t *db, uint64_t s,
                         mulib_graphdb_visit_fn f, void *ctx)
{
  unsigned char key[24] = { 0 };
  size_t at, count = 0;
  if (!db || !f)
    return SIZE_MAX;
  mulib_graphdb_key_ (key, s);
  at = mulib_memtable_lower_bound (&db->edges, key, 24);
  if (at == SIZE_MAX)
    return SIZE_MAX;
  for (; at < db->edges.count; ++at)
    {
      const mulib_memtable_record_t *r = db->edges.records + at;
      if (r->key_size != 24 || memcmp (r->key, key, 8))
        break;
      if (!r->deleted)
        {
          ++count;
          if (!f (ctx, mulib_graphdb_load_ (r->key + 8),
                  mulib_graphdb_load_ (r->key + 16)))
            break;
        }
    }
  return count;
}
void
mulib_graphdb_free (mulib_graphdb_t *db)
{
  if (db)
    mulib_memtable_free (&db->edges);
}
#endif

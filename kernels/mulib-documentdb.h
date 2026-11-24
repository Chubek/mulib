/** @file mulib-documentdb.h
 * @brief In-memory binary-keyed JSON document records over memtable.
 * Define MULIB_IMPL_DOCUMENTDB, MEMTABLE, JSON and JSON dependencies.
 * Zero initialize; no persistence, query language or concurrency. */
#ifndef MULIB_DOCUMENTDB_H
#define MULIB_DOCUMENTDB_H
#include "mulib-json.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_documentdb
  {
    mulib_memtable_t records;
  } mulib_documentdb_t;
  /** Copy object as JSON text. Same sequence/tombstone rules as memtable. */
  int mulib_documentdb_put (mulib_documentdb_t *db, const void *key,
                            size_t key_length, const mulib_json_t *document,
                            uint64_t sequence);
  /** Independent owned object, or NULL for absent/deleted/error. */
  mulib_json_t *mulib_documentdb_get (const mulib_documentdb_t *db,
                                      const void *key, size_t key_length);
  int mulib_documentdb_delete (mulib_documentdb_t *db, const void *key,
                               size_t key_length, uint64_t sequence);
  void mulib_documentdb_free (mulib_documentdb_t *db);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DOCUMENTDB) && !defined(MULIB_DOCUMENTDB_IMPLEMENTED)
#define MULIB_DOCUMENTDB_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_documentdb_put (mulib_documentdb_t *db, const void *k, size_t kn,
                      const mulib_json_t *v, uint64_t seq)
{
  char *text;
  int rc;
  if (!db || mulib_json_typeof (v) != MULIB_JSON_OBJECT)
    return MULIB_MEMTABLE_EINVAL;
  text = mulib_json_stringify (v, 0);
  if (!text)
    return MULIB_MEMTABLE_ENOMEM;
  rc = mulib_memtable_put (&db->records, k, kn, text, strlen (text), seq, 0);
  free (text);
  return rc;
}
mulib_json_t *
mulib_documentdb_get (const mulib_documentdb_t *db, const void *k, size_t kn)
{
  const mulib_memtable_record_t *r;
  if (!db || mulib_memtable_get (&db->records, k, kn, &r) || r->deleted)
    return NULL;
  return mulib_json_parse ((const char *)r->value, r->value_size, NULL);
}
int
mulib_documentdb_delete (mulib_documentdb_t *db, const void *k, size_t kn,
                         uint64_t seq)
{
  return db ? mulib_memtable_put (&db->records, k, kn, NULL, 0, seq, 1)
            : MULIB_MEMTABLE_EINVAL;
}
void
mulib_documentdb_free (mulib_documentdb_t *db)
{
  if (db)
    mulib_memtable_free (&db->records);
}
#endif

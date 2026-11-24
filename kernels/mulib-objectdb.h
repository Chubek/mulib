/** @file mulib-objectdb.h
 * @brief In-memory type-tagged opaque object blobs, not C pointer persistence.
 * Define MULIB_IMPL_OBJECTDB and MEMTABLE. Zero initialize. Binary ids/values;
 * nonzero application type tags, sequence ordering and tombstones supported. */
#ifndef MULIB_OBJECTDB_H
#define MULIB_OBJECTDB_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objectdb
  {
    mulib_memtable_t records;
  } mulib_objectdb_t;
  int mulib_objectdb_put (mulib_objectdb_t *db, const void *id,
                          size_t id_length, uint32_t type, const void *data,
                          size_t length, uint64_t sequence);
  /** 0 success (borrow until mutation), 1 absent/deleted/type mismatch,
   * negative invalid. Outputs unchanged on failure. */
  int mulib_objectdb_get (const mulib_objectdb_t *db, const void *id,
                          size_t id_length, uint32_t type, const void **data,
                          size_t *length);
  int mulib_objectdb_delete (mulib_objectdb_t *db, const void *id,
                             size_t id_length, uint64_t sequence);
  void mulib_objectdb_free (mulib_objectdb_t *db);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJECTDB) && !defined(MULIB_OBJECTDB_IMPLEMENTED)
#define MULIB_OBJECTDB_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_objectdb_put (mulib_objectdb_t *db, const void *id, size_t in,
                    uint32_t type, const void *p, size_t n, uint64_t seq)
{
  unsigned char *bytes;
  int rc;
  if (!db || !type || (!p && n) || n > SIZE_MAX - 4)
    return MULIB_MEMTABLE_EINVAL;
  bytes = (unsigned char *)malloc (n + 4);
  if (!bytes)
    return MULIB_MEMTABLE_ENOMEM;
  for (unsigned i = 0; i < 4; ++i)
    bytes[i] = (unsigned char)(type >> (8 * i));
  if (n)
    memcpy (bytes + 4, p, n);
  rc = mulib_memtable_put (&db->records, id, in, bytes, n + 4, seq, 0);
  free (bytes);
  return rc;
}
int
mulib_objectdb_get (const mulib_objectdb_t *db, const void *id, size_t n,
                    uint32_t type, const void **p, size_t *length)
{
  const mulib_memtable_record_t *r;
  uint32_t tag = 0;
  int rc;
  if (!db || !type || !p || !length)
    return MULIB_MEMTABLE_EINVAL;
  rc = mulib_memtable_get (&db->records, id, n, &r);
  if (rc)
    return rc;
  if (r->deleted || r->value_size < 4)
    return MULIB_MEMTABLE_NOTFOUND;
  for (unsigned i = 0; i < 4; ++i)
    tag |= (uint32_t)r->value[i] << (8 * i);
  if (tag != type)
    return MULIB_MEMTABLE_NOTFOUND;
  *p = r->value + 4;
  *length = r->value_size - 4;
  return 0;
}
int
mulib_objectdb_delete (mulib_objectdb_t *db, const void *id, size_t n,
                       uint64_t seq)
{
  return db ? mulib_memtable_put (&db->records, id, n, NULL, 0, seq, 1)
            : MULIB_MEMTABLE_EINVAL;
}
void
mulib_objectdb_free (mulib_objectdb_t *db)
{
  if (db)
    mulib_memtable_free (&db->records);
}
#endif

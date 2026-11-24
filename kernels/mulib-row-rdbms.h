/** @file mulib-row-rdbms.h
 * @brief Schema-checked row-oriented in-memory table, not a SQL engine.
 * Define MULIB_IMPL_ROW_RDBMS, DBSCHEMA, DOCUMENTDB and their dependencies.
 * Zero initialize then assign schema (borrowed for table lifetime). */
#ifndef MULIB_ROW_RDBMS_H
#define MULIB_ROW_RDBMS_H
#include "mulib-dbschema.h"
#include "mulib-documentdb.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_row_rdbms
  {
    const mulib_dbschema_t *schema;
    mulib_documentdb_t documents;
  } mulib_row_rdbms_t;
  int mulib_row_rdbms_put (mulib_row_rdbms_t *table, uint64_t id,
                           const mulib_json_t *row, uint64_t sequence);
  mulib_json_t *mulib_row_rdbms_get (const mulib_row_rdbms_t *table,
                                     uint64_t id);
  int mulib_row_rdbms_delete (mulib_row_rdbms_t *table, uint64_t id,
                              uint64_t sequence);
  void mulib_row_rdbms_free (mulib_row_rdbms_t *table);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROW_RDBMS) && !defined(MULIB_ROW_RDBMS_IMPLEMENTED)
#define MULIB_ROW_RDBMS_IMPLEMENTED
static void
mulib_row_rdbms_key_ (uint64_t id, unsigned char key[8])
{
  for (unsigned i = 0; i < 8; ++i)
    key[i] = (unsigned char)(id >> (56 - 8 * i));
}
int
mulib_row_rdbms_put (mulib_row_rdbms_t *t, uint64_t id, const mulib_json_t *v,
                     uint64_t seq)
{
  unsigned char key[8];
  if (!t || !mulib_dbschema_check (t->schema, v))
    return MULIB_MEMTABLE_EINVAL;
  mulib_row_rdbms_key_ (id, key);
  return mulib_documentdb_put (&t->documents, key, 8, v, seq);
}
mulib_json_t *
mulib_row_rdbms_get (const mulib_row_rdbms_t *t, uint64_t id)
{
  unsigned char key[8];
  mulib_row_rdbms_key_ (id, key);
  return t ? mulib_documentdb_get (&t->documents, key, 8) : NULL;
}
int
mulib_row_rdbms_delete (mulib_row_rdbms_t *t, uint64_t id, uint64_t seq)
{
  unsigned char key[8];
  mulib_row_rdbms_key_ (id, key);
  return t ? mulib_documentdb_delete (&t->documents, key, 8, seq)
           : MULIB_MEMTABLE_EINVAL;
}
void
mulib_row_rdbms_free (mulib_row_rdbms_t *t)
{
  if (t)
    {
      mulib_documentdb_free (&t->documents);
      t->schema = NULL;
    }
}
#endif

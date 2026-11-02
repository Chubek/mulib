#define MULIB_IMPL_DOCUMENTDB
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-documentdb.h"
#include <assert.h>
int
main (void)
{
  mulib_documentdb_t db = { 0 };
  mulib_json_t *record = mulib_json_new (MULIB_JSON_OBJECT), *copy;
  assert (record && mulib_json_set (record, "name", mulib_json_string ("Ada")));
  assert (!mulib_documentdb_put (&db, "id", 2, record, 1));
  copy = mulib_documentdb_get (&db, "id", 2);
  assert (copy);
  mulib_json_destroy (copy);
  mulib_json_destroy (record);
  mulib_documentdb_free (&db);
  return 0;
}

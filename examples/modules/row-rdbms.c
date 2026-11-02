#define MULIB_IMPL_ROW_RDBMS
#define MULIB_IMPL_DOCUMENTDB
#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-row-rdbms.h"
#include <assert.h>
int
main (void)
{
  const mulib_dbschema_field_t fields[]
      = { { "name", MULIB_JSON_STRING, 1, 0 } };
  const mulib_dbschema_t schema = { "person", fields, 1 };
  mulib_row_rdbms_t table = { 0 };
  mulib_json_t *record = mulib_json_new (MULIB_JSON_OBJECT), *copy;
  table.schema = &schema;
  assert (record && mulib_json_set (record, "name", mulib_json_string ("Ada")));
  assert (!mulib_row_rdbms_put (&table, 1, record, 1));
  copy = mulib_row_rdbms_get (&table, 1);
  assert (copy);
  mulib_json_destroy (copy);
  mulib_json_destroy (record);
  mulib_row_rdbms_free (&table);
  return 0;
}

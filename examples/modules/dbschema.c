#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dbschema.h"
#include <assert.h>
int
main (void)
{
  const mulib_dbschema_field_t fields[]
      = { { "name", MULIB_JSON_STRING, 1, 0 } };
  const mulib_dbschema_t schema = { "person", fields, 1 };
  mulib_json_t *record = mulib_json_parse ("{\"name\":\"Ada\"}", 14, NULL);
  assert (record && mulib_dbschema_check (&schema, record));
  mulib_json_destroy (record);
  return 0;
}

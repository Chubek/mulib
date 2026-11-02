#define MULIB_IMPL_MUIDL
#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-muidl.h"
#include <assert.h>
int
main (void)
{
  const mulib_dbschema_field_t fields[]
      = { { "name", MULIB_JSON_STRING, 1, 0 } };
  const mulib_dbschema_t schema = { "person", fields, 1 };
  mulib_buffer_t source = { 0 };
  assert (mulib_muidl_generate (&schema, &source));
  assert (source.length
          && fwrite (source.data, 1, source.length, stdout) == source.length);
  mulib_buffer_free (&source);
  return 0;
}

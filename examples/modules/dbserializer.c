#define MULIB_IMPL_DBSERIALIZER
#define MULIB_IMPL_BSON
#define MULIB_IMPL_CBOR
#define MULIB_IMPL_MESSAGEPACK
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dbserializer.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t wire = { 0 };
  mulib_json_t *value = mulib_json_bool (1);
  assert (value
          && mulib_dbserializer_write (MULIB_DBFORMAT_CBOR, value, &wire));
  assert (wire.length == 1 && wire.data[0] == 0xf5);
  mulib_json_destroy (value);
  mulib_buffer_free (&wire);
  return 0;
}

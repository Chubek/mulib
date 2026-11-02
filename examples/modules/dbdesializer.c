#define MULIB_IMPL_DBDESIALIZER
#define MULIB_IMPL_DBSERIALIZER
#define MULIB_IMPL_BSON
#define MULIB_IMPL_CBOR
#define MULIB_IMPL_MESSAGEPACK
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dbdesializer.h"
#include <assert.h>
int
main (void)
{
  const unsigned char wire[] = { 0xf5 };
  mulib_json_t *value
      = mulib_dbdeserializer_read (MULIB_DBFORMAT_CBOR, wire, 1);
  assert (value && mulib_json_typeof (value) == MULIB_JSON_BOOL
          && mulib_json_boolean (value));
  mulib_json_destroy (value);
  return 0;
}

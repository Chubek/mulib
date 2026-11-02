#define MULIB_IMPL_BSON
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-bson.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t wire = { 0 };
  mulib_json_t *value = mulib_json_new (MULIB_JSON_OBJECT), *copy;
  assert (value
          && mulib_json_set (value, "hello", mulib_json_string ("world")));
  assert (mulib_bson_encode (value, &wire));
  copy = mulib_bson_decode (wire.data, wire.length);
  assert (copy
          && !strcmp (mulib_json_text (mulib_json_get (copy, "hello"), NULL),
                      "world"));
  mulib_json_destroy (copy);
  mulib_json_destroy (value);
  mulib_buffer_free (&wire);
  return 0;
}

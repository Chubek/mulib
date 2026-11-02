#define MULIB_IMPL_MESSAGEPACK
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-messagepack.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t wire = { 0 };
  mulib_json_t *value = mulib_json_string ("hello"), *copy;
  assert (value && mulib_messagepack_encode (value, &wire));
  copy = mulib_messagepack_decode (wire.data, wire.length);
  assert (copy && !strcmp (mulib_json_text (copy, NULL), "hello"));
  mulib_json_destroy (copy);
  mulib_json_destroy (value);
  mulib_buffer_free (&wire);
  return 0;
}

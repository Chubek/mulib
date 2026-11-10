#define MULIB_IMPL_DATUM
#include "mulib-datum.h"
#include <assert.h>
int
main (void)
{
  mulib_datum_t value = { 0 }, copy = { 0 };
  assert (mulib_datum_set_bytes (&value, "a\0b", 3)
          && mulib_datum_clone (&copy, &value));
  mulib_datum_free (&value);
  assert (copy.value.bytes.length == 3 && !memcmp (copy.value.bytes.data, "a\0b", 3));
  mulib_datum_free (&copy);
  return 0;
}

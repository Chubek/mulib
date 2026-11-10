#define MULIB_IMPL_DATUM
#define MULIB_IMPL_RECORD
#include "mulib-record.h"
#include <assert.h>
int
main (void)
{
  mulib_record_t row = { 0 };
  mulib_datum_t value = { 0 };
  assert (mulib_datum_set_bytes (&value, "Ada", 3)
          && mulib_record_set (&row, "name", &value));
  mulib_datum_free (&value);
  assert (mulib_record_get (&row, "name")->value.bytes.length == 3);
  mulib_record_free (&row);
  return 0;
}

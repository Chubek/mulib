#define MULIB_IMPL_DATUM
#define MULIB_IMPL_RECORD
#define MULIB_IMPL_RECORDBATCH
#include "mulib-recordbatch.h"
#include <assert.h>
int
main (void)
{
  mulib_record_t row = { 0 };
  mulib_recordbatch_t batch = { 0 };
  mulib_datum_t value = { 0 };
  assert (mulib_datum_set_i64 (&value, 42) && mulib_record_set (&row, "id", &value));
  assert (mulib_recordbatch_append (&batch, &row));
  mulib_record_free (&row);
  assert (mulib_record_get (&batch.items[0], "id")->value.integer == 42);
  mulib_recordbatch_free (&batch);
  mulib_datum_free (&value);
  return 0;
}

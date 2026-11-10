#define MULIB_IMPL_DATUM
#define MULIB_IMPL_RECORD
#define MULIB_IMPL_ETL_TRANSFORM
#include "mulib-etl-transform.h"
#include <assert.h>
static int
project (const mulib_record_t *in, mulib_record_t *out, void *ctx)
{
  (void)ctx;
  const mulib_datum_t *id = mulib_record_get (in, "id");
  return id ? (mulib_record_set (out, "id", id) ? 1 : -1) : 0;
}
int
main (void)
{
  mulib_record_t in = { 0 }, out = { 0 };
  mulib_datum_t id = { 0 };
  assert (mulib_datum_set_i64 (&id, 42) && mulib_record_set (&in, "id", &id));
  assert (mulib_etl_transform_apply (&in, &out, project, NULL) == 1 && out.count == 1);
  mulib_record_free (&in);
  mulib_record_free (&out);
  mulib_datum_free (&id);
  return 0;
}

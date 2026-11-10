#define MULIB_IMPL_DATUM
#define MULIB_IMPL_RECORD
#define MULIB_IMPL_RECORDBATCH
#define MULIB_IMPL_QUERYEXPR
#define MULIB_IMPL_ETL_TRANSFORM
#define MULIB_IMPL_ITERATOR
#define MULIB_IMPL_BLOCKDEV
#include "mulib-recordbatch.h"
#include "mulib-queryexpr.h"
#include "mulib-etl-transform.h"
#include "mulib-iterator.h"
#include "mulib-blockdev.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int
transform (const mulib_record_t *in, mulib_record_t *out, void *ctx)
{
  assert (mulib_record_clone (out, in));
  return *(int *)ctx;
}
static int
next (void *ctx, const void **out)
{
  int *n = ctx;
  if (*n == 3)
    return 0;
  ++*n;
  *out = n;
  return 1;
}
static void
close_it (void *ctx)
{
  *(int *)ctx = 99;
}
static int
read_block (void *ctx, uint64_t block, void *out, size_t n)
{
  if (block >= 2)
    return 0;
  memcpy (out, (unsigned char *)ctx + block * n, n);
  return 1;
}
static int
write_block (void *ctx, uint64_t block, void *in, size_t n)
{
  if (block >= 2)
    return 0;
  memcpy ((unsigned char *)ctx + block * n, in, n);
  return 1;
}
int
main (void)
{
  mulib_datum_t a = { 0 }, b = { 0 };
  const unsigned char raw[] = { 'a', 0, 'b' };
  assert (mulib_datum_set_bytes (&a, raw, 3));
  assert (mulib_datum_clone (&b, &a) && b.value.bytes.data != a.value.bytes.data);
  assert (mulib_queryexpr_compare (&a, &b) == 0);
  assert (mulib_datum_set_bytes (&a, a.value.bytes.data + 1, 2));
  assert (mulib_queryexpr_compare (&a, &b) == -1);
  assert (!mulib_datum_set_bytes (&a, NULL, 1) && a.value.bytes.length == 2);
  assert (mulib_datum_clone (&a, &a));
  mulib_record_t r = { 0 }, copy = { 0 };
  assert (mulib_record_set (&r, "payload", &b));
  for (int i = 0; i < 32; i++)
    {
      char key[20];
      snprintf (key, sizeof (key), "field%d", i);
      assert (mulib_record_set (&r, key, mulib_record_get (&r, "payload")));
    }
  assert (mulib_record_clone (&copy, &r));
  assert (copy.fields[0].value.value.bytes.data != r.fields[0].value.value.bytes.data);
  mulib_recordbatch_t batch = { 0 };
  assert (mulib_recordbatch_append (&batch, &r));
  for (int i = 0; i < 20; i++)
    assert (mulib_recordbatch_append (&batch, &batch.items[0]));
  mulib_record_free (&r);
  assert (mulib_record_get (&batch.items[20], "payload")->value.bytes.length == 3);
  for (int status = -1; status <= 1; status++)
    {
      assert (mulib_etl_transform_apply (&batch.items[0], &copy, transform, &status)
              == status);
      assert (copy.count == 33);
    }
  assert (mulib_etl_transform_apply (&copy, &copy, transform, NULL) == -1);
  assert (mulib_datum_set_i64 (&a, INT64_MIN) && mulib_datum_set_i64 (&b, INT64_MAX));
  assert (mulib_queryexpr_compare (&a, &b) == -1);
  assert (mulib_datum_set_f64 (&b, NAN) && mulib_queryexpr_compare (&b, &b) == 2);
  assert (mulib_queryexpr_compare (&a, &b) == 2);
  mulib_datum_free (&a);
  mulib_datum_free (&b);
  mulib_datum_free (&b);
  mulib_record_free (&copy);
  mulib_recordbatch_free (&batch);
  mulib_recordbatch_free (&batch);
  int state = 0;
  const void *item = NULL;
  mulib_iterator_t it = { &state, next, close_it };
  for (int i = 1; i <= 3; i++)
    assert (mulib_iterator_next (&it, &item) == 1 && *(const int *)item == i);
  assert (mulib_iterator_next (&it, &item) == 0);
  mulib_iterator_close (&it);
  assert (state == 99);
  mulib_iterator_close (&it);
  assert (mulib_iterator_next (&it, &item) == -1);
  unsigned char disk[8] = { 0 }, data[4] = { 1, 2, 3, 4 }, result[4] = { 0 };
  mulib_blockdev_t dev = { disk, 4, read_block, write_block, NULL };
  assert (mulib_blockdev_write (&dev, 1, data)
          && mulib_blockdev_read (&dev, 1, result));
  assert (!memcmp (data, result, 4) && mulib_blockdev_flush (&dev));
  assert (!mulib_blockdev_read (&dev, 2, result)
          && !mulib_blockdev_write (&dev, 0, NULL));
  return 0;
}

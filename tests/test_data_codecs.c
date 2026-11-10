#define MULIB_IMPL_BITPACK
#define MULIB_IMPL_ENCODING
#define MULIB_IMPL_DELTACODEC
#define MULIB_IMPL_DOCID
#define MULIB_IMPL_RLECOL
#define MULIB_IMPL_ZORDER
#include "mulib-bitpack.h"
#include "mulib-encoding.h"
#include "mulib-deltacodec.h"
#include "mulib-docid.h"
#include "mulib-rlecol.h"
#include "mulib-zorder.h"
#include <assert.h>

int
main (void)
{
  unsigned char bytes[512], before[512];
  uint32_t values[127], decoded[127];
  for (unsigned w = 1; w <= 32; ++w)
    {
      uint32_t mask = w == 32 ? UINT32_MAX : (UINT32_C (1) << w) - 1;
      for (size_t i = 0; i < 127; i++)
        values[i] = (uint32_t)(i * 2654435761U) & mask;
      size_t n = mulib_bitpack_bytes (127, w);
      assert (mulib_bitpack_encode (values, 127, w, bytes, n));
      assert (mulib_bitpack_decode (bytes, n, 127, w, decoded));
      assert (!memcmp (values, decoded, sizeof (values)));
      memset (bytes, 0xa5, sizeof (bytes));
      memcpy (before, bytes, sizeof (bytes));
      assert (!mulib_bitpack_encode (values, 127, w, bytes, n - 1));
      assert (!memcmp (bytes, before, sizeof (bytes)));
    }
  assert (mulib_bitpack_encode (NULL, 0, 1, NULL, 0));
  assert (mulib_bitpack_decode (NULL, 0, 0, 32, NULL));
  assert (mulib_bitpack_bytes (SIZE_MAX, 32) == SIZE_MAX);
  assert (mulib_bitpack_bytes (1, 0) == SIZE_MAX);
  values[0] = 8;
  assert (!mulib_bitpack_encode (values, 1, 3, bytes, 1));
  const uint64_t edges[] = { 0, 1, 127, 128, 16383, 16384, UINT64_MAX };
  for (size_t i = 0; i < sizeof (edges) / sizeof (*edges); i++)
    {
      uint64_t x = 42;
      size_t n = mulib_encoding_uvarint (edges[i], bytes);
      assert (mulib_encoding_read_uvarint (bytes, n, &x) == n && x == edges[i]);
      x = 42;
      assert (!mulib_encoding_read_uvarint (bytes, n - 1, &x) && x == 42);
      mulib_docid_encode (edges[i], bytes);
      assert (mulib_docid_decode (bytes) == edges[i]);
      if (i)
        {
          mulib_docid_encode (edges[i - 1], before);
          assert (memcmp (before, bytes, 8) < 0);
        }
    }
  memset (bytes, 0x80, 10);
  bytes[9] = 2;
  uint64_t x = 42;
  assert (!mulib_encoding_read_uvarint (bytes, 10, &x) && x == 42);
  const int64_t signed_edges[] = { INT64_MIN, -123, -1, 0, 1, INT64_MAX };
  for (size_t i = 0; i < sizeof (signed_edges) / sizeof (*signed_edges); i++)
    assert (mulib_encoding_unzigzag (mulib_encoding_zigzag (signed_edges[i]))
            == signed_edges[i]);
  uint64_t nums[] = { 0, 1, 1, 900, UINT64_MAX }, copy[5];
  memcpy (copy, nums, sizeof (nums));
  assert (mulib_deltacodec_encode (nums, 5, nums));
  assert (mulib_deltacodec_decode (nums, 5, nums));
  assert (!memcmp (nums, copy, sizeof (nums)));
  uint64_t bad[] = { UINT64_MAX, 1 }, output[] = { 17, 18 };
  assert (!mulib_deltacodec_decode (bad, 2, output) && output[0] == 17
          && output[1] == 18);
  assert (!mulib_deltacodec_encode (bad, 2, output) && output[0] == 17);
  assert (mulib_deltacodec_encode (NULL, 0, NULL));
  mulib_rlecol_run_t runs[5] = { { 99, 99 } };
  assert (mulib_rlecol_encode (copy, 5, runs, 1) == SIZE_MAX && runs[0].value == 99);
  size_t count = mulib_rlecol_encode (copy, 5, runs, 5);
  assert (count == 4 && runs[1].count == 2);
  assert (mulib_rlecol_decode (runs, count, nums, 5) == 5
          && !memcmp (nums, copy, sizeof (nums)));
  runs[0].count = 0;
  assert (mulib_rlecol_decode (runs, count, nums, 5) == SIZE_MAX);
  for (uint32_t i = 0; i < 1000; i++)
    {
      uint32_t a = i * 2654435761U, b = ~a, aa, bb;
      mulib_zorder_decode (mulib_zorder_encode (a, b), &aa, &bb);
      assert (a == aa && b == bb);
    }
  assert (mulib_zorder_encode (UINT32_MAX, UINT32_MAX) == UINT64_MAX);
  return 0;
}

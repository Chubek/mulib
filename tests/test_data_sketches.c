#define MULIB_IMPL_COUNTMIN
#define MULIB_IMPL_HLL
#define MULIB_IMPL_MINHASH
#define MULIB_IMPL_LSH
#define MULIB_IMPL_SIMHASH
#define MULIB_IMPL_JOINFILTER
#include "mulib-countmin.h"
#include "mulib-hll.h"
#include "mulib-minhash.h"
#include "mulib-lsh.h"
#include "mulib-simhash.h"
#include "mulib-joinfilter.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>

int
main (void)
{
  mulib_countmin_t cms = { 0 };
  mulib_joinfilter_t bloom = { 0 };
  mulib_hll_t h = { 0 }, half = { 0 }, all = { 0 };
  assert (!mulib_countmin_init (&cms, SIZE_MAX, 2));
  assert (mulib_countmin_init (&cms, 256, 5) && !mulib_countmin_init (&cms, 256, 5));
  assert (mulib_joinfilter_init (&bloom, 8192, 42));
  assert (!mulib_joinfilter_maybe (&bloom, "absent", 6));
  for (unsigned i = 0; i < 1000; i++)
    {
      char key[16];
      int n = snprintf (key, sizeof (key), "key%u", i);
      assert (mulib_countmin_add (&cms, key, (size_t)n, i + 1));
      mulib_joinfilter_add (&bloom, key, (size_t)n);
      mulib_hll_add (i < 500 ? &h : &half, key, (size_t)n);
      mulib_hll_add (&all, key, (size_t)n);
    }
  for (unsigned i = 0; i < 1000; i++)
    {
      char key[16];
      int n = snprintf (key, sizeof (key), "key%u", i);
      assert (mulib_countmin_estimate (&cms, key, (size_t)n) >= i + 1);
      assert (mulib_joinfilter_maybe (&bloom, key, (size_t)n));
    }
  assert (mulib_countmin_add (&cms, "x", 1, UINT64_MAX));
  assert (mulib_countmin_add (&cms, "x", 1, 1)
          && mulib_countmin_estimate (&cms, "x", 1) == UINT64_MAX);
  mulib_hll_merge (&h, &half);
  assert (!memcmp (&h, &all, sizeof (h)));
  double estimate = mulib_hll_estimate (&h);
  assert (estimate > 850 && estimate < 1150);
  mulib_hll_merge (&h, &h);
  assert (mulib_hll_estimate (&h) == estimate);
  uint64_t a[128], b[128], copy[128];
  mulib_minhash_init (a, 128);
  mulib_minhash_init (b, 128);
  mulib_minhash_add (a, 128, "a", 1);
  mulib_minhash_add (a, 128, "b", 1);
  mulib_minhash_add (b, 128, "b", 1);
  mulib_minhash_add (b, 128, "a", 1);
  assert (mulib_minhash_similarity (a, b, 128) == 1);
  memcpy (copy, a, sizeof (a));
  mulib_minhash_add (a, 128, "a", 1);
  assert (!memcmp (a, copy, sizeof (a)));
  assert (mulib_lsh_band (a, 32, 16) == mulib_lsh_band (b, 32, 16));
  assert (!mulib_lsh_band (a, SIZE_MAX, 2));
  uint64_t hashes[] = { UINT64_MAX, UINT64_MAX, 0, 0, 0 };
  double weights[] = { DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX };
  assert (mulib_simhash64 (hashes, weights, 5) == 0);
  assert (mulib_simhash64 (hashes, weights, 3) == UINT64_MAX);
  assert (mulib_simhash_distance (0, UINT64_MAX) == 64);
  weights[0] = NAN;
  assert (mulib_simhash64 (hashes, weights, 5) == 0);
  mulib_joinfilter_free (&bloom);
  mulib_joinfilter_free (&bloom);
  mulib_countmin_free (&cms);
  return 0;
}

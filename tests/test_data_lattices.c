#define MULIB_IMPL_FCA
#define MULIB_IMPL_GALOIS
#define MULIB_IMPL_NEXTCLOSURE
#define MULIB_IMPL_CONCEPTLATTICE
#define MULIB_IMPL_CLOSEDITEMSET
#define MULIB_IMPL_LATTICEQUERY
#define MULIB_IMPL_LATTICE
#define MULIB_IMPL_SEMILATTICE
#define MULIB_IMPL_SEMIRING
#include "mulib-galois.h"
#include "mulib-latticequery.h"
#include "mulib-closeditemset.h"
#include "mulib-lattice.h"
#include "mulib-semilattice.h"
#include "mulib-semiring.h"
#include <assert.h>

static int
visit (uint64_t intent, uint64_t extent, void *ctx)
{
  (void)intent;
  assert (extent);
  ++*(size_t *)ctx;
  return 1;
}
int
main (void)
{
  /* Exhaust all 512 binary contexts with three objects and attributes.
   * Enumerated intents must equal an independent powerset closure oracle. */
  for (unsigned bits = 0; bits < 512; bits++)
    {
      mulib_fca_t c;
      assert (mulib_fca_init (&c, 3, 3));
      for (size_t o = 0; o < 3; o++)
        for (size_t a = 0; a < 3; a++)
          assert (mulib_fca_set (&c, o, a, (bits >> (3 * o + a)) & 1));
      unsigned expected = 0;
      size_t supported = 0;
      for (unsigned a = 0; a < 8; a++)
        {
          uint64_t extent = 0, intent = 7;
          for (size_t o = 0; o < 3; o++)
            if ((c.rows[o] & a) == a)
              {
                extent |= UINT64_C (1) << o;
                intent &= c.rows[o];
              }
          assert (mulib_fca_extent (&c, a) == extent
                  && mulib_fca_closure (&c, a) == intent);
          assert (mulib_galois_objects (&c, a) == extent
                  && mulib_galois_attributes (&c, extent) == intent);
          assert ((mulib_galois_object_closure (&c, extent) & extent) == extent);
          if (intent == a)
            {
              expected |= 1u << a;
              supported += extent != 0;
            }
        }
      mulib_concept_t out[8];
      size_t n = mulib_conceptlattice_enumerate (&c, out, 8);
      unsigned actual = 0;
      assert (n > 0 && n <= 8);
      for (size_t i = 0; i < n; i++)
        {
          assert (!(actual & (1u << out[i].intent)));
          actual |= 1u << out[i].intent;
        }
      assert (actual == expected);
      mulib_concept_t sentinel = { 99, 99 };
      assert (mulib_conceptlattice_enumerate (&c, &sentinel, 0) == SIZE_MAX
              && sentinel.intent == 99);
      size_t seen = 0;
      assert (mulib_closeditemset_enumerate (&c, 1, visit, &seen) == supported
              && seen == supported);
      size_t idx[8];
      size_t matches = mulib_latticequery_filter (out, n, 1, 0, idx, 8), want = 0;
      for (size_t i = 0; i < n; i++)
        if (out[i].intent & 1)
          {
            assert (idx[want] == i);
            want++;
          }
      assert (matches == want);
      for (size_t i = 0; i < n; i++)
        assert (mulib_conceptlattice_leq (out[i], out[i]));
    }
  mulib_fca_t c;
  assert (mulib_fca_init (&c, 64, 64));
  assert (mulib_fca_set (&c, 63, 63, 1));
  assert (mulib_fca_extent (&c, UINT64_C (1) << 63) == (UINT64_C (1) << 63));
  assert (mulib_fca_init (&c, 0, 0));
  mulib_concept_t only;
  assert (mulib_conceptlattice_enumerate (&c, &only, 1) == 1 && only.intent == 0
          && only.extent == 0);
  c.objects = 65;
  assert (!mulib_fca_set (&c, 64, 0, 1));
  assert (mulib_conceptlattice_enumerate (&c, &only, 1) == SIZE_MAX);
  mulib_lattice_t l = { 0 };
  l.attributes = 4;
  assert (mulib_lattice_add_rule (&l, 1, 2) && mulib_lattice_add_rule (&l, 2, 4));
  assert (mulib_lattice_closure (&l, 1) == 7 && mulib_lattice_join (&l, 1, 8) == 15);
  assert (mulib_lattice_meet (&l, 1, 2) == 6);
  assert (!mulib_lattice_add_rule (&l, 16, 1));
  for (uint64_t a = 0; a < 16; a++)
    for (uint64_t b = 0; b < 16; b++)
      {
        assert (mulib_semilattice_leq (a, mulib_semilattice_join (a, b)));
        assert (mulib_semilattice_leq (mulib_semilattice_meet (a, b), a));
        uint64_t x = mulib_lattice_closure (&l, a);
        assert ((x & a) == a && mulib_lattice_closure (&l, x) == x);
      }
  mulib_lattice_free (&l);
  for (int k = 0; k < 4; k++)
    {
      mulib_semiring_kind_t kind = (mulib_semiring_kind_t)k;
      double x = k == 0 ? 1 : 4;
      double zero = mulib_semiring_zero (kind), one = mulib_semiring_one (kind);
      assert (mulib_semiring_add (kind, zero, x) == x
              && mulib_semiring_mul (kind, one, x) == x);
      assert (mulib_semiring_mul (kind, zero, x) == zero);
    }
  assert (mulib_semiring_mul (MULIB_SEMIRING_MIN_PLUS, INFINITY, -INFINITY)
          == INFINITY);
  assert (mulib_semiring_mul (MULIB_SEMIRING_NATURAL, 0, INFINITY) == 0);
  assert (isnan (mulib_semiring_add (MULIB_SEMIRING_NATURAL, -1, 1)));
  assert (isnan (mulib_semiring_one ((mulib_semiring_kind_t)-1)));
  return 0;
}

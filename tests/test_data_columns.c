#define MULIB_IMPL_COLUMNAR
#define MULIB_IMPL_COLUMNSTATS
#define MULIB_IMPL_QUERYAGGREGATE
#define MULIB_IMPL_MATVIEW
#define MULIB_IMPL_SCANVEC
#define MULIB_IMPL_ZONEINDEX
#define MULIB_IMPL_QUERYJOIN
#include "mulib-columnar.h"
#include "mulib-columnstats.h"
#include "mulib-queryaggregate.h"
#include "mulib-matview.h"
#include "mulib-scanvec.h"
#include "mulib-zoneindex.h"
#include "mulib-queryjoin.h"
#include <assert.h>
#include <float.h>

int
main (void)
{
  mulib_columnar_t c = { 0 };
  mulib_columnstats_t stats = { 0 };
  for (int i = 0; i < 100; i++)
    {
      assert (mulib_columnar_append (&c, i, i % 3));
      assert (mulib_columnstats_add (&stats, i, i % 3));
      double x;
      int valid;
      assert (mulib_columnar_get (&c, (size_t)i, &x, &valid));
      assert (valid == !!(i % 3) && x == (valid ? i : 0));
    }
  assert (stats.values == 66 && stats.nulls == 34 && stats.min == 1 && stats.max == 98);
  assert (mulib_columnstats_may_match (&stats, 98, 100));
  assert (!mulib_columnstats_may_match (&stats, 99, 100));
  assert (!mulib_columnar_append (&c, INFINITY, 1) && c.count == 100);
  assert (mulib_columnar_append (&c, NAN, 0));
  assert (!mulib_columnar_get (&c, 101, NULL, NULL));
  mulib_columnar_free (&c);
  mulib_columnar_free (&c);
  mulib_queryaggregate_t a = { 0 }, b = { 0 };
  assert (mulib_queryaggregate_add (&a, DBL_MAX));
  assert (!mulib_queryaggregate_add (&a, DBL_MAX) && a.count == 1);
  assert (mulib_queryaggregate_add (&a, -DBL_MAX) && a.mean == 0);
  assert (mulib_queryaggregate_add (&b, 6));
  assert (mulib_queryaggregate_merge (&a, &b) && a.count == 3 && a.mean == 2);
  assert (!mulib_queryaggregate_add (&a, NAN) && a.sum == 6);
  a = (mulib_queryaggregate_t){ 0 };
  b = a;
  assert (mulib_queryaggregate_add (&a, DBL_MAX)
          && mulib_queryaggregate_add (&b, -DBL_MAX));
  assert (mulib_queryaggregate_merge (&a, &b) && a.mean == 0);
  mulib_matview_t view = { 0 };
  assert (!mulib_matview_delete (&view, 1) && isnan (mulib_matview_avg (&view)));
  assert (mulib_matview_insert (&view, 3) && mulib_matview_insert (&view, 5));
  assert (mulib_matview_avg (&view) == 4 && mulib_matview_delete (&view, 3));
  assert (mulib_matview_delete (&view, 5) && view.sum == 0);
  double values[] = { -1, 0, 1, NAN, INFINITY };
  size_t idx[5] = { 99 };
  assert (mulib_scanvec_double (values, 5, MULIB_SCAN_GE, 0, idx, 1) == SIZE_MAX
          && idx[0] == 99);
  assert (mulib_scanvec_double (values, 5, MULIB_SCAN_GE, 0, idx, 5) == 3
          && idx[2] == 4);
  assert (mulib_scanvec_double (values, 5, MULIB_SCAN_NE, NAN, idx, 5) == 0);
  assert (mulib_scanvec_double (values, 5, (mulib_scan_predicate_t)-1, 0, idx, 5)
          == SIZE_MAX);
  mulib_zoneindex_zone_t zones[] = { { 5, 10 }, { -2, 1 }, { 1, 2 } };
  assert (mulib_zoneindex_filter (zones, 3, 1, 1, idx, 5) == 2 && idx[0] == 1
          && idx[1] == 2);
  idx[0] = 99;
  assert (mulib_zoneindex_filter (zones, 3, 1, 1, idx, 1) == SIZE_MAX && idx[0] == 99);
  zones[2].low = NAN;
  assert (mulib_zoneindex_filter (zones, 3, 1, 1, idx, 5) == SIZE_MAX);
  mulib_queryjoin_row_t left[] = { { 1, 10 }, { 2, 20 }, { 2, 21 }, { 4, 40 } },
                        right[] = { { 2, 30 }, { 2, 31 }, { 3, 32 } };
  mulib_queryjoin_hit_t pairs[4] = { { 99, 99 } };
  assert (mulib_queryjoin_merge (left, 4, right, 3, pairs, 3) == SIZE_MAX
          && pairs[0].left_row == 99);
  assert (mulib_queryjoin_merge (left, 4, right, 3, pairs, 4) == 4);
  for (size_t i = 0; i < 4; i++)
    assert (pairs[i].left_row == 20 + i / 2 && pairs[i].right_row == 30 + i % 2);
  left[0].key = 5;
  assert (mulib_queryjoin_merge (left, 4, right, 3, pairs, 4) == SIZE_MAX);
  return 0;
}

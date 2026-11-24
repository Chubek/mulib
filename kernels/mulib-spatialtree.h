/** @file mulib-spatialtree.h
 * @brief Owned static balanced 2D k-d tree for inclusive rectangle searches.
 * Define MULIB_IMPL_SPATIALTREE once. Coordinates must be finite. */
#ifndef MULIB_SPATIALTREE_H
#define MULIB_SPATIALTREE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_spatialtree_point
  {
    double x, y;
    uint64_t id;
  } mulib_spatialtree_point_t;
  typedef struct mulib_spatialtree
  {
    mulib_spatialtree_point_t *points;
    size_t count;
  } mulib_spatialtree_t;
  typedef int (*mulib_spatialtree_visit_fn) (const mulib_spatialtree_point_t *,
                                             void *);
  /** @brief Copy/build into uninitialized output, 1 success. Duplicate
   * positions allowed. Median partition uses in-place sorting per level (O(n
   * log^2 n) construction). */
  int mulib_spatialtree_build (const mulib_spatialtree_point_t *points,
                               size_t count, mulib_spatialtree_t *out);
  /** @brief Visit inclusive rectangle. Callback nonzero continues, zero stops.
   * Returns count including stopping callback, SIZE_MAX invalid; do not mutate
   * tree. */
  size_t mulib_spatialtree_query (const mulib_spatialtree_t *tree, double xmin,
                                  double ymin, double xmax, double ymax,
                                  mulib_spatialtree_visit_fn visit,
                                  void *context);
  /** @brief Free/reset. */
  void mulib_spatialtree_free (mulib_spatialtree_t *tree);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPATIALTREE) && !defined(MULIB_SPATIALTREE_IMPLEMENTED)
#define MULIB_SPATIALTREE_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int
mulib_spatialtree_x_ (const void *a, const void *b)
{
  const mulib_spatialtree_point_t *p = (const mulib_spatialtree_point_t *)a,
                                  *q = (const mulib_spatialtree_point_t *)b;
  return (p->x > q->x) - (p->x < q->x);
}
static int
mulib_spatialtree_y_ (const void *a, const void *b)
{
  const mulib_spatialtree_point_t *p = (const mulib_spatialtree_point_t *)a,
                                  *q = (const mulib_spatialtree_point_t *)b;
  return (p->y > q->y) - (p->y < q->y);
}
static void
mulib_spatialtree_build_ (mulib_spatialtree_point_t *p, size_t n,
                          unsigned axis)
{
  size_t m;
  if (!n)
    return;
  m = n / 2;
  qsort (p, n, sizeof (*p),
         axis ? mulib_spatialtree_y_ : mulib_spatialtree_x_);
  mulib_spatialtree_build_ (p, m, !axis);
  mulib_spatialtree_build_ (p + m + 1, n - m - 1, !axis);
}
int
mulib_spatialtree_build (const mulib_spatialtree_point_t *p, size_t n,
                         mulib_spatialtree_t *out)
{
  mulib_spatialtree_t t = { NULL, 0 };
  size_t i;
  if (!out || (!p && n) || n > SIZE_MAX / sizeof (*p))
    return 0;
  for (i = 0; i < n; ++i)
    if (!isfinite (p[i].x) || !isfinite (p[i].y))
      return 0;
  if (n)
    {
      t.points = (mulib_spatialtree_point_t *)malloc (n * sizeof (*p));
      if (!t.points)
        return 0;
      memcpy (t.points, p, n * sizeof (*p));
      mulib_spatialtree_build_ (t.points, n, 0);
    }
  t.count = n;
  *out = t;
  return 1;
}
static int
mulib_spatialtree_query_ (const mulib_spatialtree_point_t *p, size_t n,
                          unsigned a, const double box[4],
                          mulib_spatialtree_visit_fn f, void *ctx,
                          size_t *count)
{
  size_t m;
  double split;
  if (!n)
    return 1;
  m = n / 2;
  split = a ? p[m].y : p[m].x;
  if (p[m].x >= box[0] && p[m].x <= box[2] && p[m].y >= box[1]
      && p[m].y <= box[3])
    {
      ++*count;
      if (!f (p + m, ctx))
        return 0;
    }
  if (box[a] <= split
      && !mulib_spatialtree_query_ (p, m, !a, box, f, ctx, count))
    return 0;
  if (box[a + 2] >= split
      && !mulib_spatialtree_query_ (p + m + 1, n - m - 1, !a, box, f, ctx,
                                    count))
    return 0;
  return 1;
}
size_t
mulib_spatialtree_query (const mulib_spatialtree_t *t, double x, double y,
                         double X, double Y, mulib_spatialtree_visit_fn f,
                         void *ctx)
{
  double b[4] = { x, y, X, Y };
  size_t count = 0;
  if (!t || !f || !isfinite (x) || !isfinite (y) || !isfinite (X)
      || !isfinite (Y) || x > X || y > Y)
    return SIZE_MAX;
  mulib_spatialtree_query_ (t->points, t->count, 0, b, f, ctx, &count);
  return count;
}
void
mulib_spatialtree_free (mulib_spatialtree_t *t)
{
  if (t)
    {
      free (t->points);
      t->points = NULL;
      t->count = 0;
    }
}
#endif

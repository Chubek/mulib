/** @file mulib-vangage_point_trees.h
 * @brief Balanced metric vantage-point trees over copied fixed-stride points.
 * Define MULIB_IMPL_VANGAGE_POINT_TREES; link libm. Filename retained.
 */
#ifndef MULIB_VANGAGE_POINT_TREES_H
#define MULIB_VANGAGE_POINT_TREES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_vp_metric_fn)(const void *a, const void *b, void *context);
typedef int (*mulib_vp_visit_fn)(size_t original_index, const void *point, double distance, void *context);
typedef struct mulib_vp_node mulib_vp_node_t;
typedef struct mulib_vp_tree {
  unsigned char *points;
  mulib_vp_node_t *nodes;
  size_t size, stride;
  mulib_vp_metric_fn metric;
  void *context;
} mulib_vp_tree_t;
/** Initialize uninitialized storage; zero initialization also works. */
void mulib_vp_tree_init(mulib_vp_tree_t *tree);
/** Copy points and build a balanced tree. Metric/context are borrowed; metric
 * must be deterministic, finite, nonnegative and obey the triangle inequality.
 * Returns 0 on invalid/OOM/bad metric, preserving the old tree. */
int mulib_vp_tree_build(mulib_vp_tree_t *tree, const void *points, size_t n, size_t stride,
                        mulib_vp_metric_fn metric, void *context);
/** Radius query. Nonzero callback continues; no mutation from callbacks.
 * Return calls including stopping call, SIZE_MAX on invalid input/bad metric.
 * Bad metric may be reported after callbacks. Original indices survive building. */
size_t mulib_vp_tree_search(const mulib_vp_tree_t *tree, const void *query, double radius,
                           mulib_vp_visit_fn visit, void *context);
/** Release copied points and nodes, reset. */
void mulib_vp_tree_free(mulib_vp_tree_t *tree);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VANGAGE_POINT_TREES) && !defined(MULIB_VANGAGE_POINT_TREES_IMPLEMENTED)
#define MULIB_VANGAGE_POINT_TREES_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_vp_node { size_t point, left, right; double threshold; };
typedef struct mulib_vp_record_ { size_t index; double distance; } mulib_vp_record_;
void mulib_vp_tree_init(mulib_vp_tree_t *t) { if (t) memset(t, 0, sizeof(*t)); }
void mulib_vp_tree_free(mulib_vp_tree_t *t)
{ if (t) { free(t->points); free(t->nodes); mulib_vp_tree_init(t); } }
static int mulib_vp_compare_(const void *a, const void *b)
{
  const mulib_vp_record_ *x = (const mulib_vp_record_ *)a, *y = (const mulib_vp_record_ *)b;
  if (x->distance != y->distance) return x->distance < y->distance ? -1 : 1;
  return x->index < y->index ? -1 : x->index != y->index;
}
static int mulib_vp_build_(mulib_vp_tree_t *t, mulib_vp_record_ *r, size_t lo, size_t hi)
{
  mulib_vp_node_t *node = &t->nodes[lo];
  size_t mid = lo + 1 + (hi - lo - 1) / 2;
  node->point = r[lo].index; node->left = node->right = SIZE_MAX; node->threshold = 0;
  for (size_t i = lo + 1; i < hi; ++i) {
    r[i].distance = t->metric(t->points + node->point * t->stride,
                             t->points + r[i].index * t->stride, t->context);
    if (!isfinite(r[i].distance) || r[i].distance < 0) return 0;
  }
  qsort(r + lo + 1, hi - lo - 1, sizeof(*r), mulib_vp_compare_);
  if (mid < hi) node->threshold = r[mid].distance;
  if (lo + 1 < mid) { node->left = lo + 1; if (!mulib_vp_build_(t, r, lo + 1, mid)) return 0; }
  if (mid < hi) { node->right = mid; if (!mulib_vp_build_(t, r, mid, hi)) return 0; }
  return 1;
}
int mulib_vp_tree_build(mulib_vp_tree_t *t, const void *points, size_t n, size_t stride,
                        mulib_vp_metric_fn metric, void *ctx)
{
  mulib_vp_tree_t next = {0};
  mulib_vp_record_ *records;
  if (!t || !metric || (!points && n) || (n && !stride)
      || (stride && n > SIZE_MAX / stride) || n > SIZE_MAX / sizeof(*records)
      || n > SIZE_MAX / sizeof(*next.nodes)) return 0;
  next.size = n; next.stride = stride; next.metric = metric; next.context = ctx;
  if (!n) { mulib_vp_tree_free(t); *t = next; return 1; }
  next.points = (unsigned char *)malloc(n * stride);
  next.nodes = (mulib_vp_node_t *)malloc(n * sizeof(*next.nodes));
  records = (mulib_vp_record_ *)malloc(n * sizeof(*records));
  if (!next.points || !next.nodes || !records) { free(records); mulib_vp_tree_free(&next); return 0; }
  memcpy(next.points, points, n * stride);
  for (size_t i = 0; i < n; ++i) { records[i].index = i; records[i].distance = 0; }
  if (!mulib_vp_build_(&next, records, 0, n)) { free(records); mulib_vp_tree_free(&next); return 0; }
  free(records); mulib_vp_tree_free(t); *t = next; return 1;
}
static int mulib_vp_search_(const mulib_vp_tree_t *t, size_t index, const void *q, double radius,
                            mulib_vp_visit_fn fn, void *ctx, size_t *calls)
{
  const mulib_vp_node_t *n = &t->nodes[index];
  const void *point = t->points + n->point * t->stride;
  double d = t->metric(q, point, t->context);
  int status;
  if (!isfinite(d) || d < 0) return -1;
  if (d <= radius) { ++*calls; if (!fn(n->point, point, d, ctx)) return 0; }
  if (n->left != SIZE_MAX && (long double)d - radius <= n->threshold) {
    status = mulib_vp_search_(t, n->left, q, radius, fn, ctx, calls); if (status != 1) return status;
  }
  if (n->right != SIZE_MAX && (long double)d + radius >= n->threshold)
    return mulib_vp_search_(t, n->right, q, radius, fn, ctx, calls);
  return 1;
}
size_t mulib_vp_tree_search(const mulib_vp_tree_t *t, const void *q, double radius,
                           mulib_vp_visit_fn fn, void *ctx)
{
  size_t calls = 0;
  if (!t || !q || !fn || !t->metric || !isfinite(radius) || radius < 0) return SIZE_MAX;
  if (t->size && mulib_vp_search_(t, 0, q, radius, fn, ctx, &calls) < 0) return SIZE_MAX;
  return calls;
}
#ifdef __cplusplus
}
#endif
#endif

#define MULIB_IMPL_VANGAGE_POINT_TREES
#include "mulib-vangage_point_trees.h"
#include <assert.h>
#include <math.h>
static double metric(const void *a, const void *b, void *ctx) { (void)ctx; return fabs(*(const double *)a - *(const double *)b); }
static int visit(size_t index, const void *p, double d, void *ctx) { (void)index; (void)p; (void)d; ++*(size_t *)ctx; return 1; }
int main(void) {
  mulib_vp_tree_t tree = {0}; double points[] = {1, 3, 5}, query = 3; size_t count = 0;
  assert(mulib_vp_tree_build(&tree, points, 3, sizeof(double), metric, NULL));
  assert(mulib_vp_tree_search(&tree, &query, 0, visit, &count) == 1 && count == 1);
  mulib_vp_tree_free(&tree); return 0;
}

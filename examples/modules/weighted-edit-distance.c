#define MULIB_IMPL_WEIGHTED_EDIT_DISTANCE
#define MULIB_IMPL_WEIGHED_EDIT_DISTANE
#include "mulib-weighted-edit-distance.h"
#include <assert.h>
static double cost(mulib_weighted_edit_operation_t operation, uint32_t a, uint32_t b, void *user) {
  (void)user;
  return operation == MULIB_WEIGHTED_SUBSTITUTE && a == 'q' && b == 'k' ? 0.25 : 1;
}
int main(void) {
  const uint32_t a[] = {'q', 'i', 't'}, b[] = {'k', 'i', 't'}; double distance;
  assert(mulib_weighted_edit_distance_ex(a, 3, b, 3, cost, NULL, 0, &distance) && distance == 0.25);
  assert(mulib_weighted_edit_distance("a", 1, "b", 1, 1, 1, 0.5) == 0.5); return 0;
}

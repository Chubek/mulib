#define MULIB_IMPL_WEIGHED_EDIT_DISTANE
#include "mulib-weighed_edit_distane.h"
#include <assert.h>
int main(void) {
  assert(mulib_weighted_edit_distance("cat", 3, "cats", 4, 2, 3, 4) == 2);
  return 0;
}

#define MULIB_IMPL_LEVENSHTEIN
#define MULIB_IMPL_EDIT_DISTANCE
#include "mulib-edit_distance.h"
#include <assert.h>
int main(void) {
  assert(mulib_edit_distance("kitten", "sitting") == 3);
  assert(mulib_edit_distance_n("a\0b", 3, "a\0c", 3) == 1);
  return 0;
}

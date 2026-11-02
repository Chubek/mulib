#define MULIB_IMPL_VEC
#define MULIB_IMPL_FST
#include "mulib-fst.h"
#include <assert.h>

int main (void)
{
  mulib_fst_t *fst = mulib_fst_create (0, 0);
  mulib_fst_result_t result = {0};
  const uint32_t input[] = {1, 2, 1};
  size_t state;
  assert (fst);
  assert (mulib_fst_add_state (fst, 1, &state) == MULIB_FST_OK);
  assert (mulib_fst_add_range (fst, state, state, 1, 1, 1, 10, NULL) == MULIB_FST_OK);
  assert (mulib_fst_add_range (fst, state, state, 2, 2, 0, 0, NULL) == MULIB_FST_OK);
  assert (mulib_fst_transduce (fst, state, input, 3, 100, &result, NULL) == MULIB_FST_OK);
  assert (result.length == 2 && result.output[0] == 10 && result.output[1] == 10);
  mulib_fst_result_free (&result);
  mulib_fst_destroy (fst);
  return 0;
}

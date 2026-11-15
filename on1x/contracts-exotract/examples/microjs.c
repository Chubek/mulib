#include "mulib.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  mulib_microjs_t *js = mulib_microjs_create(NULL);
  if (!js) return 1;
  mulib_microjs_value_t value = {0};
  mulib_microjs_error_t error;
  assert(mulib_microjs_eval(js, "6*7", 3, &value, &error) == MULIB_MICROJS_OK);
  assert(value.number == 42);
  printf("%.0f\n", value.number);
  mulib_microjs_value_free(&value);
  mulib_microjs_destroy(js);
  return 0;
}

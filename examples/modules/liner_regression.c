#define MULIB_IMPL_LINER_REGRESSION
#include "mulib-liner_regression.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double x[] = {1, 2, 3}, y[] = {3, 5, 7}; mulib_linear_regression_t model;
  assert(mulib_linear_regression_fit(x, y, 3, &model));
  assert(fabs(mulib_linear_regression_predict(&model, 4) - 9) < 1e-12); return 0;
}

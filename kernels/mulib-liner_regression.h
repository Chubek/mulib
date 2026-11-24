/** @file mulib-liner_regression.h
 * @brief Ordinary least-squares regression of one predictor with an intercept.
 * Define MULIB_IMPL_LINER_REGRESSION; link libm. Filename retained.
 */
#ifndef MULIB_LINER_REGRESSION_H
#define MULIB_LINER_REGRESSION_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_linear_regression { double slope, intercept, r_squared; } mulib_linear_regression_t;
/** Fit y=intercept+slope*x using scaled centered sums. Return 0 for n<2,
 * nonfinite data, constant x, unrepresentable coefficients or invalid pointers.
 * Output is unchanged on failure. Constant y gives r_squared=1. */
int mulib_linear_regression_fit(const double *x, const double *y, size_t n, mulib_linear_regression_t *result);
/** Evaluate fitted line; NAN for NULL model or nonfinite x. */
double mulib_linear_regression_predict(const mulib_linear_regression_t *model, double x);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LINER_REGRESSION) && !defined(MULIB_LINER_REGRESSION_IMPLEMENTED)
#define MULIB_LINER_REGRESSION_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_linear_regression_fit(const double *x, const double *y, size_t n, mulib_linear_regression_t *out)
{
  double sx = 0, sy = 0;
  long double mx = 0, my = 0, xx = 0, xy = 0, yy = 0, slope, intercept;
  mulib_linear_regression_t r;
  if (!x || !y || !out || n < 2) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(x[i]) || !isfinite(y[i])) return 0;
    sx = fmax(sx, fabs(x[i])); sy = fmax(sy, fabs(y[i]));
  }
  if (!sx) return 0;
  if (!sy) sy = 1;
  for (size_t i = 0; i < n; ++i) { mx += (long double)x[i] / sx; my += (long double)y[i] / sy; }
  mx /= n; my /= n;
  for (size_t i = 0; i < n; ++i) {
    long double a = (long double)x[i] / sx - mx, b = (long double)y[i] / sy - my;
    xx += a * a; xy += a * b; yy += b * b;
  }
  if (!xx) return 0;
  slope = (xy / xx) * ((long double)sy / sx);
  intercept = my * sy - slope * mx * sx;
  r.slope = (double)slope; r.intercept = (double)intercept;
  r.r_squared = yy ? (double)fminl(1, (xy / sqrtl(xx) / sqrtl(yy)) * (xy / sqrtl(xx) / sqrtl(yy))) : 1;
  if (!isfinite(r.slope) || !isfinite(r.intercept)) return 0;
  *out = r; return 1;
}
double mulib_linear_regression_predict(const mulib_linear_regression_t *m, double x)
{ return m && isfinite(x) ? fma(m->slope, x, m->intercept) : NAN; }
#ifdef __cplusplus
}
#endif
#endif

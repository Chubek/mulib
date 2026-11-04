/** @file mulib-kmm.h
 * @brief Kernel mean matching by projected gradient on a capped simplex.
 * Define MULIB_IMPL_KMM; link libm. KMM means kernel mean matching, not k-means.
 */
#ifndef MULIB_KMM_H
#define MULIB_KMM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Minimize 0.5*w'K*w-kappa'w, with 0<=w<=bound and sum(w)=n (epsilon=0).
 * K is borrowed row-major n*n, symmetric positive semidefinite (PSD is a caller
 * precondition); kappa[i]=(n/m)*sum_j kernel(source_i,target_j).
 * bound>=1, tolerance>0, iterations>0. Output has n slots, does not overlap inputs.
 * Return 1 if successive max change <= tolerance, 2 on iteration limit (valid
 * approximate weights), 0 invalid/OOM/numerical failure (output unchanged).
 * Convergence is a numerical stopping test, not an optimality certificate.
 */
int mulib_kmm_fit(const double *kernel, const double *kappa, size_t n, double bound,
                  size_t iterations, double tolerance, double *weights);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_KMM) && !defined(MULIB_KMM_IMPLEMENTED)
#define MULIB_KMM_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_kmm_fit(const double *k, const double *q, size_t n, double bound,
                  size_t iterations, double tolerance, double *out)
{
  long double lipschitz = 0, *w, *v;
  int status = 2;
  if (!k || !q || !out || !n || n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(double)
      || n > SIZE_MAX / (2 * sizeof(long double)) || !isfinite(bound) || bound < 1
      || !iterations || !isfinite(tolerance) || tolerance <= 0) return 0;
  for (size_t i = 0; i < n; ++i) {
    long double row = 0;
    if (!isfinite(q[i])) return 0;
    for (size_t j = 0; j < n; ++j) {
      if (!isfinite(k[i * n + j]) || k[i * n + j] != k[j * n + i]) return 0;
      row += fabsl(k[i * n + j]);
    }
    if (row > lipschitz) lipschitz = row;
  }
  if (!isfinite(lipschitz)) return 0;
  if (lipschitz < 1) lipschitz = 1;
  w = (long double *)malloc(2 * n * sizeof(*w));
  if (!w) return 0;
  v = w + n;
  for (size_t i = 0; i < n; ++i) w[i] = 1;
  for (size_t iteration = 0; iteration < iterations; ++iteration) {
    long double low = INFINITY, high = -INFINITY, change = 0, offset = -INFINITY;
    for (size_t i = 0; i < n; ++i) {
      long double gradient = -q[i];
      for (size_t j = 0; j < n; ++j) gradient += k[i * n + j] * w[j];
      v[i] = w[i] - gradient / lipschitz;
      if (!isfinite(v[i])) { free(w); return 0; }
      offset = fmaxl(offset, v[i]);
    }
    /* A common shift does not alter projection; remove it to reduce cancellation. */
    for (size_t i = 0; i < n; ++i) {
      v[i] -= offset;
      if (!isfinite(v[i])) { free(w); return 0; }
      low = fminl(low, v[i] - bound); high = fmaxl(high, v[i]);
    }
    if (!isfinite(low) || !isfinite(high)) { free(w); return 0; }
    for (unsigned bisection = 0; bisection < 128; ++bisection) {
      long double mid = low / 2 + high / 2, sum = 0;
      for (size_t i = 0; i < n; ++i) sum += fmaxl(0, fminl(bound, v[i] - mid));
      if (sum > n) low = mid; else high = mid;
    }
    long double sum = 0;
    for (size_t i = 0; i < n; ++i) {
      long double next = fmaxl(0, fminl(bound, v[i] - (low / 2 + high / 2)));
      change = fmaxl(change, fabsl(next - w[i])); w[i] = next; sum += next;
    }
    if (!isfinite(sum) || fabsl(sum - n) > 1e-10L * n) { free(w); return 0; }
    if (change <= tolerance) { status = 1; break; }
  }
  for (size_t i = 0; i < n; ++i) out[i] = (double)w[i];
  free(w); return status;
}
#ifdef __cplusplus
}
#endif
#endif

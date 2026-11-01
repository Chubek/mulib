/**
 * @file mulib-simdmath.h
 * @brief Four-lane SIMD arithmetic and batched graphics transforms.
 * Define MULIB_IMPL_SIMDMATH and MULIB_IMPL_LINALG. SSE2 and NEON are selected
 * at compile time; MULIB_SIMDMATH_SCALAR forces the portable fallback.
 */
#ifndef MULIB_SIMDMATH_H
#define MULIB_SIMDMATH_H
#include "mulib-linalg.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Stable, unaligned-safe public representation; lane order is
   * x,y,z,w. */
  typedef struct mulib_simdmath_f4
  {
    float lane[4];
  } mulib_simdmath_f4_t;
  /** @brief Backend name: "sse2", "neon", or "scalar". */
  const char *mulib_simdmath_backend (void);
  /** @brief Load/store exactly four floats; pointers must be non-NULL. */
  mulib_simdmath_f4_t mulib_simdmath_load (const float *values);
  void mulib_simdmath_store (float *values, mulib_simdmath_f4_t a);
  /** @brief Broadcast, lane-wise arithmetic, square root and four-lane dot. */
  mulib_simdmath_f4_t mulib_simdmath_splat (float value);
  mulib_simdmath_f4_t mulib_simdmath_add (mulib_simdmath_f4_t a,
                                          mulib_simdmath_f4_t b);
  mulib_simdmath_f4_t mulib_simdmath_sub (mulib_simdmath_f4_t a,
                                          mulib_simdmath_f4_t b);
  mulib_simdmath_f4_t mulib_simdmath_mul (mulib_simdmath_f4_t a,
                                          mulib_simdmath_f4_t b);
  mulib_simdmath_f4_t mulib_simdmath_div (mulib_simdmath_f4_t a,
                                          mulib_simdmath_f4_t b);
  mulib_simdmath_f4_t mulib_simdmath_sqrt (mulib_simdmath_f4_t a);
  float mulib_simdmath_dot (mulib_simdmath_f4_t a, mulib_simdmath_f4_t b);
  /** @brief Normalize a finite nonzero vector; preserve output on failure. */
  int mulib_simdmath_normalize (mulib_simdmath_f4_t a,
                                mulib_simdmath_f4_t *out);
  /** @brief SIMD column-major matrix composition (a*b). */
  mulib_linalg_mat4_t mulib_simdmath_mat4_mul (mulib_linalg_mat4_t a,
                                               mulib_linalg_mat4_t b);
  /** @brief Homogeneous transform, with the same column-major convention as
   * linalg. */
  mulib_linalg_vec4_t mulib_simdmath_transform (mulib_linalg_mat4_t m,
                                                mulib_linalg_vec4_t v);
  /** @brief Transform arrays; exact in-place use is allowed, other overlap is
   * not. NULL arrays are accepted for count zero. Return 0 for invalid
   * pointers/count.
   */
  int mulib_simdmath_transform_many (mulib_linalg_mat4_t m,
                                     const mulib_linalg_vec4_t *in,
                                     mulib_linalg_vec4_t *out, size_t count);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SIMDMATH) && !defined(MULIB_SIMDMATH_IMPLEMENTED)
#define MULIB_SIMDMATH_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <string.h>
#if !defined(MULIB_SIMDMATH_SCALAR) && defined(__SSE2__)
#include <emmintrin.h>
#define MULIB_SIMDMATH_SSE2_ 1
#elif !defined(MULIB_SIMDMATH_SCALAR) && defined(__ARM_NEON)
#include <arm_neon.h>
#define MULIB_SIMDMATH_NEON_ 1
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  const char *
  mulib_simdmath_backend (void)
  {
#if defined(MULIB_SIMDMATH_SSE2_)
    return "sse2";
#elif defined(MULIB_SIMDMATH_NEON_)
  return "neon";
#else
  return "scalar";
#endif
  }
  mulib_simdmath_f4_t
  mulib_simdmath_load (const float *v)
  {
    mulib_simdmath_f4_t r;
    memcpy (r.lane, v, sizeof r.lane);
    return r;
  }
  void
  mulib_simdmath_store (float *v, mulib_simdmath_f4_t a)
  {
    memcpy (v, a.lane, sizeof a.lane);
  }
  mulib_simdmath_f4_t
  mulib_simdmath_splat (float x)
  {
    mulib_simdmath_f4_t r = { { x, x, x, x } };
    return r;
  }
#if defined(MULIB_SIMDMATH_SSE2_)
#define MULIB_SIMDMATH_BINARY_(name, op, sse, neon)                           \
  mulib_simdmath_f4_t mulib_simdmath_##name (mulib_simdmath_f4_t a,           \
                                             mulib_simdmath_f4_t b)           \
  {                                                                           \
    mulib_simdmath_f4_t r;                                                    \
    _mm_storeu_ps (r.lane,                                                    \
                   sse (_mm_loadu_ps (a.lane), _mm_loadu_ps (b.lane)));       \
    return r;                                                                 \
  }
#elif defined(MULIB_SIMDMATH_NEON_)
#define MULIB_SIMDMATH_BINARY_(name, op, sse, neon)                           \
  mulib_simdmath_f4_t mulib_simdmath_##name (mulib_simdmath_f4_t a,           \
                                             mulib_simdmath_f4_t b)           \
  {                                                                           \
    mulib_simdmath_f4_t r;                                                    \
    vst1q_f32 (r.lane, neon (vld1q_f32 (a.lane), vld1q_f32 (b.lane)));        \
    return r;                                                                 \
  }
#else
#define MULIB_SIMDMATH_BINARY_(name, op, sse, neon)                           \
  mulib_simdmath_f4_t mulib_simdmath_##name (mulib_simdmath_f4_t a,           \
                                             mulib_simdmath_f4_t b)           \
  {                                                                           \
    mulib_simdmath_f4_t r;                                                    \
    for (int i = 0; i < 4; ++i)                                               \
      r.lane[i] = a.lane[i] op b.lane[i];                                     \
    return r;                                                                 \
  }
#endif
  MULIB_SIMDMATH_BINARY_ (add, +, _mm_add_ps, vaddq_f32)
  MULIB_SIMDMATH_BINARY_ (sub, -, _mm_sub_ps, vsubq_f32)
  MULIB_SIMDMATH_BINARY_ (mul, *, _mm_mul_ps, vmulq_f32)
#undef MULIB_SIMDMATH_BINARY_
  mulib_simdmath_f4_t
  mulib_simdmath_div (mulib_simdmath_f4_t a, mulib_simdmath_f4_t b)
  {
    mulib_simdmath_f4_t r;
#if defined(MULIB_SIMDMATH_SSE2_)
    _mm_storeu_ps (r.lane,
                   _mm_div_ps (_mm_loadu_ps (a.lane), _mm_loadu_ps (b.lane)));
#elif defined(MULIB_SIMDMATH_NEON_) && defined(__aarch64__)
  vst1q_f32 (r.lane, vdivq_f32 (vld1q_f32 (a.lane), vld1q_f32 (b.lane)));
#else
  for (int i = 0; i < 4; ++i)
    r.lane[i] = a.lane[i] / b.lane[i];
#endif
    return r;
  }
  mulib_simdmath_f4_t
  mulib_simdmath_sqrt (mulib_simdmath_f4_t a)
  {
    mulib_simdmath_f4_t r;
#if defined(MULIB_SIMDMATH_SSE2_)
    _mm_storeu_ps (r.lane, _mm_sqrt_ps (_mm_loadu_ps (a.lane)));
#elif defined(MULIB_SIMDMATH_NEON_) && defined(__aarch64__)
  vst1q_f32 (r.lane, vsqrtq_f32 (vld1q_f32 (a.lane)));
#else
  for (int i = 0; i < 4; ++i)
    r.lane[i] = sqrtf (a.lane[i]);
#endif
    return r;
  }
  float
  mulib_simdmath_dot (mulib_simdmath_f4_t a, mulib_simdmath_f4_t b)
  {
    mulib_simdmath_f4_t r = mulib_simdmath_mul (a, b);
    return r.lane[0] + r.lane[1] + r.lane[2] + r.lane[3];
  }
  int
  mulib_simdmath_normalize (mulib_simdmath_f4_t a, mulib_simdmath_f4_t *out)
  {
    float scale = 0, length;
    if (!out)
      return 0;
    for (int i = 0; i < 4; ++i)
      {
        if (!isfinite (a.lane[i]))
          return 0;
        scale = fmaxf (scale, fabsf (a.lane[i]));
      }
    if (scale == 0)
      return 0;
    a = mulib_simdmath_div (a, mulib_simdmath_splat (scale));
    length = sqrtf (mulib_simdmath_dot (a, a));
    *out = mulib_simdmath_div (a, mulib_simdmath_splat (length));
    return 1;
  }
  mulib_linalg_vec4_t
  mulib_simdmath_transform (mulib_linalg_mat4_t m, mulib_linalg_vec4_t v)
  {
    mulib_simdmath_f4_t r = mulib_simdmath_mul (mulib_simdmath_load (m.m),
                                                mulib_simdmath_splat (v.x));
    r = mulib_simdmath_add (r,
                            mulib_simdmath_mul (mulib_simdmath_load (m.m + 4),
                                                mulib_simdmath_splat (v.y)));
    r = mulib_simdmath_add (r,
                            mulib_simdmath_mul (mulib_simdmath_load (m.m + 8),
                                                mulib_simdmath_splat (v.z)));
    r = mulib_simdmath_add (r,
                            mulib_simdmath_mul (mulib_simdmath_load (m.m + 12),
                                                mulib_simdmath_splat (v.w)));
    {
      mulib_linalg_vec4_t out = { r.lane[0], r.lane[1], r.lane[2], r.lane[3] };
      return out;
    }
  }
  mulib_linalg_mat4_t
  mulib_simdmath_mat4_mul (mulib_linalg_mat4_t a, mulib_linalg_mat4_t b)
  {
    mulib_linalg_mat4_t r;
    for (int i = 0; i < 4; ++i)
      {
        mulib_linalg_vec4_t v
            = { b.m[4 * i], b.m[4 * i + 1], b.m[4 * i + 2], b.m[4 * i + 3] };
        v = mulib_simdmath_transform (a, v);
        r.m[4 * i] = v.x;
        r.m[4 * i + 1] = v.y;
        r.m[4 * i + 2] = v.z;
        r.m[4 * i + 3] = v.w;
      }
    return r;
  }
  int
  mulib_simdmath_transform_many (mulib_linalg_mat4_t m,
                                 const mulib_linalg_vec4_t *in,
                                 mulib_linalg_vec4_t *out, size_t count)
  {
    if ((!in && count) || (!out && count) || count > SIZE_MAX / sizeof (*in))
      return 0;
    for (size_t i = 0; i < count; ++i)
      out[i] = mulib_simdmath_transform (m, in[i]);
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif

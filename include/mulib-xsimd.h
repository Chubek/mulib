/** @file mulib-xsimd.h
 * @brief Runtime-selected scalar, SSE4.1, AVX2 and AArch64 NEON float kernels.
 */
#ifndef MULIB_XSIMD_H
#define MULIB_XSIMD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_xsimd_backend {
  MULIB_XSIMD_AUTO, MULIB_XSIMD_SCALAR, MULIB_XSIMD_SSE4,
  MULIB_XSIMD_AVX2, MULIB_XSIMD_NEON
} mulib_xsimd_backend_t;
/** Return whether this binary and host support a backend (including OS AVX state). */
int mulib_xsimd_available (mulib_xsimd_backend_t backend);
/** Resolve AUTO to the fastest available compiled backend. */
mulib_xsimd_backend_t mulib_xsimd_best (void);
/** dst[i]=src[i]*gain. Exact alias is allowed, partial overlap is not.
 * Unaligned arrays and arbitrary tails are supported. Return 0 for invalid
 * pointers/backend; output unchanged in that case. NULL is allowed for n=0. */
int mulib_xsimd_scale (mulib_xsimd_backend_t, float *dst, const float *src, size_t n, float gain);
/** dst[i]+=src[i]*gain; same alias and failure rules as scale. */
int mulib_xsimd_mix (mulib_xsimd_backend_t, float *dst, const float *src, size_t n, float gain);
/** Float dot product. Reduction rounding can differ between backends. */
int mulib_xsimd_dot (mulib_xsimd_backend_t, const float *a, const float *b, size_t n, float *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XSIMD) && !defined(MULIB_XSIMD_IMPLEMENTED)
#define MULIB_XSIMD_IMPLEMENTED
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define MULIB_XSIMD_X86_ 1
#endif
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define MULIB_XSIMD_ARM_ 1
#endif
int mulib_xsimd_available (mulib_xsimd_backend_t b)
{
  if (b == MULIB_XSIMD_AUTO || b == MULIB_XSIMD_SCALAR) return 1;
#ifdef MULIB_XSIMD_X86_
  if (b == MULIB_XSIMD_SSE4) return !!__builtin_cpu_supports ("sse4.1");
  if (b == MULIB_XSIMD_AVX2) return !!__builtin_cpu_supports ("avx2");
#endif
#ifdef MULIB_XSIMD_ARM_
  if (b == MULIB_XSIMD_NEON) return 1;
#endif
  return 0;
}
mulib_xsimd_backend_t mulib_xsimd_best (void)
{
  if (mulib_xsimd_available (MULIB_XSIMD_AVX2)) return MULIB_XSIMD_AVX2;
  if (mulib_xsimd_available (MULIB_XSIMD_SSE4)) return MULIB_XSIMD_SSE4;
  if (mulib_xsimd_available (MULIB_XSIMD_NEON)) return MULIB_XSIMD_NEON;
  return MULIB_XSIMD_SCALAR;
}
#ifdef MULIB_XSIMD_X86_
__attribute__((target("sse4.1"))) static size_t
mulib_xsimd_sse_ (float *d, const float *s, size_t n, float g, int mix)
{
  size_t i = 0; __m128 gain = _mm_set1_ps (g);
  for (; n-i >= 4; i += 4) {
    __m128 v = _mm_mul_ps (_mm_loadu_ps (s+i), gain);
    if (mix) v = _mm_add_ps (v, _mm_loadu_ps (d+i));
    _mm_storeu_ps (d+i, v);
  }
  return i;
}
__attribute__((target("avx2"))) static size_t
mulib_xsimd_avx_ (float *d, const float *s, size_t n, float g, int mix)
{
  size_t i = 0; __m256 gain = _mm256_set1_ps (g);
  for (; n-i >= 8; i += 8) {
    __m256 v = _mm256_mul_ps (_mm256_loadu_ps (s+i), gain);
    if (mix) v = _mm256_add_ps (v, _mm256_loadu_ps (d+i));
    _mm256_storeu_ps (d+i, v);
  }
  return i;
}
__attribute__((target("sse4.1"))) static size_t
mulib_xsimd_sdot_ (const float *a, const float *b, size_t n, float *out)
{
  size_t i = 0; float v[4]; __m128 sum = _mm_setzero_ps ();
  for (; n-i >= 4; i += 4) sum = _mm_add_ps (sum, _mm_mul_ps (_mm_loadu_ps (a+i), _mm_loadu_ps (b+i)));
  _mm_storeu_ps (v, sum); *out = (v[0]+v[1])+(v[2]+v[3]); return i;
}
__attribute__((target("avx2"))) static size_t
mulib_xsimd_adot_ (const float *a, const float *b, size_t n, float *out)
{
  size_t i = 0; float v[8]; __m256 sum = _mm256_setzero_ps ();
  for (; n-i >= 8; i += 8) sum = _mm256_add_ps (sum, _mm256_mul_ps (_mm256_loadu_ps (a+i), _mm256_loadu_ps (b+i)));
  _mm256_storeu_ps (v, sum); *out = 0;
  { unsigned j; for (j=0; j<8; ++j) *out += v[j]; } return i;
}
#endif
static int mulib_xsimd_apply_ (mulib_xsimd_backend_t b, float *d, const float *s, size_t n, float g, int mix)
{
  size_t i = 0;
  if ((!d && n) || (!s && n) || !mulib_xsimd_available (b)) return 0;
  if (b == MULIB_XSIMD_AUTO) b = mulib_xsimd_best ();
#ifdef MULIB_XSIMD_X86_
  if (b == MULIB_XSIMD_AVX2) i = mulib_xsimd_avx_ (d,s,n,g,mix);
  else if (b == MULIB_XSIMD_SSE4) i = mulib_xsimd_sse_ (d,s,n,g,mix);
#endif
#ifdef MULIB_XSIMD_ARM_
  if (b == MULIB_XSIMD_NEON) for (; n-i >= 4; i+=4) {
    float32x4_t v = vmulq_n_f32 (vld1q_f32 (s+i), g);
    if (mix) v = vaddq_f32 (v, vld1q_f32 (d+i));
    vst1q_f32 (d+i,v);
  }
#endif
  for (; i<n; ++i) d[i] = mix ? d[i] + s[i]*g : s[i]*g;
  return 1;
}
int mulib_xsimd_scale (mulib_xsimd_backend_t b, float *d, const float *s, size_t n, float g)
{ return mulib_xsimd_apply_ (b,d,s,n,g,0); }
int mulib_xsimd_mix (mulib_xsimd_backend_t b, float *d, const float *s, size_t n, float g)
{ return mulib_xsimd_apply_ (b,d,s,n,g,1); }
int mulib_xsimd_dot (mulib_xsimd_backend_t b, const float *a, const float *c, size_t n, float *out)
{
  size_t i = 0; float sum = 0;
  if (!out || (!a && n) || (!c && n) || !mulib_xsimd_available (b)) return 0;
  if (b == MULIB_XSIMD_AUTO) b = mulib_xsimd_best ();
#ifdef MULIB_XSIMD_X86_
  if (b == MULIB_XSIMD_AVX2) i = mulib_xsimd_adot_ (a,c,n,&sum);
  else if (b == MULIB_XSIMD_SSE4) i = mulib_xsimd_sdot_ (a,c,n,&sum);
#endif
#ifdef MULIB_XSIMD_ARM_
  if (b == MULIB_XSIMD_NEON) {
    float32x4_t v = vdupq_n_f32 (0);
    for (; n-i >= 4; i+=4) v = vaddq_f32 (v, vmulq_f32 (vld1q_f32(a+i),vld1q_f32(c+i)));
    sum = vaddvq_f32 (v);
  }
#endif
  for (; i<n; ++i) sum += a[i]*c[i];
  *out = sum; return 1;
}
#endif

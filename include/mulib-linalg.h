/**
 * @file mulib-linalg.h
 * @brief Float vectors, column-major transforms and quaternions for graphics.
 * Define MULIB_IMPL_LINALG in one C11 translation unit; link with libm.
 * Angles are radians, coordinates right-handed, matrices multiply column
 * vectors. Checked functions return 1 on success and preserve output on error.
 */
#ifndef MULIB_LINALG_H
#define MULIB_LINALG_H
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_linalg_vec2
  {
    float x, y;
  } mulib_linalg_vec2_t;
  typedef struct mulib_linalg_vec3
  {
    float x, y, z;
  } mulib_linalg_vec3_t;
  typedef struct mulib_linalg_vec4
  {
    float x, y, z, w;
  } mulib_linalg_vec4_t;
  typedef struct mulib_linalg_mat4
  {
    float m[16];
  } mulib_linalg_mat4_t;
  typedef struct mulib_linalg_quat
  {
    float x, y, z, w;
  } mulib_linalg_quat_t;
  /** @brief Component arithmetic; ordinary IEEE float overflow rules apply. */
  mulib_linalg_vec2_t mulib_linalg_vec2_add (mulib_linalg_vec2_t a,
                                             mulib_linalg_vec2_t b);
  mulib_linalg_vec2_t mulib_linalg_vec2_sub (mulib_linalg_vec2_t a,
                                             mulib_linalg_vec2_t b);
  mulib_linalg_vec2_t mulib_linalg_vec2_scale (mulib_linalg_vec2_t a, float s);
  float mulib_linalg_vec2_dot (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b);
  float mulib_linalg_vec2_length (mulib_linalg_vec2_t a);
  /** @brief Normalize a finite, nonzero vector, including subnormal vectors.
   */
  int mulib_linalg_vec2_normalize (mulib_linalg_vec2_t a,
                                   mulib_linalg_vec2_t *out);
  /** @brief Three-dimensional component arithmetic and cross product. */
  mulib_linalg_vec3_t mulib_linalg_vec3_add (mulib_linalg_vec3_t a,
                                             mulib_linalg_vec3_t b);
  mulib_linalg_vec3_t mulib_linalg_vec3_sub (mulib_linalg_vec3_t a,
                                             mulib_linalg_vec3_t b);
  mulib_linalg_vec3_t mulib_linalg_vec3_scale (mulib_linalg_vec3_t a, float s);
  float mulib_linalg_vec3_dot (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b);
  mulib_linalg_vec3_t mulib_linalg_vec3_cross (mulib_linalg_vec3_t a,
                                               mulib_linalg_vec3_t b);
  float mulib_linalg_vec3_length (mulib_linalg_vec3_t a);
  int mulib_linalg_vec3_normalize (mulib_linalg_vec3_t a,
                                   mulib_linalg_vec3_t *out);
  /** @brief Linear interpolation; t need not lie in [0,1]. */
  mulib_linalg_vec3_t mulib_linalg_vec3_lerp (mulib_linalg_vec3_t a,
                                              mulib_linalg_vec3_t b, float t);
  /** @brief Four-dimensional component arithmetic. */
  mulib_linalg_vec4_t mulib_linalg_vec4_add (mulib_linalg_vec4_t a,
                                             mulib_linalg_vec4_t b);
  mulib_linalg_vec4_t mulib_linalg_vec4_scale (mulib_linalg_vec4_t a, float s);
  float mulib_linalg_vec4_dot (mulib_linalg_vec4_t a, mulib_linalg_vec4_t b);
  int mulib_linalg_vec4_normalize (mulib_linalg_vec4_t a,
                                   mulib_linalg_vec4_t *out);
  /** @brief Identity, composition (a*b), transpose, and homogeneous transform.
   */
  mulib_linalg_mat4_t mulib_linalg_mat4_identity (void);
  mulib_linalg_mat4_t mulib_linalg_mat4_mul (mulib_linalg_mat4_t a,
                                             mulib_linalg_mat4_t b);
  mulib_linalg_mat4_t mulib_linalg_mat4_transpose (mulib_linalg_mat4_t a);
  mulib_linalg_vec4_t mulib_linalg_mat4_transform (mulib_linalg_mat4_t m,
                                                   mulib_linalg_vec4_t v);
  /** @brief Pivoted inverse; reject singular/nonfinite matrices or float
   * overflow. */
  int mulib_linalg_mat4_inverse (mulib_linalg_mat4_t m,
                                 mulib_linalg_mat4_t *out);
  /** @brief Translation and nonuniform scale transforms. */
  mulib_linalg_mat4_t mulib_linalg_mat4_translation (mulib_linalg_vec3_t t);
  mulib_linalg_mat4_t mulib_linalg_mat4_scaling (mulib_linalg_vec3_t s);
  /** @brief Right-handed view transform; reject coincident eye/target or
   * parallel up. */
  int mulib_linalg_look_at (mulib_linalg_vec3_t eye,
                            mulib_linalg_vec3_t target, mulib_linalg_vec3_t up,
                            mulib_linalg_mat4_t *out);
  /** @brief Perspective with OpenGL NDC depth [-1,1]; 0<fovy<pi, aspect>0,
   * 0<near<far. */
  int mulib_linalg_perspective (float fovy, float aspect, float near_z,
                                float far_z, mulib_linalg_mat4_t *out);
  /** @brief Orthographic OpenGL projection; bounds must be finite and
   * increasing. */
  int mulib_linalg_ortho (float left, float right, float bottom, float top,
                          float near_z, float far_z, mulib_linalg_mat4_t *out);
  /** @brief Normalize quaternion; zero or nonfinite input fails. */
  int mulib_linalg_quat_normalize (mulib_linalg_quat_t q,
                                   mulib_linalg_quat_t *out);
  /** @brief Unit quaternion from nonzero axis and a finite radian angle. */
  int mulib_linalg_quat_axis_angle (mulib_linalg_vec3_t axis, float angle,
                                    mulib_linalg_quat_t *out);
  /** @brief Hamilton product: b is applied before a. */
  mulib_linalg_quat_t mulib_linalg_quat_mul (mulib_linalg_quat_t a,
                                             mulib_linalg_quat_t b);
  /** @brief Convert any finite nonzero quaternion into a rotation matrix. */
  int mulib_linalg_quat_matrix (mulib_linalg_quat_t q,
                                mulib_linalg_mat4_t *out);
  /** @brief Shortest-arc spherical interpolation of normalized inputs;
   * 0<=t<=1. */
  int mulib_linalg_quat_slerp (mulib_linalg_quat_t a, mulib_linalg_quat_t b,
                               float t, mulib_linalg_quat_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_LINALG) && !defined(MULIB_LINALG_IMPLEMENTED)
#define MULIB_LINALG_IMPLEMENTED
#include <float.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_linalg_vec2_t
  mulib_linalg_vec2_add (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b)
  {
    mulib_linalg_vec2_t r = { a.x + b.x, a.y + b.y };
    return r;
  }
  mulib_linalg_vec2_t
  mulib_linalg_vec2_sub (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b)
  {
    mulib_linalg_vec2_t r = { a.x - b.x, a.y - b.y };
    return r;
  }
  mulib_linalg_vec2_t
  mulib_linalg_vec2_scale (mulib_linalg_vec2_t a, float s)
  {
    mulib_linalg_vec2_t r = { a.x * s, a.y * s };
    return r;
  }
  float
  mulib_linalg_vec2_dot (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b)
  {
    return a.x * b.x + a.y * b.y;
  }
  float
  mulib_linalg_vec2_length (mulib_linalg_vec2_t a)
  {
    return hypotf (a.x, a.y);
  }
  int
  mulib_linalg_vec2_normalize (mulib_linalg_vec2_t a, mulib_linalg_vec2_t *out)
  {
    double n = hypot ((double)a.x, a.y);
    if (!out || !isfinite (n) || n == 0)
      return 0;
    a.x = (float)(a.x / n);
    a.y = (float)(a.y / n);
    *out = a;
    return 1;
  }
  mulib_linalg_vec3_t
  mulib_linalg_vec3_add (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    mulib_linalg_vec3_t r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
  }
  mulib_linalg_vec3_t
  mulib_linalg_vec3_sub (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    mulib_linalg_vec3_t r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
  }
  mulib_linalg_vec3_t
  mulib_linalg_vec3_scale (mulib_linalg_vec3_t a, float s)
  {
    mulib_linalg_vec3_t r = { a.x * s, a.y * s, a.z * s };
    return r;
  }
  float
  mulib_linalg_vec3_dot (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }
  mulib_linalg_vec3_t
  mulib_linalg_vec3_cross (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    mulib_linalg_vec3_t r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                              a.x * b.y - a.y * b.x };
    return r;
  }
  float
  mulib_linalg_vec3_length (mulib_linalg_vec3_t a)
  {
    return hypotf (hypotf (a.x, a.y), a.z);
  }
  int
  mulib_linalg_vec3_normalize (mulib_linalg_vec3_t a, mulib_linalg_vec3_t *out)
  {
    double n = hypot (hypot ((double)a.x, a.y), a.z);
    if (!out || !isfinite (n) || n == 0)
      return 0;
    a.x = (float)(a.x / n);
    a.y = (float)(a.y / n);
    a.z = (float)(a.z / n);
    *out = a;
    return 1;
  }
  mulib_linalg_vec3_t
  mulib_linalg_vec3_lerp (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b,
                          float t)
  {
    mulib_linalg_vec3_t r = { (float)((1.0 - t) * a.x + t * (double)b.x),
                              (float)((1.0 - t) * a.y + t * (double)b.y),
                              (float)((1.0 - t) * a.z + t * (double)b.z) };
    return r;
  }
  mulib_linalg_vec4_t
  mulib_linalg_vec4_add (mulib_linalg_vec4_t a, mulib_linalg_vec4_t b)
  {
    mulib_linalg_vec4_t r = { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
    return r;
  }
  mulib_linalg_vec4_t
  mulib_linalg_vec4_scale (mulib_linalg_vec4_t a, float s)
  {
    mulib_linalg_vec4_t r = { a.x * s, a.y * s, a.z * s, a.w * s };
    return r;
  }
  float
  mulib_linalg_vec4_dot (mulib_linalg_vec4_t a, mulib_linalg_vec4_t b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  }
  int
  mulib_linalg_vec4_normalize (mulib_linalg_vec4_t a, mulib_linalg_vec4_t *out)
  {
    double n = hypot (hypot ((double)a.x, a.y), hypot ((double)a.z, a.w));
    if (!out || !isfinite (n) || n == 0)
      return 0;
    a.x = (float)(a.x / n);
    a.y = (float)(a.y / n);
    a.z = (float)(a.z / n);
    a.w = (float)(a.w / n);
    *out = a;
    return 1;
  }
  mulib_linalg_mat4_t
  mulib_linalg_mat4_identity (void)
  {
    mulib_linalg_mat4_t r
        = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    return r;
  }
  mulib_linalg_mat4_t
  mulib_linalg_mat4_mul (mulib_linalg_mat4_t a, mulib_linalg_mat4_t b)
  {
    mulib_linalg_mat4_t r;
    for (int c = 0; c < 4; ++c)
      for (int row = 0; row < 4; ++row)
        {
          double v = 0;
          for (int k = 0; k < 4; ++k)
            v += (double)a.m[k * 4 + row] * b.m[c * 4 + k];
          r.m[c * 4 + row] = (float)v;
        }
    return r;
  }
  mulib_linalg_mat4_t
  mulib_linalg_mat4_transpose (mulib_linalg_mat4_t a)
  {
    mulib_linalg_mat4_t r;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        r.m[i * 4 + j] = a.m[j * 4 + i];
    return r;
  }
  mulib_linalg_vec4_t
  mulib_linalg_mat4_transform (mulib_linalg_mat4_t m, mulib_linalg_vec4_t v)
  {
    float a[4] = { v.x, v.y, v.z, v.w }, b[4];
    for (int i = 0; i < 4; ++i)
      b[i] = m.m[i] * a[0] + m.m[4 + i] * a[1] + m.m[8 + i] * a[2]
             + m.m[12 + i] * a[3];
    {
      mulib_linalg_vec4_t r = { b[0], b[1], b[2], b[3] };
      return r;
    }
  }
  static int
  mulib_linalg_finite_matrix_ (mulib_linalg_mat4_t m)
  {
    for (int i = 0; i < 16; ++i)
      if (!isfinite (m.m[i]))
        return 0;
    return 1;
  }
  int
  mulib_linalg_mat4_inverse (mulib_linalg_mat4_t m, mulib_linalg_mat4_t *out)
  {
    double a[4][8];
    mulib_linalg_mat4_t r;
    if (!out || !mulib_linalg_finite_matrix_ (m))
      return 0;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 8; ++j)
        a[i][j] = j < 4 ? m.m[j * 4 + i] : (double)(j - 4 == i);
    for (int k = 0; k < 4; ++k)
      {
        int p = k;
        for (int i = k + 1; i < 4; ++i)
          if (fabs (a[i][k]) > fabs (a[p][k]))
            p = i;
        if (a[p][k] == 0)
          return 0;
        for (int j = 0; j < 8; ++j)
          {
            double t = a[p][j];
            a[p][j] = a[k][j];
            a[k][j] = t;
          }
        {
          double d = a[k][k];
          for (int j = 0; j < 8; ++j)
            a[k][j] /= d;
        }
        for (int i = 0; i < 4; ++i)
          if (i != k)
            {
              double d = a[i][k];
              for (int j = 0; j < 8; ++j)
                a[i][j] -= d * a[k][j];
            }
      }
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        {
          if (!isfinite (a[i][j + 4]) || fabs (a[i][j + 4]) > FLT_MAX)
            return 0;
          r.m[j * 4 + i] = (float)a[i][j + 4];
        }
    *out = r;
    return 1;
  }
  mulib_linalg_mat4_t
  mulib_linalg_mat4_translation (mulib_linalg_vec3_t t)
  {
    mulib_linalg_mat4_t r = mulib_linalg_mat4_identity ();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
  }
  mulib_linalg_mat4_t
  mulib_linalg_mat4_scaling (mulib_linalg_vec3_t s)
  {
    mulib_linalg_mat4_t r = mulib_linalg_mat4_identity ();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
  }
  int
  mulib_linalg_look_at (mulib_linalg_vec3_t eye, mulib_linalg_vec3_t target,
                        mulib_linalg_vec3_t up, mulib_linalg_mat4_t *out)
  {
    mulib_linalg_vec3_t f, s, u;
    mulib_linalg_mat4_t r = mulib_linalg_mat4_identity ();
    if (!out
        || !mulib_linalg_vec3_normalize (mulib_linalg_vec3_sub (target, eye),
                                         &f)
        || !mulib_linalg_vec3_normalize (up, &u)
        || !mulib_linalg_vec3_normalize (mulib_linalg_vec3_cross (f, u), &s))
      return 0;
    u = mulib_linalg_vec3_cross (s, f);
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -mulib_linalg_vec3_dot (s, eye);
    r.m[13] = -mulib_linalg_vec3_dot (u, eye);
    r.m[14] = mulib_linalg_vec3_dot (f, eye);
    if (!mulib_linalg_finite_matrix_ (r))
      return 0;
    *out = r;
    return 1;
  }
  int
  mulib_linalg_perspective (float fovy, float aspect, float n, float f,
                            mulib_linalg_mat4_t *out)
  {
    mulib_linalg_mat4_t r = { { 0 } };
    double scale;
    if (!out || !isfinite (fovy) || !isfinite (aspect) || !isfinite (n)
        || !isfinite (f) || fovy <= 0 || fovy >= 3.14159265358979323846
        || aspect <= 0 || n <= 0 || f <= n)
      return 0;
    scale = 1 / tan (fovy / 2.0);
    r.m[0] = (float)(scale / aspect);
    r.m[5] = (float)scale;
    r.m[10] = (float)(-((double)f + n) / (f - (double)n));
    r.m[11] = -1;
    r.m[14] = (float)(-2.0 * f * n / (f - (double)n));
    if (!mulib_linalg_finite_matrix_ (r))
      return 0;
    *out = r;
    return 1;
  }
  int
  mulib_linalg_ortho (float l, float r, float b, float t, float n, float f,
                      mulib_linalg_mat4_t *out)
  {
    mulib_linalg_mat4_t m = mulib_linalg_mat4_identity ();
    if (!out || !isfinite (l) || !isfinite (r) || !isfinite (b)
        || !isfinite (t) || !isfinite (n) || !isfinite (f) || r <= l || t <= b
        || f <= n)
      return 0;
    m.m[0] = (float)(2 / (r - (double)l));
    m.m[5] = (float)(2 / (t - (double)b));
    m.m[10] = (float)(-2 / (f - (double)n));
    m.m[12] = (float)(-(r + (double)l) / (r - (double)l));
    m.m[13] = (float)(-(t + (double)b) / (t - (double)b));
    m.m[14] = (float)(-(f + (double)n) / (f - (double)n));
    if (!mulib_linalg_finite_matrix_ (m))
      return 0;
    *out = m;
    return 1;
  }
  int
  mulib_linalg_quat_normalize (mulib_linalg_quat_t q, mulib_linalg_quat_t *out)
  {
    mulib_linalg_vec4_t v = { q.x, q.y, q.z, q.w };
    if (!out || !mulib_linalg_vec4_normalize (v, &v))
      return 0;
    q.x = v.x;
    q.y = v.y;
    q.z = v.z;
    q.w = v.w;
    *out = q;
    return 1;
  }
  int
  mulib_linalg_quat_axis_angle (mulib_linalg_vec3_t a, float angle,
                                mulib_linalg_quat_t *out)
  {
    mulib_linalg_quat_t q;
    float s;
    if (!out || !isfinite (angle) || !mulib_linalg_vec3_normalize (a, &a))
      return 0;
    s = sinf (angle / 2);
    q.x = a.x * s;
    q.y = a.y * s;
    q.z = a.z * s;
    q.w = cosf (angle / 2);
    *out = q;
    return 1;
  }
  mulib_linalg_quat_t
  mulib_linalg_quat_mul (mulib_linalg_quat_t a, mulib_linalg_quat_t b)
  {
    mulib_linalg_quat_t q = { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                              a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                              a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                              a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
    return q;
  }
  int
  mulib_linalg_quat_matrix (mulib_linalg_quat_t q, mulib_linalg_mat4_t *out)
  {
    mulib_linalg_mat4_t m = mulib_linalg_mat4_identity ();
    if (!out || !mulib_linalg_quat_normalize (q, &q))
      return 0;
    m.m[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
    m.m[4] = 2 * (q.x * q.y - q.z * q.w);
    m.m[8] = 2 * (q.x * q.z + q.y * q.w);
    m.m[1] = 2 * (q.x * q.y + q.z * q.w);
    m.m[5] = 1 - 2 * (q.x * q.x + q.z * q.z);
    m.m[9] = 2 * (q.y * q.z - q.x * q.w);
    m.m[2] = 2 * (q.x * q.z - q.y * q.w);
    m.m[6] = 2 * (q.y * q.z + q.x * q.w);
    m.m[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
    *out = m;
    return 1;
  }
  int
  mulib_linalg_quat_slerp (mulib_linalg_quat_t a, mulib_linalg_quat_t b,
                           float t, mulib_linalg_quat_t *out)
  {
    double d, x, y;
    mulib_linalg_quat_t q;
    if (!out || !isfinite (t) || t < 0 || t > 1
        || !mulib_linalg_quat_normalize (a, &a)
        || !mulib_linalg_quat_normalize (b, &b))
      return 0;
    d = (double)a.x * b.x + (double)a.y * b.y + (double)a.z * b.z
        + (double)a.w * b.w;
    if (d < 0)
      {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
        d = -d;
      }
    x = 1 - t;
    y = t;
    if (d < 0.9995)
      {
        double theta = acos (d), s = sin (theta);
        x = sin ((1 - t) * theta) / s;
        y = sin (t * theta) / s;
      }
    q.x = (float)(a.x * x + b.x * y);
    q.y = (float)(a.y * x + b.y * y);
    q.z = (float)(a.z * x + b.z * y);
    q.w = (float)(a.w * x + b.w * y);
    return mulib_linalg_quat_normalize (q, out);
  }
#ifdef __cplusplus
}
#endif
#endif

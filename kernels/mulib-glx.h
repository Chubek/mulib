/**
 * @file mulib-glx.h
 * @brief Software graphics surfaces backed by memory or MuLIB DRM dumb
 * buffers. Define MULIB_IMPL_GLX, MULIB_IMPL_DRM and MULIB_IMPL_LINALG. Pixels
 * are native uint32_t 0xAARRGGBB, straight alpha. Coordinates are top-left, y
 * downward. This is MuLIB's graphics library, independent of the X11 GLX
 * protocol.
 */
#ifndef MULIB_GLX_H
#define MULIB_GLX_H
#include "mulib-drm.h"
#include "mulib-linalg.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_glx mulib_glx_t;
  typedef struct mulib_glx_vertex
  {
    float x, y, z;
    mulib_linalg_vec4_t color;
  } mulib_glx_vertex_t;
  /** @brief Fragment callback; returns ARGB. Borrowed user data, no recursive
   * drawing. */
  typedef uint32_t (*mulib_glx_fragment_fn) (int x, int y, float z,
                                             mulib_linalg_vec4_t color,
                                             void *user);
  /** @brief Allocate a transparent surface and depth buffer;
   * dimensions 1..16384. NULL on error. */
  mulib_glx_t *mulib_glx_create (unsigned width, unsigned height);
  /** @brief Borrow aligned pixel memory. Pitch is bytes, a multiple of four
   * and >=width*4. bytes must cover height*pitch. Pixel storage must outlive
   * the context.
   */
  mulib_glx_t *mulib_glx_wrap (uint32_t *pixels, size_t bytes, unsigned width,
                               unsigned height, size_t pitch);
  /** @brief Map and borrow a 32-bpp DRM dumb buffer (including padded pitch).
   * The caller owns buffer/mapping, including on allocation failure. Destroy
   * this context before unmapping/destroying the buffer. Modesetting is
   * caller-owned.
   */
  mulib_glx_t *mulib_glx_map_drm (mulib_drm_dumb_t *buffer);
  /** @brief Free context/depth and owned pixels; NULL safe, never unmaps
   * borrowed DRM. */
  void mulib_glx_destroy (mulib_glx_t *context);
  /** @brief Borrow pixels or query dimensions/pitch; NULL context gives
   * NULL/zero. */
  uint32_t *mulib_glx_pixels (mulib_glx_t *context);
  unsigned mulib_glx_width (const mulib_glx_t *context);
  unsigned mulib_glx_height (const mulib_glx_t *context);
  size_t mulib_glx_pitch (const mulib_glx_t *context);
  /** @brief Set an intersected scissor; nonpositive size means empty. NULL is
   * harmless. */
  void mulib_glx_clip (mulib_glx_t *context, int x, int y, int width,
                       int height);
  void mulib_glx_reset_clip (mulib_glx_t *context);
  /** @brief Replace every pixel and reset depth to infinity, ignoring scissor.
   */
  void mulib_glx_clear (mulib_glx_t *context, uint32_t color);
  /** @brief Source-over blend a pixel or integer rectangle through the
   * scissor. */
  void mulib_glx_pixel (mulib_glx_t *context, int x, int y, uint32_t color);
  void mulib_glx_fill_rect (mulib_glx_t *context, int x, int y, int width,
                            int height, uint32_t color);
  /** @brief Clipped one-pixel line; returns 0 for nonfinite endpoints/NULL
   * context. */
  int mulib_glx_line (mulib_glx_t *context, mulib_linalg_vec2_t a,
                      mulib_linalg_vec2_t b, uint32_t color);
  /** @brief Flat triangle with top-left coverage rule, no depth test. */
  int mulib_glx_fill_triangle (mulib_glx_t *context, mulib_linalg_vec2_t a,
                               mulib_linalg_vec2_t b, mulib_linalg_vec2_t c,
                               uint32_t color);
  /** @brief Screen-space triangle with interpolated RGBA/depth and strict less
   * depth test. NULL shader packs interpolated color. No perspective divide or
   * near-plane clipping is implicit. Returns 0 on invalid/nonfinite inputs, 1
   * otherwise.
   */
  int mulib_glx_triangle (mulib_glx_t *context, mulib_glx_vertex_t a,
                          mulib_glx_vertex_t b, mulib_glx_vertex_t c,
                          mulib_glx_fragment_fn shader, void *user);
  /** @brief Blend an image; source rows are pitch bytes apart and must not
   * alias destination. Validate dimensions/pitch/bytes before drawing; return
   * 0 on invalid input.
   */
  int mulib_glx_blit (mulib_glx_t *context, int x, int y,
                      const uint32_t *pixels, size_t bytes, unsigned width,
                      unsigned height, size_t pitch);
  /** @brief Clamp finite linear components to [0,1], round to ARGB8; NaN
   * becomes zero. */
  uint32_t mulib_glx_rgba (float r, float g, float b, float a);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_GLX) && !defined(MULIB_GLX_IMPLEMENTED)
#define MULIB_GLX_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_glx
  {
    uint32_t *pixels, *allocation;
    float *depth;
    size_t stride;
    unsigned width, height;
    int x0, y0, x1, y1;
  };
  static int
  mulib_glx_layout_ (unsigned w, unsigned h, size_t pitch, size_t bytes)
  {
    return w && h && w <= 16384 && h <= 16384 && pitch % 4 == 0
           && pitch / 4 >= w && pitch <= SIZE_MAX / h && bytes >= pitch * h;
  }
  mulib_glx_t *
  mulib_glx_wrap (uint32_t *pixels, size_t bytes, unsigned w, unsigned h,
                  size_t pitch)
  {
    mulib_glx_t *g;
    if (!pixels || !mulib_glx_layout_ (w, h, pitch, bytes)
        || (size_t)w * h > SIZE_MAX / sizeof (float))
      return NULL;
    g = (mulib_glx_t *)calloc (1, sizeof (*g));
    if (!g)
      return NULL;
    g->depth = (float *)malloc ((size_t)w * h * sizeof (float));
    if (!g->depth)
      {
        free (g);
        return NULL;
      }
    g->pixels = pixels;
    g->width = w;
    g->height = h;
    g->stride = pitch / 4;
    g->x1 = (int)w;
    g->y1 = (int)h;
    for (size_t i = 0; i < (size_t)w * h; ++i)
      g->depth[i] = INFINITY;
    return g;
  }
  mulib_glx_t *
  mulib_glx_create (unsigned w, unsigned h)
  {
    uint32_t *p;
    mulib_glx_t *g;
    size_t bytes;
    if (!w || !h || w > 16384 || h > 16384 || (size_t)w * h > SIZE_MAX / 4)
      return NULL;
    bytes = (size_t)w * h * 4;
    p = (uint32_t *)calloc (1, bytes);
    if (!p)
      return NULL;
    g = mulib_glx_wrap (p, bytes, w, h, (size_t)w * 4);
    if (!g)
      {
        free (p);
        return NULL;
      }
    g->allocation = p;
    return g;
  }
  mulib_glx_t *
  mulib_glx_map_drm (mulib_drm_dumb_t *b)
  {
    if (!b || b->bpp != 32 || b->size > SIZE_MAX
        || !mulib_glx_layout_ (b->width, b->height, b->pitch, (size_t)b->size))
      return NULL;
    if (mulib_drm_dumb_map (b) != MULIB_DRM_OK)
      return NULL;
    return mulib_glx_wrap ((uint32_t *)b->data, b->mapped_size, b->width,
                           b->height, b->pitch);
  }
  void
  mulib_glx_destroy (mulib_glx_t *g)
  {
    if (g)
      {
        free (g->allocation);
        free (g->depth);
        free (g);
      }
  }
  uint32_t *
  mulib_glx_pixels (mulib_glx_t *g)
  {
    return g ? g->pixels : NULL;
  }
  unsigned
  mulib_glx_width (const mulib_glx_t *g)
  {
    return g ? g->width : 0;
  }
  unsigned
  mulib_glx_height (const mulib_glx_t *g)
  {
    return g ? g->height : 0;
  }
  size_t
  mulib_glx_pitch (const mulib_glx_t *g)
  {
    return g ? g->stride * 4 : 0;
  }
  void
  mulib_glx_clip (mulib_glx_t *g, int x, int y, int w, int h)
  {
    int64_t x1 = (int64_t)x + (w > 0 ? w : 0),
            y1 = (int64_t)y + (h > 0 ? h : 0);
    if (!g)
      return;
    g->x0 = x < 0 ? 0 : (x > (int)g->width ? (int)g->width : x);
    g->y0 = y < 0 ? 0 : (y > (int)g->height ? (int)g->height : y);
    g->x1 = x1 < g->x0 ? g->x0 : (x1 > g->width ? (int)g->width : (int)x1);
    g->y1 = y1 < g->y0 ? g->y0 : (y1 > g->height ? (int)g->height : (int)y1);
  }
  void
  mulib_glx_reset_clip (mulib_glx_t *g)
  {
    if (g)
      mulib_glx_clip (g, 0, 0, (int)g->width, (int)g->height);
  }
  void
  mulib_glx_clear (mulib_glx_t *g, uint32_t c)
  {
    if (!g)
      return;
    for (unsigned y = 0; y < g->height; ++y)
      for (unsigned x = 0; x < g->width; ++x)
        {
          g->pixels[y * g->stride + x] = c;
          g->depth[(size_t)y * g->width + x] = INFINITY;
        }
  }
  static uint32_t
  mulib_glx_blend_ (uint32_t d, uint32_t s)
  {
    unsigned sa = s >> 24, da = d >> 24, inv = 255 - sa,
             alpha = sa * 255 + da * inv;
    uint32_t out;
    if (sa == 255)
      return s;
    if (!sa)
      return d;
    out = ((alpha + 127) / 255) << 24;
    for (unsigned shift = 0; shift < 24; shift += 8)
      {
        unsigned sc = (s >> shift) & 255, dc = (d >> shift) & 255;
        out |= ((sc * sa * 255 + dc * da * inv + alpha / 2) / alpha) << shift;
      }
    return out;
  }
  void
  mulib_glx_pixel (mulib_glx_t *g, int x, int y, uint32_t c)
  {
    if (g && x >= g->x0 && x < g->x1 && y >= g->y0 && y < g->y1)
      {
        uint32_t *p = &g->pixels[(size_t)y * g->stride + (size_t)x];
        *p = mulib_glx_blend_ (*p, c);
      }
  }
  void
  mulib_glx_fill_rect (mulib_glx_t *g, int x, int y, int w, int h, uint32_t c)
  {
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int x0, y0, x1, y1;
    if (!g || w <= 0 || h <= 0 || right <= g->x0 || bottom <= g->y0
        || x >= g->x1 || y >= g->y1)
      return;
    x0 = x < g->x0 ? g->x0 : x;
    y0 = y < g->y0 ? g->y0 : y;
    x1 = right > g->x1 ? g->x1 : (int)right;
    y1 = bottom > g->y1 ? g->y1 : (int)bottom;
    for (int j = y0; j < y1; ++j)
      for (int i = x0; i < x1; ++i)
        mulib_glx_pixel (g, i, j, c);
  }
  static int
  mulib_glx_clip_line_ (double p, double q, double *lo, double *hi)
  {
    double t;
    if (p == 0)
      return q >= 0;
    t = q / p;
    if (p < 0)
      {
        if (t > *hi)
          return 0;
        if (t > *lo)
          *lo = t;
      }
    else
      {
        if (t < *lo)
          return 0;
        if (t < *hi)
          *hi = t;
      }
    return 1;
  }
  int
  mulib_glx_line (mulib_glx_t *g, mulib_linalg_vec2_t a, mulib_linalg_vec2_t b,
                  uint32_t c)
  {
    double dx = (double)b.x - a.x, dy = (double)b.y - a.y, lo = 0, hi = 1;
    int x0, y0, x1, y1, sx, sy, ix, iy, err;
    if (!g || !isfinite (a.x) || !isfinite (a.y) || !isfinite (b.x)
        || !isfinite (b.y))
      return 0;
    if (g->x0 == g->x1 || g->y0 == g->y1)
      return 1;
    if (!mulib_glx_clip_line_ (-dx, a.x - g->x0, &lo, &hi)
        || !mulib_glx_clip_line_ (dx, g->x1 - 1.0 - a.x, &lo, &hi)
        || !mulib_glx_clip_line_ (-dy, a.y - g->y0, &lo, &hi)
        || !mulib_glx_clip_line_ (dy, g->y1 - 1.0 - a.y, &lo, &hi))
      return 1;
    x0 = (int)fmax (g->x0, fmin (g->x1 - 1, round (a.x + lo * dx)));
    y0 = (int)fmax (g->y0, fmin (g->y1 - 1, round (a.y + lo * dy)));
    x1 = (int)fmax (g->x0, fmin (g->x1 - 1, round (a.x + hi * dx)));
    y1 = (int)fmax (g->y0, fmin (g->y1 - 1, round (a.y + hi * dy)));
    ix = abs (x1 - x0);
    iy = -abs (y1 - y0);
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = ix + iy;
    for (;;)
      {
        int e = 2 * err;
        mulib_glx_pixel (g, x0, y0, c);
        if (x0 == x1 && y0 == y1)
          break;
        if (e >= iy)
          {
            err += iy;
            x0 += sx;
          }
        if (e <= ix)
          {
            err += ix;
            y0 += sy;
          }
      }
    return 1;
  }
  static unsigned
  mulib_glx_channel_ (float v)
  {
    return !(v > 0) ? 0 : v >= 1 ? 255 : (unsigned)(v * 255 + 0.5f);
  }
  uint32_t
  mulib_glx_rgba (float r, float g, float b, float a)
  {
    return (mulib_glx_channel_ (a) << 24) | (mulib_glx_channel_ (r) << 16)
           | (mulib_glx_channel_ (g) << 8) | mulib_glx_channel_ (b);
  }
  static double
  mulib_glx_edge_ (mulib_glx_vertex_t a, mulib_glx_vertex_t b, double x,
                   double y)
  {
    return ((double)b.x - a.x) * (y - a.y) - ((double)b.y - a.y) * (x - a.x);
  }
  static int
  mulib_glx_top_left_ (mulib_glx_vertex_t a, mulib_glx_vertex_t b)
  {
    return b.y < a.y || (b.y == a.y && b.x > a.x);
  }
  static int
  mulib_glx_vertex_valid_ (mulib_glx_vertex_t v)
  {
    return isfinite (v.x) && isfinite (v.y) && isfinite (v.z)
           && isfinite (v.color.x) && isfinite (v.color.y)
           && isfinite (v.color.z) && isfinite (v.color.w);
  }
  static int
  mulib_glx_raster_ (mulib_glx_t *g, mulib_glx_vertex_t a,
                     mulib_glx_vertex_t b, mulib_glx_vertex_t c, int depth,
                     uint32_t flat, mulib_glx_fragment_fn shader, void *user)
  {
    double area;
    int x0, y0, x1, y1, ab, bc, ca;
    if (!g || !mulib_glx_vertex_valid_ (a) || !mulib_glx_vertex_valid_ (b)
        || !mulib_glx_vertex_valid_ (c))
      return 0;
    area = mulib_glx_edge_ (a, b, c.x, c.y);
    if (area == 0)
      return 1;
    if (area < 0)
      {
        mulib_glx_vertex_t t = b;
        b = c;
        c = t;
        area = -area;
      }
    x0 = (int)fmax (g->x0, fmin (g->x1, floor (fmin (a.x, fmin (b.x, c.x)))));
    y0 = (int)fmax (g->y0, fmin (g->y1, floor (fmin (a.y, fmin (b.y, c.y)))));
    x1 = (int)fmax (g->x0, fmin (g->x1, ceil (fmax (a.x, fmax (b.x, c.x)))));
    y1 = (int)fmax (g->y0, fmin (g->y1, ceil (fmax (a.y, fmax (b.y, c.y)))));
    ab = mulib_glx_top_left_ (a, b);
    bc = mulib_glx_top_left_ (b, c);
    ca = mulib_glx_top_left_ (c, a);
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x)
        {
          double wa = mulib_glx_edge_ (b, c, x + 0.5, y + 0.5),
                 wb = mulib_glx_edge_ (c, a, x + 0.5, y + 0.5),
                 wc = mulib_glx_edge_ (a, b, x + 0.5, y + 0.5);
          uint32_t color = flat;
          if (wa < 0 || wb < 0 || wc < 0 || (wa == 0 && !bc)
              || (wb == 0 && !ca) || (wc == 0 && !ab))
            continue;
          if (depth)
            {
              float z;
              size_t index = (size_t)y * g->width + (size_t)x;
              mulib_linalg_vec4_t v;
              wa /= area;
              wb /= area;
              wc /= area;
              z = (float)(wa * a.z + wb * b.z + wc * c.z);
              if (z >= g->depth[index])
                continue;
              v.x = (float)(wa * a.color.x + wb * b.color.x + wc * c.color.x);
              v.y = (float)(wa * a.color.y + wb * b.color.y + wc * c.color.y);
              v.z = (float)(wa * a.color.z + wb * b.color.z + wc * c.color.z);
              v.w = (float)(wa * a.color.w + wb * b.color.w + wc * c.color.w);
              color = shader ? shader (x, y, z, v, user)
                             : mulib_glx_rgba (v.x, v.y, v.z, v.w);
              if (!(color >> 24))
                continue;
              g->depth[index] = z;
            }
          mulib_glx_pixel (g, x, y, color);
        }
    return 1;
  }
  int
  mulib_glx_fill_triangle (mulib_glx_t *g, mulib_linalg_vec2_t a,
                           mulib_linalg_vec2_t b, mulib_linalg_vec2_t c,
                           uint32_t color)
  {
    mulib_glx_vertex_t va = { a.x, a.y, 0, { 0, 0, 0, 0 } },
                       vb = { b.x, b.y, 0, { 0, 0, 0, 0 } },
                       vc = { c.x, c.y, 0, { 0, 0, 0, 0 } };
    return mulib_glx_raster_ (g, va, vb, vc, 0, color, NULL, NULL);
  }
  int
  mulib_glx_triangle (mulib_glx_t *g, mulib_glx_vertex_t a,
                      mulib_glx_vertex_t b, mulib_glx_vertex_t c,
                      mulib_glx_fragment_fn shader, void *user)
  {
    return mulib_glx_raster_ (g, a, b, c, 1, 0, shader, user);
  }
  int
  mulib_glx_blit (mulib_glx_t *g, int x, int y, const uint32_t *p,
                  size_t bytes, unsigned w, unsigned h, size_t pitch)
  {
    int64_t x0, y0, x1, y1;
    if (!g || !p || !mulib_glx_layout_ (w, h, pitch, bytes))
      return 0;
    x0 = (int64_t)g->x0 - x;
    y0 = (int64_t)g->y0 - y;
    x1 = (int64_t)g->x1 - x;
    y1 = (int64_t)g->y1 - y;
    if (x0 < 0)
      x0 = 0;
    if (y0 < 0)
      y0 = 0;
    if (x1 > w)
      x1 = w;
    if (y1 > h)
      y1 = h;
    for (int64_t j = y0; j < y1; ++j)
      for (int64_t i = x0; i < x1; ++i)
        mulib_glx_pixel (g, (int)((int64_t)x + i), (int)((int64_t)y + j),
                         p[(size_t)j * (pitch / 4) + (size_t)i]);
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif

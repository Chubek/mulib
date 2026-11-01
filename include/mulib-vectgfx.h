/**
 * @file mulib-vectgfx.h
 * @brief Vector paths, Bezier/Catmull-Rom curves, elliptical arcs and path
 * language. Define MULIB_IMPL_VECTGFX plus GLX, DRM, LINALG, VEC and PARSEC
 * switches. Rendering uses adaptive flattening, even-odd fills and
 * round-cap/round-join strokes.
 */
#ifndef MULIB_VECTGFX_H
#define MULIB_VECTGFX_H
#include "mulib-glx.h"
#include "mulib-parsec.h"
#include "mulib-vec.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vectgfx mulib_vectgfx_t;
  typedef struct mulib_vectgfx_error
  {
    size_t offset;
    const char *message;
  } mulib_vectgfx_error_t;
  /** @brief Create/free/reset an owned path. NULL destroy/reset is harmless.
   */
  mulib_vectgfx_t *mulib_vectgfx_create (void);
  void mulib_vectgfx_destroy (mulib_vectgfx_t *path);
  void mulib_vectgfx_clear (mulib_vectgfx_t *path);
  /** @brief Start a contour; coordinates must be finite and within +/-1000000.
   * All path builders return 1 on success, 0 without changing the path on
   * failure. Paths are limited to 65536 commands. Line/curve/close require an
   * open contour.
   */
  int mulib_vectgfx_move_to (mulib_vectgfx_t *path, mulib_linalg_vec2_t point);
  int mulib_vectgfx_line_to (mulib_vectgfx_t *path, mulib_linalg_vec2_t point);
  int mulib_vectgfx_quad_to (mulib_vectgfx_t *path,
                             mulib_linalg_vec2_t control,
                             mulib_linalg_vec2_t end);
  int mulib_vectgfx_cubic_to (mulib_vectgfx_t *path, mulib_linalg_vec2_t a,
                              mulib_linalg_vec2_t b, mulib_linalg_vec2_t end);
  /** @brief Uniform Catmull-Rom segment from current to end, with
   * previous/next neighbors. */
  int mulib_vectgfx_catmull_to (mulib_vectgfx_t *path,
                                mulib_linalg_vec2_t previous,
                                mulib_linalg_vec2_t end,
                                mulib_linalg_vec2_t next);
  /** @brief Elliptical arc in radians, |sweep|<=2*pi, radii>0. Connect to its
   * start, or start a new contour. Uses cubic approximations of at most
   * quarter turns.
   */
  int mulib_vectgfx_arc (mulib_vectgfx_t *path, mulib_linalg_vec2_t center,
                         float rx, float ry, float start, float sweep);
  /** @brief Close and finish the current contour; next drawing command must
   * start one. */
  int mulib_vectgfx_close (mulib_vectgfx_t *path);
  /** @brief Parse absolute commands M,L,Q,C,S,A,Z and signed decimal numbers.
   * S takes previous,end,next; A takes cx,cy,rx,ry,start,sweep. Whitespace
   * separates tokens. Source<=64 KiB, empty input is an empty path. NULL on
   * diagnostic/OOM.
   */
  mulib_vectgfx_t *mulib_vectgfx_parse (const char *source, size_t length,
                                        mulib_vectgfx_error_t *error);
  /** @brief Fill all contours using even-odd coverage; open contours close
   * implicitly. tolerance is finite, 0<tolerance<=1000000. Flattening:
   * depth<=16, <=262144 edges. Allocation/flattening failure returns 0 before
   * drawing; success returns 1.
   */
  int mulib_vectgfx_fill (const mulib_vectgfx_t *path, mulib_glx_t *surface,
                          uint32_t color, float tolerance);
  /** @brief Round stroke of finite width in (0,1000000]; overlapping segments
   * blend once. Allocates a one-byte-per-pixel coverage mask, released before
   * returning.
   */
  int mulib_vectgfx_stroke (const mulib_vectgfx_t *path, mulib_glx_t *surface,
                            uint32_t color, float width, float tolerance);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_VECTGFX) && !defined(MULIB_VECTGFX_IMPLEMENTED)
#define MULIB_VECTGFX_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vectgfx_command_
  {
    char kind;
    mulib_linalg_vec2_t p[3];
  } mulib_vectgfx_command_;
  typedef struct mulib_vectgfx_edge_
  {
    mulib_linalg_vec2_t a, b;
  } mulib_vectgfx_edge_;
  struct mulib_vectgfx
  {
    mulib_vec_t commands;
    mulib_linalg_vec2_t current, start;
    int open;
  };
  mulib_vectgfx_t *
  mulib_vectgfx_create (void)
  {
    mulib_vectgfx_t *p = (mulib_vectgfx_t *)calloc (1, sizeof (*p));
    if (p)
      mulib_vec_init (&p->commands, sizeof (mulib_vectgfx_command_));
    return p;
  }
  void
  mulib_vectgfx_destroy (mulib_vectgfx_t *p)
  {
    if (p)
      {
        mulib_vec_free (&p->commands);
        free (p);
      }
  }
  void
  mulib_vectgfx_clear (mulib_vectgfx_t *p)
  {
    if (p)
      {
        mulib_vec_clear (&p->commands);
        p->open = 0;
      }
  }
  static int
  mulib_vectgfx_point_ (mulib_linalg_vec2_t p)
  {
    return isfinite (p.x) && isfinite (p.y) && fabsf (p.x) <= 1000000
           && fabsf (p.y) <= 1000000;
  }
  static int
  mulib_vectgfx_add_ (mulib_vectgfx_t *p, char kind, mulib_linalg_vec2_t a,
                      mulib_linalg_vec2_t b, mulib_linalg_vec2_t c)
  {
    mulib_vectgfx_command_ cmd;
    if (!p || p->commands.length == 65536 || !mulib_vectgfx_point_ (a)
        || !mulib_vectgfx_point_ (b) || !mulib_vectgfx_point_ (c)
        || (kind != 'M' && !p->open))
      return 0;
    cmd.kind = kind;
    cmd.p[0] = a;
    cmd.p[1] = b;
    cmd.p[2] = c;
    if (!mulib_vec_push (&p->commands, &cmd))
      return 0;
    p->current = c;
    if (kind == 'M')
      {
        p->start = c;
        p->open = 1;
      }
    if (kind == 'Z')
      p->open = 0;
    return 1;
  }
  int
  mulib_vectgfx_move_to (mulib_vectgfx_t *p, mulib_linalg_vec2_t q)
  {
    return mulib_vectgfx_add_ (p, 'M', q, q, q);
  }
  int
  mulib_vectgfx_line_to (mulib_vectgfx_t *p, mulib_linalg_vec2_t q)
  {
    return mulib_vectgfx_add_ (p, 'L', q, q, q);
  }
  int
  mulib_vectgfx_quad_to (mulib_vectgfx_t *p, mulib_linalg_vec2_t a,
                         mulib_linalg_vec2_t end)
  {
    return mulib_vectgfx_add_ (p, 'Q', a, a, end);
  }
  int
  mulib_vectgfx_cubic_to (mulib_vectgfx_t *p, mulib_linalg_vec2_t a,
                          mulib_linalg_vec2_t b, mulib_linalg_vec2_t end)
  {
    return mulib_vectgfx_add_ (p, 'C', a, b, end);
  }
  int
  mulib_vectgfx_catmull_to (mulib_vectgfx_t *p, mulib_linalg_vec2_t prev,
                            mulib_linalg_vec2_t end, mulib_linalg_vec2_t next)
  {
    mulib_linalg_vec2_t a, b;
    if (!p || !p->open || !mulib_vectgfx_point_ (prev)
        || !mulib_vectgfx_point_ (next))
      return 0;
    a = mulib_linalg_vec2_add (
        p->current,
        mulib_linalg_vec2_scale (mulib_linalg_vec2_sub (end, prev), 1.0f / 6));
    b = mulib_linalg_vec2_sub (
        end, mulib_linalg_vec2_scale (mulib_linalg_vec2_sub (next, p->current),
                                      1.0f / 6));
    return mulib_vectgfx_cubic_to (p, a, b, end);
  }
  int
  mulib_vectgfx_close (mulib_vectgfx_t *p)
  {
    return p ? mulib_vectgfx_add_ (p, 'Z', p->start, p->start, p->start) : 0;
  }
  int
  mulib_vectgfx_arc (mulib_vectgfx_t *p, mulib_linalg_vec2_t center, float rx,
                     float ry, float start, float sweep)
  {
    size_t length;
    mulib_linalg_vec2_t oldcurrent, oldstart, q;
    int open, segments;
    double step, base;
    if (!p || !mulib_vectgfx_point_ (center) || !isfinite (rx)
        || !isfinite (ry) || !isfinite (start) || !isfinite (sweep) || rx <= 0
        || ry <= 0 || rx > 1000000 || ry > 1000000
        || fabs ((double)sweep) > 6.2831855)
      return 0;
    segments = (int)ceil (fabs ((double)sweep) / 1.5707963267948966);
    if (!segments)
      segments = 1;
    base = remainder ((double)start, 6.2831853071795864769);
    step = (double)sweep / segments;
    length = p->commands.length;
    oldcurrent = p->current;
    oldstart = p->start;
    open = p->open;
    q.x = (float)(center.x + rx * cos (base));
    q.y = (float)(center.y + ry * sin (base));
    if (!(open ? mulib_vectgfx_line_to (p, q) : mulib_vectgfx_move_to (p, q)))
      return 0;
    for (int i = 0; i < segments; ++i)
      {
        double a = base + i * step, b = a + step, k = 4.0 / 3 * tan (step / 4);
        mulib_linalg_vec2_t c1, c2, end;
        c1.x = (float)(center.x + rx * (cos (a) - k * sin (a)));
        c1.y = (float)(center.y + ry * (sin (a) + k * cos (a)));
        c2.x = (float)(center.x + rx * (cos (b) + k * sin (b)));
        c2.y = (float)(center.y + ry * (sin (b) - k * cos (b)));
        end.x = (float)(center.x + rx * cos (b));
        end.y = (float)(center.y + ry * sin (b));
        if (!mulib_vectgfx_cubic_to (p, c1, c2, end))
          {
            p->commands.length = length;
            p->current = oldcurrent;
            p->start = oldstart;
            p->open = open;
            return 0;
          }
      }
    return 1;
  }
  static double
  mulib_vectgfx_distance_ (double x, double y, mulib_linalg_vec2_t a,
                           mulib_linalg_vec2_t b)
  {
    double dx = (double)b.x - a.x, dy = (double)b.y - a.y,
           d = dx * dx + dy * dy,
           t = d ? ((x - a.x) * dx + (y - a.y) * dy) / d : 0;
    t = fmax (0, fmin (1, t));
    x -= a.x + t * dx;
    y -= a.y + t * dy;
    return x * x + y * y;
  }
  static int
  mulib_vectgfx_edge_add_ (mulib_vec_t *edges, mulib_linalg_vec2_t a,
                           mulib_linalg_vec2_t b)
  {
    mulib_vectgfx_edge_ e = { a, b };
    if (a.x == b.x && a.y == b.y)
      return 1;
    return edges->length < 262144 && mulib_vec_push (edges, &e);
  }
  static mulib_linalg_vec2_t
  mulib_vectgfx_mid_ (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b)
  {
    return mulib_linalg_vec2_scale (mulib_linalg_vec2_add (a, b), 0.5f);
  }
  static int
  mulib_vectgfx_flatten_curve_ (mulib_vec_t *edges, mulib_linalg_vec2_t a,
                                mulib_linalg_vec2_t b, mulib_linalg_vec2_t c,
                                mulib_linalg_vec2_t d, double tolerance2,
                                unsigned depth)
  {
    mulib_linalg_vec2_t ab, bc, cd, abc, bcd, mid;
    if (mulib_vectgfx_distance_ (b.x, b.y, a, d) <= tolerance2
        && mulib_vectgfx_distance_ (c.x, c.y, a, d) <= tolerance2)
      return mulib_vectgfx_edge_add_ (edges, a, d);
    if (depth == 16)
      return 0;
    ab = mulib_vectgfx_mid_ (a, b);
    bc = mulib_vectgfx_mid_ (b, c);
    cd = mulib_vectgfx_mid_ (c, d);
    abc = mulib_vectgfx_mid_ (ab, bc);
    bcd = mulib_vectgfx_mid_ (bc, cd);
    mid = mulib_vectgfx_mid_ (abc, bcd);
    return mulib_vectgfx_flatten_curve_ (edges, a, ab, abc, mid, tolerance2,
                                         depth + 1)
           && mulib_vectgfx_flatten_curve_ (edges, mid, bcd, cd, d, tolerance2,
                                            depth + 1);
  }
  static int
  mulib_vectgfx_flatten_ (const mulib_vectgfx_t *p, mulib_vec_t *edges,
                          float tolerance, int fill)
  {
    mulib_linalg_vec2_t current = { 0, 0 }, start = { 0, 0 };
    int open = 0;
    mulib_vec_init (edges, sizeof (mulib_vectgfx_edge_));
    if (!p || !isfinite (tolerance) || tolerance <= 0 || tolerance > 1000000)
      return 0;
    for (size_t i = 0; i < p->commands.length; ++i)
      {
        const mulib_vectgfx_command_ *c
            = (const mulib_vectgfx_command_ *)(p->commands.data
                                               + i * sizeof (*c));
        if (c->kind == 'M')
          {
            if (fill && open
                && !mulib_vectgfx_edge_add_ (edges, current, start))
              return 0;
            start = current = c->p[2];
            open = 1;
            continue;
          }
        if (c->kind == 'Q' || c->kind == 'C')
          {
            mulib_linalg_vec2_t a = c->p[0], b = c->p[1];
            if (c->kind == 'Q')
              {
                a = mulib_linalg_vec2_add (
                    current,
                    mulib_linalg_vec2_scale (
                        mulib_linalg_vec2_sub (c->p[0], current), 2.0f / 3));
                b = mulib_linalg_vec2_add (
                    c->p[2],
                    mulib_linalg_vec2_scale (
                        mulib_linalg_vec2_sub (c->p[0], c->p[2]), 2.0f / 3));
              }
            if (!mulib_vectgfx_flatten_curve_ (edges, current, a, b, c->p[2],
                                               (double)tolerance * tolerance,
                                               0))
              return 0;
          }
        else if (!mulib_vectgfx_edge_add_ (edges, current, c->p[2]))
          return 0;
        current = c->p[2];
        if (c->kind == 'Z')
          open = 0;
      }
    return !fill || !open || mulib_vectgfx_edge_add_ (edges, current, start);
  }
  static int
  mulib_vectgfx_compare_ (const void *a, const void *b)
  {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
  }
  int
  mulib_vectgfx_fill (const mulib_vectgfx_t *p, mulib_glx_t *g, uint32_t color,
                      float tolerance)
  {
    mulib_vec_t edges;
    double *crossings;
    unsigned width = mulib_glx_width (g), height = mulib_glx_height (g);
    if (!g)
      return 0;
    if (!mulib_vectgfx_flatten_ (p, &edges, tolerance, 1))
      {
        mulib_vec_free (&edges);
        return 0;
      }
    if (!edges.length)
      {
        mulib_vec_free (&edges);
        return 1;
      }
    crossings = (double *)malloc (edges.length * sizeof (*crossings));
    if (!crossings)
      {
        mulib_vec_free (&edges);
        return 0;
      }
    for (unsigned y = 0; y < height; ++y)
      {
        size_t n = 0;
        double scan = y + 0.5;
        for (size_t i = 0; i < edges.length; ++i)
          {
            const mulib_vectgfx_edge_ *e
                = (const mulib_vectgfx_edge_ *)(edges.data + i * sizeof (*e));
            if ((e->a.y > scan) != (e->b.y > scan))
              crossings[n++] = e->a.x
                               + (scan - e->a.y) * ((double)e->b.x - e->a.x)
                                     / ((double)e->b.y - e->a.y);
          }
        qsort (crossings, n, sizeof (*crossings), mulib_vectgfx_compare_);
        for (size_t i = 0; i + 1 < n; i += 2)
          {
            int left = (int)fmax (0, fmin (width, ceil (crossings[i] - 0.5)));
            int right
                = (int)fmax (0, fmin (width, ceil (crossings[i + 1] - 0.5)));
            mulib_glx_fill_rect (g, left, (int)y, right - left, 1, color);
          }
      }
    free (crossings);
    mulib_vec_free (&edges);
    return 1;
  }
  int
  mulib_vectgfx_stroke (const mulib_vectgfx_t *p, mulib_glx_t *g,
                        uint32_t color, float width, float tolerance)
  {
    mulib_vec_t edges;
    unsigned w = mulib_glx_width (g), h = mulib_glx_height (g);
    unsigned char *mask;
    double radius = width / 2.0;
    if (!g || !isfinite (width) || width <= 0 || width > 1000000)
      return 0;
    if (!mulib_vectgfx_flatten_ (p, &edges, tolerance, 0))
      {
        mulib_vec_free (&edges);
        return 0;
      }
    if (!edges.length)
      {
        mulib_vec_free (&edges);
        return 1;
      }
    mask = (unsigned char *)calloc ((size_t)w, h);
    if (!mask)
      {
        mulib_vec_free (&edges);
        return 0;
      }
    for (size_t i = 0; i < edges.length; ++i)
      {
        const mulib_vectgfx_edge_ *e
            = (const mulib_vectgfx_edge_ *)(edges.data + i * sizeof (*e));
        int x0
            = (int)fmax (0, fmin (w, floor (fmin (e->a.x, e->b.x) - radius)));
        int y0
            = (int)fmax (0, fmin (h, floor (fmin (e->a.y, e->b.y) - radius)));
        int x1
            = (int)fmax (0, fmin (w, ceil (fmax (e->a.x, e->b.x) + radius)));
        int y1
            = (int)fmax (0, fmin (h, ceil (fmax (e->a.y, e->b.y) + radius)));
        for (int y = y0; y < y1; ++y)
          for (int x = x0; x < x1; ++x)
            if (mulib_vectgfx_distance_ (x + 0.5, y + 0.5, e->a, e->b)
                <= radius * radius)
              mask[(size_t)y * w + (size_t)x] = 1;
      }
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x)
        if (mask[(size_t)y * w + x])
          mulib_glx_pixel (g, (int)x, (int)y, color);
    free (mask);
    mulib_vec_free (&edges);
    return 1;
  }
  static int
  mulib_vectgfx_space_ (unsigned char c, void *u)
  {
    (void)u;
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  static int
  mulib_vectgfx_command_char_ (unsigned char c, void *u)
  {
    (void)u;
    return c && strchr ("MLQCSAZ", c) != NULL;
  }
  static int
  mulib_vectgfx_lex_ (const char *s, size_t n, mulib_parsec_result_t *out)
  {
    mulib_parsec_parser_t digit = mulib_parsec_range ('0', '9'),
                          digits = mulib_parsec_repeat (&digit, 1, SIZE_MAX);
    mulib_parsec_parser_t signs[2]
        = { mulib_parsec_byte ('+'), mulib_parsec_byte ('-') },
        sign = mulib_parsec_choice (signs, 2);
    mulib_parsec_parser_t fractionparts[2]
        = { mulib_parsec_byte ('.'),
            mulib_parsec_repeat (&digit, 0, SIZE_MAX) };
    mulib_parsec_parser_t fraction = mulib_parsec_sequence (fractionparts, 2);
    mulib_parsec_parser_t wholeparts[2]
        = { digits, mulib_parsec_repeat (&fraction, 0, 1) };
    mulib_parsec_parser_t shortparts[2] = { mulib_parsec_byte ('.'), digits };
    mulib_parsec_parser_t mantissas[2]
        = { mulib_parsec_sequence (wholeparts, 2),
            mulib_parsec_sequence (shortparts, 2) };
    mulib_parsec_parser_t es[2]
        = { mulib_parsec_byte ('e'), mulib_parsec_byte ('E') };
    mulib_parsec_parser_t expparts[3]
        = { mulib_parsec_choice (es, 2), mulib_parsec_repeat (&sign, 0, 1),
            digits };
    mulib_parsec_parser_t exponent = mulib_parsec_sequence (expparts, 3);
    mulib_parsec_parser_t numparts[3]
        = { mulib_parsec_repeat (&sign, 0, 1),
            mulib_parsec_choice (mantissas, 2),
            mulib_parsec_repeat (&exponent, 0, 1) };
    mulib_parsec_parser_t number = mulib_parsec_sequence (numparts, 3);
    mulib_parsec_parser_t command = mulib_parsec_satisfy (
        mulib_vectgfx_command_char_, NULL, "path command");
    mulib_parsec_parser_t space
        = mulib_parsec_satisfy (mulib_vectgfx_space_, NULL, "space");
    mulib_parsec_parser_t choices[3]
        = { mulib_parsec_capture (&command, 1),
            mulib_parsec_capture (&number, 2),
            mulib_parsec_repeat (&space, 1, SIZE_MAX) };
    mulib_parsec_parser_t token = mulib_parsec_choice (choices, 3);
    mulib_parsec_parser_t all[2]
        = { mulib_parsec_repeat (&token, 0, SIZE_MAX), mulib_parsec_end () },
        parser = mulib_parsec_sequence (all, 2);
    mulib_parsec_options_t options = { 64, 2000000, 65536 };
    return mulib_parsec_run (&parser, s, n, &options, out);
  }
  static int
  mulib_vectgfx_number_ (const char *s, size_t n, float *out)
  {
    size_t i = 0;
    double v = 0, scale = 1;
    int negative = 0, expnegative = 0, exponent = 0;
    if (n > 63)
      return 0;
    if (i < n && (s[i] == '+' || s[i] == '-'))
      negative = s[i++] == '-';
    while (i < n && s[i] >= '0' && s[i] <= '9')
      v = v * 10 + s[i++] - '0';
    if (i < n && s[i] == '.')
      {
        ++i;
        while (i < n && s[i] >= '0' && s[i] <= '9')
          {
            scale *= 0.1;
            v += (s[i++] - '0') * scale;
          }
      }
    if (i < n && (s[i] == 'e' || s[i] == 'E'))
      {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-'))
          expnegative = s[i++] == '-';
        while (i < n && s[i] >= '0' && s[i] <= '9')
          {
            if (exponent > 512)
              return 0;
            exponent = exponent * 10 + s[i++] - '0';
          }
        if (v != 0)
          v *= pow (10.0, expnegative ? -exponent : exponent);
      }
    if (i != n || !isfinite (v) || v > 1000000)
      return 0;
    *out = (float)(negative ? -v : v);
    return 1;
  }
  mulib_vectgfx_t *
  mulib_vectgfx_parse (const char *s, size_t n, mulib_vectgfx_error_t *error)
  {
    mulib_parsec_result_t tokens;
    mulib_vectgfx_error_t e = { 0, NULL };
    mulib_vectgfx_t *p = NULL;
    size_t at = 0;
    memset (&tokens, 0, sizeof tokens);
    if ((!s && n) || n > 65536)
      {
        e.message = "invalid source length";
        goto done;
      }
    if (mulib_vectgfx_lex_ (s, n, &tokens) != 1)
      {
        e.offset = tokens.error.offset;
        e.message = "invalid path token or parser limit";
        goto done;
      }
    p = mulib_vectgfx_create ();
    if (!p)
      {
        e.message = "allocation failed";
        goto done;
      }
    for (size_t i = 1; i < tokens.count; ++i)
      if (tokens.captures[i - 1].tag == 2 && tokens.captures[i].tag == 2
          && tokens.captures[i - 1].offset + tokens.captures[i - 1].length
                 == tokens.captures[i].offset)
        {
          e.offset = tokens.captures[i].offset;
          e.message = "numbers require whitespace";
          goto done;
        }
    while (at < tokens.count)
      {
        mulib_parsec_capture_t *t = &tokens.captures[at++];
        char kind = s[t->offset];
        size_t count;
        float v[6] = { 0 };
        int ok = 0;
        mulib_linalg_vec2_t a, b, c;
        e.offset = t->offset;
        if (t->tag != 1)
          {
            e.message = "expected path command";
            break;
          }
        count = kind == 'Z'                    ? 0
                : (kind == 'M' || kind == 'L') ? 2
                : kind == 'Q'                  ? 4
                                               : 6;
        for (size_t i = 0; i < count; ++i)
          {
            if (at == tokens.count)
              {
                e.offset = n;
                e.message = "missing coordinate";
                break;
              }
            t = &tokens.captures[at++];
            e.offset = t->offset;
            if (t->tag != 2
                || !mulib_vectgfx_number_ (s + t->offset, t->length, &v[i]))
              {
                e.message = "invalid coordinate";
                break;
              }
          }
        if (e.message)
          break;
        a.x = v[0];
        a.y = v[1];
        b.x = v[2];
        b.y = v[3];
        c.x = v[4];
        c.y = v[5];
        switch (kind)
          {
          case 'M':
            ok = mulib_vectgfx_move_to (p, a);
            break;
          case 'L':
            ok = mulib_vectgfx_line_to (p, a);
            break;
          case 'Q':
            ok = mulib_vectgfx_quad_to (p, a, b);
            break;
          case 'C':
            ok = mulib_vectgfx_cubic_to (p, a, b, c);
            break;
          case 'S':
            ok = mulib_vectgfx_catmull_to (p, a, b, c);
            break;
          case 'A':
            ok = mulib_vectgfx_arc (p, a, v[2], v[3], v[4], v[5]);
            break;
          case 'Z':
            ok = mulib_vectgfx_close (p);
            break;
          }
        if (!ok)
          {
            e.message = "invalid path state, geometry or allocation limit";
            break;
          }
      }
  done:
    mulib_parsec_result_free (&tokens);
    if (e.message)
      {
        mulib_vectgfx_destroy (p);
        p = NULL;
      }
    else
      e.offset = 0;
    if (error)
      *error = e;
    return p;
  }
#ifdef __cplusplus
}
#endif
#endif

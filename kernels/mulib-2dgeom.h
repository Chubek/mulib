/** @file mulib-2dgeom.h
 * @brief Double-precision Bezier/path geometry and parsec-backed vector path DSL.
 * Define MULIB_IMPL_2DGEOM, GEOMETRY, VEC and PARSEC; link libm.
 * No rendering, boolean-path engine, exact predicates or implicit SVG parsing.
 */
#ifndef MULIB_2DGEOM_H
#define MULIB_2DGEOM_H
#include "mulib-geometry.h"
#include "mulib-vec.h"
#include "mulib-parsec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_geometry_point_t mulib_2dgeom_point_t;
typedef struct mulib_2dgeom_curve { unsigned degree; mulib_2dgeom_point_t p[4]; } mulib_2dgeom_curve_t;
typedef struct mulib_2dgeom_box { mulib_2dgeom_point_t min, max; } mulib_2dgeom_box_t;
typedef struct mulib_2dgeom_segment { mulib_2dgeom_curve_t curve; int move; } mulib_2dgeom_segment_t;
/** Owned path; zero initialization works. A move flag starts a new contour.
 * Closing edges are stored explicitly, not inferred by evaluation. */
typedef struct mulib_2dgeom_path { mulib_vec_t segments; } mulib_2dgeom_path_t;
typedef struct mulib_2dgeom_polyline { mulib_2dgeom_point_t *points; size_t count; } mulib_2dgeom_polyline_t;
typedef struct mulib_2dgeom_error { size_t offset, line, column; const char *message; } mulib_2dgeom_error_t;
/** Finite coordinates |component|<=1e100, degrees 1..3. */
int mulib_2dgeom_valid(const mulib_2dgeom_curve_t *);
int mulib_2dgeom_evaluate(const mulib_2dgeom_curve_t *, double t, mulib_2dgeom_point_t *);
int mulib_2dgeom_derivatives(const mulib_2dgeom_curve_t *, double t, mulib_2dgeom_point_t *first, mulib_2dgeom_point_t *second);
/** Unit tangent and signed curvature; fail at stationary/unrepresentable points. */
int mulib_2dgeom_tangent(const mulib_2dgeom_curve_t *, double, mulib_2dgeom_point_t *);
int mulib_2dgeom_curvature(const mulib_2dgeom_curve_t *, double, double *);
/** de Casteljau split, t in [0,1]. Outputs distinct, may alias input. */
int mulib_2dgeom_split(const mulib_2dgeom_curve_t *, double, mulib_2dgeom_curve_t *, mulib_2dgeom_curve_t *);
int mulib_2dgeom_bounds(const mulib_2dgeom_curve_t *, mulib_2dgeom_box_t *);
/** Adaptive chord/control-polygon length bounds; absolute requested error.
 * Flattening bounds control-hull distance to each output chord. At most 32
 * subdivision levels and 1000000 work/leaves; failure preserves outputs. */
int mulib_2dgeom_length(const mulib_2dgeom_curve_t *, double tolerance, double *);
int mulib_2dgeom_flatten(const mulib_2dgeom_curve_t *, double tolerance, mulib_2dgeom_polyline_t *);
void mulib_2dgeom_polyline_free(mulib_2dgeom_polyline_t *);
void mulib_2dgeom_path_free(mulib_2dgeom_path_t *);
int mulib_2dgeom_path_append(mulib_2dgeom_path_t *, const mulib_2dgeom_curve_t *, int move);
size_t mulib_2dgeom_path_count(const mulib_2dgeom_path_t *);
const mulib_2dgeom_segment_t *mulib_2dgeom_path_at(const mulib_2dgeom_path_t *, size_t);
int mulib_2dgeom_path_bounds(const mulib_2dgeom_path_t *, mulib_2dgeom_box_t *);
/** Affine [a,b,c,d,e,f]: (a*x+c*y+e, b*x+d*y+f), atomic in-place. */
int mulib_2dgeom_path_transform(mulib_2dgeom_path_t *, const double matrix[6]);
/** Split at segment index/local t. Outputs must be initialized, distinct and
 * not alias input. Returned pieces are open paths, with explicit curve edges. */
int mulib_2dgeom_path_split(const mulib_2dgeom_path_t *, size_t, double,
    mulib_2dgeom_path_t *left, mulib_2dgeom_path_t *right);
/** Analytic polynomial integral, implicitly closes each contour by a line. */
int mulib_2dgeom_path_area(const mulib_2dgeom_path_t *, double *);
/** Absolute path DSL: M x y, L x y, Q cx cy x y, C c1x c1y c2x c2y x y, Z.
 * Numbers accept sign/decimal/exponent; spaces/commas separate. Empty valid.
 * M required before drawing. Complete parse, <=16 MiB/65536 segments, no
 * retained source; replaces initialized path only on success. C numeric locale. */
int mulib_2dgeom_path_parse(const char *, size_t, mulib_2dgeom_path_t *, mulib_2dgeom_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_2DGEOM) && !defined(MULIB_2DGEOM_IMPLEMENTED)
#define MULIB_2DGEOM_IMPLEMENTED
#include <float.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_2dgeom_point_valid_(mulib_2dgeom_point_t p) { return isfinite(p.x) && isfinite(p.y) && fabs(p.x) <= 1e100 && fabs(p.y) <= 1e100; }
int mulib_2dgeom_valid(const mulib_2dgeom_curve_t *c) {
  unsigned i; if (!c || c->degree < 1 || c->degree > 3) return 0;
  for (i = 0; i <= c->degree; ++i) if (!mulib_2dgeom_point_valid_(c->p[i])) return 0;
  return 1;
}
static mulib_2dgeom_point_t mulib_2dgeom_lerp_(mulib_2dgeom_point_t a, mulib_2dgeom_point_t b, double t) {
  mulib_2dgeom_point_t p = {(1 - t) * a.x + t * b.x, (1 - t) * a.y + t * b.y}; return p;
}
static mulib_2dgeom_point_t mulib_2dgeom_eval_(const mulib_2dgeom_point_t *p, unsigned degree, double t) {
  mulib_2dgeom_point_t q[4]; unsigned i, j; memcpy(q, p, (degree + 1) * sizeof *p);
  for (j = degree; j; --j) for (i = 0; i < j; ++i) q[i] = mulib_2dgeom_lerp_(q[i], q[i + 1], t);
  return q[0];
}
int mulib_2dgeom_evaluate(const mulib_2dgeom_curve_t *c, double t, mulib_2dgeom_point_t *out) {
  if (!out || !mulib_2dgeom_valid(c) || !isfinite(t) || t < 0 || t > 1) return 0;
  *out = mulib_2dgeom_eval_(c->p, c->degree, t); return 1;
}
int mulib_2dgeom_derivatives(const mulib_2dgeom_curve_t *c, double t, mulib_2dgeom_point_t *first, mulib_2dgeom_point_t *second) {
  mulib_2dgeom_point_t d[3] = {{0, 0}}, dd[2] = {{0, 0}}, a, b = {0, 0}; unsigned i;
  if (!first || !second || first == second || !mulib_2dgeom_valid(c) || !isfinite(t) || t < 0 || t > 1) return 0;
  for (i = 0; i < c->degree; ++i) { d[i].x = c->degree * (c->p[i + 1].x - c->p[i].x); d[i].y = c->degree * (c->p[i + 1].y - c->p[i].y); }
  a = mulib_2dgeom_eval_(d, c->degree - 1, t);
  if (c->degree > 1) {
    for (i = 0; i + 1 < c->degree; ++i) { dd[i].x = (c->degree - 1) * (d[i + 1].x - d[i].x); dd[i].y = (c->degree - 1) * (d[i + 1].y - d[i].y); }
    b = mulib_2dgeom_eval_(dd, c->degree - 2, t);
  }
  *first = a; *second = b; return 1;
}
int mulib_2dgeom_tangent(const mulib_2dgeom_curve_t *c, double t, mulib_2dgeom_point_t *out) {
  mulib_2dgeom_point_t a, b; double length;
  if (!out || !mulib_2dgeom_derivatives(c, t, &a, &b) || !(length = hypot(a.x, a.y))) return 0;
  a.x /= length; a.y /= length; *out = a; return 1;
}
int mulib_2dgeom_curvature(const mulib_2dgeom_curve_t *c, double t, double *out) {
  mulib_2dgeom_point_t a, b; double length, result;
  if (!out || !mulib_2dgeom_derivatives(c, t, &a, &b) || !(length = hypot(a.x, a.y))) return 0;
  result = ((a.x / length) * (b.y / length) - (a.y / length) * (b.x / length)) / length;
  if (!isfinite(result)) return 0;
  *out = result; return 1;
}
int mulib_2dgeom_split(const mulib_2dgeom_curve_t *c, double t, mulib_2dgeom_curve_t *left, mulib_2dgeom_curve_t *right) {
  mulib_2dgeom_curve_t l = {0}, r = {0}; mulib_2dgeom_point_t q[4]; unsigned i, j;
  if (!left || !right || left == right || !mulib_2dgeom_valid(c) || !isfinite(t) || t < 0 || t > 1) return 0;
  l.degree = r.degree = c->degree; memcpy(q, c->p, (c->degree + 1) * sizeof *q);
  l.p[0] = q[0]; r.p[c->degree] = q[c->degree];
  for (j = c->degree; j; --j) {
    for (i = 0; i < j; ++i) q[i] = mulib_2dgeom_lerp_(q[i], q[i + 1], t);
    l.p[c->degree - j + 1] = q[0]; r.p[j - 1] = q[j - 1];
  }
  *left = l; *right = r; return 1;
}
static void mulib_2dgeom_expand_(mulib_2dgeom_box_t *b, mulib_2dgeom_point_t p) {
  b->min.x = fmin(b->min.x, p.x); b->min.y = fmin(b->min.y, p.y);
  b->max.x = fmax(b->max.x, p.x); b->max.y = fmax(b->max.y, p.y);
}
static size_t mulib_2dgeom_roots_(double a, double b, double c, double roots[2]) {
  double disc, q, scale = fmax(fabs(a), fmax(fabs(b), fabs(c)));
  if (!scale) return 0;
  a /= scale; b /= scale; c /= scale;
  if (fabs(a) <= DBL_EPSILON * (fabs(b) + fabs(c))) {
    if (!b) return 0;
    roots[0] = -c / b; return 1;
  }
  disc = b * b - 4 * a * c; if (disc < 0) return 0;
  q = -0.5 * (b + copysign(sqrt(disc), b));
  if (!q) { roots[0] = -b / (2 * a); return 1; }
  roots[0] = q / a; roots[1] = c / q; return disc ? 2 : 1;
}
int mulib_2dgeom_bounds(const mulib_2dgeom_curve_t *c, mulib_2dgeom_box_t *out) {
  mulib_2dgeom_box_t box; unsigned axis; size_t i, n; double p[4], roots[2];
  if (!out || !mulib_2dgeom_valid(c)) return 0;
  box.min = box.max = c->p[0]; mulib_2dgeom_expand_(&box, c->p[c->degree]);
  for (axis = 0; axis < 2; ++axis) {
    for (i = 0; i <= c->degree; ++i) p[i] = axis ? c->p[i].y : c->p[i].x;
    n = c->degree == 1 ? 0 : c->degree == 2 ? mulib_2dgeom_roots_(0, p[0] - 2 * p[1] + p[2], p[1] - p[0], roots) :
      mulib_2dgeom_roots_(-p[0] + 3 * p[1] - 3 * p[2] + p[3], 2 * (p[0] - 2 * p[1] + p[2]), p[1] - p[0], roots);
    for (i = 0; i < n; ++i) if (roots[i] > 0 && roots[i] < 1) mulib_2dgeom_expand_(&box, mulib_2dgeom_eval_(c->p, c->degree, roots[i]));
  }
  *out = box; return 1;
}
static double mulib_2dgeom_segment_distance_(mulib_2dgeom_point_t p, mulib_2dgeom_point_t a, mulib_2dgeom_point_t b) {
  double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy), t;
  if (!length) return mulib_geometry_distance(p, a);
  dx /= length; dy /= length; t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / length;
  return mulib_geometry_distance(p, mulib_2dgeom_lerp_(a, b, fmax(0, fmin(1, t))));
}
static int mulib_2dgeom_subdivide_(const mulib_2dgeom_curve_t *c, double tolerance, int flatten, mulib_vec_t *points, double *length) {
  typedef struct item { mulib_2dgeom_curve_t c; double tolerance; unsigned depth; } item;
  item stack[34]; size_t top = 1, work = 0; double sum = 0;
  if (!mulib_2dgeom_valid(c) || !isfinite(tolerance) || tolerance <= 0) return 0;
  stack[0].c = *c; stack[0].tolerance = tolerance; stack[0].depth = 0;
  if (flatten && !mulib_vec_push(points, &c->p[0])) return 0;
  while (top) {
    item node = stack[--top]; unsigned i; double chord, polygon = 0, deviation = 0;
    if (++work > 1000000) return 0;
    chord = mulib_geometry_distance(node.c.p[0], node.c.p[node.c.degree]);
    for (i = 0; i < node.c.degree; ++i) polygon += mulib_geometry_distance(node.c.p[i], node.c.p[i + 1]);
    for (i = 1; i < node.c.degree; ++i) deviation = fmax(deviation, mulib_2dgeom_segment_distance_(node.c.p[i], node.c.p[0], node.c.p[node.c.degree]));
    if (flatten ? deviation <= tolerance : polygon - chord <= 2 * node.tolerance) {
      sum += (chord + polygon) * 0.5;
      if (flatten && !mulib_vec_push(points, &node.c.p[node.c.degree])) return 0;
    } else {
      if (node.depth == 32) return 0;
      mulib_2dgeom_split(&node.c, 0.5, &stack[top + 1].c, &stack[top].c);
      stack[top].depth = stack[top + 1].depth = node.depth + 1;
      stack[top].tolerance = stack[top + 1].tolerance = node.tolerance * 0.5; top += 2;
    }
  }
  if (length) *length = sum;
  return 1;
}
int mulib_2dgeom_length(const mulib_2dgeom_curve_t *c, double tolerance, double *out) {
  double length;
  if (!out || !mulib_2dgeom_subdivide_(c, tolerance, 0, NULL, &length)) return 0;
  *out = length; return 1;
}
void mulib_2dgeom_polyline_free(mulib_2dgeom_polyline_t *p) { if (p) { free(p->points); memset(p, 0, sizeof *p); } }
int mulib_2dgeom_flatten(const mulib_2dgeom_curve_t *c, double tolerance, mulib_2dgeom_polyline_t *out) {
  mulib_vec_t points; int ok;
  if (!out) return 0;
  mulib_vec_init(&points, sizeof(mulib_2dgeom_point_t)); ok = mulib_2dgeom_subdivide_(c, tolerance, 1, &points, NULL);
  if (!ok) { mulib_vec_free(&points); return 0; }
  mulib_2dgeom_polyline_free(out); out->points = (mulib_2dgeom_point_t *)points.data; out->count = points.length; return 1;
}
void mulib_2dgeom_path_free(mulib_2dgeom_path_t *p) { if (p) { mulib_vec_free(&p->segments); memset(p, 0, sizeof *p); } }
int mulib_2dgeom_path_append(mulib_2dgeom_path_t *p, const mulib_2dgeom_curve_t *c, int move) {
  mulib_2dgeom_segment_t s;
  if (!p || !mulib_2dgeom_valid(c) || p->segments.length >= 65536 ||
      (p->segments.stride && p->segments.stride != sizeof s)) return 0;
  if (!move && p->segments.length) {
    const mulib_2dgeom_segment_t *last = mulib_2dgeom_path_at(p, p->segments.length - 1);
    if (!last || !mulib_2dgeom_valid(&last->curve) ||
        last->curve.p[last->curve.degree].x != c->p[0].x || last->curve.p[last->curve.degree].y != c->p[0].y) return 0;
  }
  s.curve = *c; s.move = move != 0 || !p->segments.length;
  if (!p->segments.stride) mulib_vec_init(&p->segments, sizeof s);
  return mulib_vec_push(&p->segments, &s);
}
size_t mulib_2dgeom_path_count(const mulib_2dgeom_path_t *p) { return p ? p->segments.length : 0; }
const mulib_2dgeom_segment_t *mulib_2dgeom_path_at(const mulib_2dgeom_path_t *p, size_t i) {
  return p && p->segments.stride == sizeof(mulib_2dgeom_segment_t) && i < p->segments.length ?
    (const mulib_2dgeom_segment_t *)(p->segments.data + i * sizeof(mulib_2dgeom_segment_t)) : NULL;
}
int mulib_2dgeom_path_bounds(const mulib_2dgeom_path_t *p, mulib_2dgeom_box_t *out) {
  mulib_2dgeom_box_t result, b; size_t i;
  if (!out || !mulib_2dgeom_path_count(p)) return 0;
  for (i = 0; i < p->segments.length; ++i) {
    const mulib_2dgeom_segment_t *s = mulib_2dgeom_path_at(p, i);
    if (!s || !mulib_2dgeom_bounds(&s->curve, &b)) return 0;
    if (!i) result = b; else { mulib_2dgeom_expand_(&result, b.min); mulib_2dgeom_expand_(&result, b.max); }
  }
  *out = result; return 1;
}
int mulib_2dgeom_path_transform(mulib_2dgeom_path_t *p, const double m[6]) {
  mulib_2dgeom_path_t result = {0}; size_t i; unsigned j;
  if (!p || !m) return 0;
  for (i = 0; i < 6; ++i) if (!isfinite(m[i]) || fabs(m[i]) > 1e100) return 0;
  for (i = 0; i < p->segments.length; ++i) {
    const mulib_2dgeom_segment_t *s = mulib_2dgeom_path_at(p, i); mulib_2dgeom_curve_t c;
    if (!s || !mulib_2dgeom_valid(&s->curve)) goto fail;
    c = s->curve;
    for (j = 0; j <= c.degree; ++j) {
      double x = c.p[j].x, y = c.p[j].y;
      c.p[j].x = m[0] * x + m[2] * y + m[4]; c.p[j].y = m[1] * x + m[3] * y + m[5];
    }
    if (!mulib_2dgeom_path_append(&result, &c, s->move)) goto fail;
  }
  mulib_2dgeom_path_free(p); *p = result; return 1;
fail:
  mulib_2dgeom_path_free(&result); return 0;
}
int mulib_2dgeom_path_split(const mulib_2dgeom_path_t *p, size_t at, double t, mulib_2dgeom_path_t *left, mulib_2dgeom_path_t *right) {
  mulib_2dgeom_path_t l = {0}, r = {0}; mulib_2dgeom_curve_t a, b; size_t i;
  if (!p || !left || !right || left == right || p == left || p == right || at >= p->segments.length) return 0;
  for (i = 0; i < p->segments.length; ++i) {
    const mulib_2dgeom_segment_t *s = mulib_2dgeom_path_at(p, i);
    if (!s) goto fail;
    if (i == at) {
      if (!mulib_2dgeom_split(&s->curve, t, &a, &b) || !mulib_2dgeom_path_append(&l, &a, s->move) || !mulib_2dgeom_path_append(&r, &b, 1)) goto fail;
    } else if (!mulib_2dgeom_path_append(i < at ? &l : &r, &s->curve, s->move)) goto fail;
  }
  mulib_2dgeom_path_free(left); mulib_2dgeom_path_free(right); *left = l; *right = r; return 1;
fail:
  mulib_2dgeom_path_free(&l); mulib_2dgeom_path_free(&r); return 0;
}
static double mulib_2dgeom_cross_(mulib_2dgeom_point_t a, mulib_2dgeom_point_t b) { return a.x * b.y - a.y * b.x; }
int mulib_2dgeom_path_area(const mulib_2dgeom_path_t *p, double *out) {
  static const double nodes[] = {0.1127016653792583, 0.5, 0.8872983346207417};
  static const double weights[] = {5.0 / 18, 4.0 / 9, 5.0 / 18};
  mulib_2dgeom_point_t start = {0, 0}, last = {0, 0}; size_t i, j; double area = 0;
  if (!p || !out) return 0;
  for (i = 0; i < p->segments.length; ++i) {
    const mulib_2dgeom_segment_t *s = mulib_2dgeom_path_at(p, i);
    if (!s || !mulib_2dgeom_valid(&s->curve)) return 0;
    if (s->move) { if (i) area += mulib_2dgeom_cross_(last, start); start = s->curve.p[0]; }
    for (j = 0; j < 3; ++j) {
      mulib_2dgeom_point_t a = mulib_2dgeom_eval_(s->curve.p, s->curve.degree, nodes[j]), b, dd;
      mulib_2dgeom_derivatives(&s->curve, nodes[j], &b, &dd); area += weights[j] * mulib_2dgeom_cross_(a, b);
    }
    last = s->curve.p[s->curve.degree];
  }
  if (p->segments.length) area += mulib_2dgeom_cross_(last, start);
  *out = area * 0.5; return 1;
}
static int mulib_2dgeom_ws_(unsigned char c, void *u) { (void)u; return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ','; }
static int mulib_2dgeom_error_(mulib_2dgeom_error_t *e, const char *s, size_t at, const char *message) {
  size_t i; if (e) {
    e->offset = at; e->line = e->column = 1; e->message = message;
    for (i = 0; s && i < at; ++i) if (s[i] == '\n') { ++e->line; e->column = 1; } else ++e->column;
  } return 0;
}
int mulib_2dgeom_path_parse(const char *s, size_t n, mulib_2dgeom_path_t *out, mulib_2dgeom_error_t *e) {
  mulib_parsec_parser_t digit = mulib_parsec_range('0', '9'), anydigits = mulib_parsec_repeat(&digit, 0, SIZE_MAX);
  mulib_parsec_parser_t digits = mulib_parsec_repeat(&digit, 1, SIZE_MAX), dotparts[] = {mulib_parsec_byte('.'), anydigits};
  mulib_parsec_parser_t dot = mulib_parsec_sequence(dotparts, 2), fracparts[] = {mulib_parsec_byte('.'), digits};
  mulib_parsec_parser_t integral[] = {digits, mulib_parsec_repeat(&dot, 0, 1)};
  mulib_parsec_parser_t bases[] = {mulib_parsec_sequence(integral, 2), mulib_parsec_sequence(fracparts, 2)};
  mulib_parsec_parser_t signs[] = {mulib_parsec_byte('+'), mulib_parsec_byte('-')}, sign = mulib_parsec_choice(signs, 2);
  mulib_parsec_parser_t es[] = {mulib_parsec_byte('e'), mulib_parsec_byte('E')};
  mulib_parsec_parser_t exparts[] = {mulib_parsec_choice(es, 2), mulib_parsec_repeat(&sign, 0, 1), digits};
  mulib_parsec_parser_t exponent = mulib_parsec_sequence(exparts, 3);
  mulib_parsec_parser_t numparts[] = {mulib_parsec_repeat(&sign, 0, 1), mulib_parsec_choice(bases, 2), mulib_parsec_repeat(&exponent, 0, 1)};
  mulib_parsec_parser_t number = mulib_parsec_sequence(numparts, 3), space = mulib_parsec_satisfy(mulib_2dgeom_ws_, NULL, "path separator");
  mulib_parsec_parser_t ws = mulib_parsec_repeat(&space, 0, SIZE_MAX);
  mulib_parsec_parser_t tokenparts[] = {mulib_parsec_capture(&number, 1), ws}, token = mulib_parsec_sequence(tokenparts, 2);
  mulib_parsec_parser_t parts[5][8], command[5], choices[5], choice, allparts[3], all;
  static const char commands[] = "MLQCZ"; static const size_t arities[] = {2, 2, 4, 6, 0};
  mulib_parsec_options_t options = {256, 10000000, 524288}; mulib_parsec_result_t result;
  mulib_2dgeom_path_t path = {0}; mulib_2dgeom_point_t current = {0, 0}, start = {0, 0};
  double values[6]; size_t i, j, used = 0; int pen = 0, move = 1, rc;
  if (e) memset(e, 0, sizeof *e);
  if (!out || (!s && n) || n > 16777216) return mulib_2dgeom_error_(e, NULL, 0, "invalid path input");
  for (i = 0; i < 5; ++i) {
    parts[i][0] = mulib_parsec_byte((unsigned char)commands[i]); parts[i][1] = ws;
    for (j = 0; j < arities[i]; ++j) parts[i][j + 2] = token;
    command[i] = mulib_parsec_sequence(parts[i], arities[i] + 2); choices[i] = mulib_parsec_capture(&command[i], (size_t)commands[i]);
  }
  choice = mulib_parsec_choice(choices, 5); allparts[0] = ws; allparts[1] = mulib_parsec_repeat(&choice, 0, SIZE_MAX); allparts[2] = mulib_parsec_end();
  all = mulib_parsec_sequence(allparts, 3); rc = mulib_parsec_run(&all, s, n, &options, &result);
  if (rc != 1) { mulib_2dgeom_error_(e, s, result.error.offset, "path syntax/resource error"); mulib_parsec_result_free(&result); return 0; }
  for (i = 0; i < result.count; ++i) {
    const mulib_parsec_capture_t *cap = &result.captures[i];
    if (cap->tag == 1) {
      char number_text[128], *end;
      if (cap->length >= sizeof number_text || used == 6) goto invalid;
      memcpy(number_text, s + cap->offset, cap->length); number_text[cap->length] = 0;
      values[used] = strtod(number_text, &end); if (*end || !isfinite(values[used]) || fabs(values[used]) > 1e100) goto invalid;
      ++used;
    } else {
      mulib_2dgeom_curve_t c = {0};
      if (cap->tag == 'M') { current.x = values[0]; current.y = values[1]; start = current; pen = 1; move = 1; }
      else {
        if (!pen) goto invalid;
        c.degree = cap->tag == 'C' ? 3 : cap->tag == 'Q' ? 2 : 1; c.p[0] = current;
        if (cap->tag == 'Z') c.p[1] = start;
        else for (j = 1; j <= c.degree; ++j) { c.p[j].x = values[2 * (j - 1)]; c.p[j].y = values[2 * (j - 1) + 1]; }
        if (!mulib_2dgeom_path_append(&path, &c, move)) goto invalid;
        current = c.p[c.degree]; move = cap->tag == 'Z';
      }
      used = 0;
    }
  }
  mulib_parsec_result_free(&result); mulib_2dgeom_path_free(out); *out = path; return 1;
invalid:
  mulib_2dgeom_error_(e, s, result.captures[i].offset, "invalid path command, number or allocation limit");
  mulib_parsec_result_free(&result); mulib_2dgeom_path_free(&path); return 0;
}
#ifdef __cplusplus
}
#endif
#endif

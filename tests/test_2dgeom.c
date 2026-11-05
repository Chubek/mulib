#define MULIB_IMPL_2DGEOM
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_VEC
#define MULIB_IMPL_PARSEC
#include "mulib-2dgeom.h"
#include <assert.h>
static int near(double a, double b) { return fabs(a - b) < 1e-8; }
int main(void) {
  mulib_2dgeom_curve_t parabola = {2, {{0, 0}, {0.5, 0}, {1, 1}, {0, 0}}};
  mulib_2dgeom_curve_t arch = {2, {{0, 0}, {1, 2}, {2, 0}, {0, 0}}};
  mulib_2dgeom_curve_t c = {3, {{0, 0}, {1, 3}, {2, -2}, {4, 1}}}, left, right;
  mulib_2dgeom_point_t p, q, d, dd; mulib_2dgeom_box_t box;
  mulib_2dgeom_polyline_t poly = {0}; double k, length, area; size_t i;
  mulib_2dgeom_path_t path = {0}, l = {0}, r = {0}; mulib_2dgeom_error_t e;
  const char text[] = "M0,0 L4,0 L4,3 L0,3 Z M10 10 Q11 12 12 10 C13 10 13 12 14 12";
  assert(mulib_2dgeom_evaluate(&parabola, 0.5, &p) && near(p.x, 0.5) && near(p.y, 0.25));
  assert(mulib_2dgeom_derivatives(&parabola, 0.5, &d, &dd) && near(d.x, 1) && near(d.y, 1) && near(dd.y, 2));
  assert(mulib_2dgeom_curvature(&parabola, 0, &k) && near(k, 2));
  assert(mulib_2dgeom_tangent(&parabola, 0, &p) && near(p.x, 1) && near(p.y, 0));
  assert(mulib_2dgeom_bounds(&arch, &box) && near(box.max.y, 1) && near(box.max.x, 2));
  assert(mulib_2dgeom_split(&c, 0.37, &left, &right));
  for (i = 0; i <= 100; ++i) {
    double t = (double)i / 100;
    assert(mulib_2dgeom_evaluate(&c, t, &p));
    assert(mulib_2dgeom_evaluate(t < 0.37 ? &left : &right, t < 0.37 ? t / 0.37 : (t - 0.37) / 0.63, &q));
    assert(near(p.x, q.x) && near(p.y, q.y));
  }
  assert(mulib_2dgeom_length(&parabola, 1e-8, &length) && fabs(length - (sqrt(5.0) / 2 + asinh(2.0) / 4)) <= 1e-8);
  assert(mulib_2dgeom_flatten(&parabola, 1e-3, &poly) && poly.count > 2);
  assert(near(poly.points[0].x, 0) && near(poly.points[poly.count - 1].y, 1)); mulib_2dgeom_polyline_free(&poly);
  assert(mulib_2dgeom_path_parse(text, sizeof text - 1, &path, &e) && !e.message && mulib_2dgeom_path_count(&path) == 6);
  assert(mulib_2dgeom_path_bounds(&path, &box) && near(box.min.x, 0) && near(box.max.x, 14));
  assert(mulib_2dgeom_path_split(&path, 1, 0.5, &l, &r) && mulib_2dgeom_path_count(&l) == 2 && mulib_2dgeom_path_count(&r) == 5);
  assert(!mulib_2dgeom_path_parse("L1 2", 4, &path, &e) && e.offset == 0 && mulib_2dgeom_path_count(&path) == 6);
  assert(!mulib_2dgeom_path_parse("M0 0 C1 2", 10, &path, &e));
  assert(!mulib_2dgeom_path_parse("M0 0 Lnan 2", 11, &path, &e));
  assert(mulib_2dgeom_path_parse("M0 0 L4 0 L4 3 L0 3 Z", strlen("M0 0 L4 0 L4 3 L0 3 Z"), &path, &e));
  assert(mulib_2dgeom_path_area(&path, &area) && near(area, 12));
  { double matrix[] = {2, 0, 0, 3, 10, -20}; assert(mulib_2dgeom_path_transform(&path, matrix)); }
  assert(mulib_2dgeom_path_area(&path, &area) && near(area, 72));
  { mulib_2dgeom_curve_t zero = {1, {{0, 0}, {0, 0}, {0, 0}, {0, 0}}};
    assert(!mulib_2dgeom_tangent(&zero, 0.5, &p) && !mulib_2dgeom_curvature(&zero, 0.5, &k));
    assert(mulib_2dgeom_length(&zero, 1e-3, &length) && length == 0);
  }
  mulib_2dgeom_path_free(&path); mulib_2dgeom_path_free(&l); mulib_2dgeom_path_free(&r);
  puts("2dgeom curves/path/parsec passed"); return 0;
}

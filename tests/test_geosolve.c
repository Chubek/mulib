#define MULIB_IMPL_GEOSOLVE
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_PORT
#include "mulib-geosolve.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int
near (double a, double b)
{
  return fabs (a - b) <= 1e-7 * fmax (1, fmax (fabs (a), fabs (b)));
}
static mulib_geosolve_constraint_t
sequential (mulib_geosolve_kind_t kind)
{
  mulib_geosolve_constraint_t c = mulib_geosolve_constraint (kind);
  for (size_t i = 0; i < 12; ++i)
    c.indices[i] = i;
  return c;
}
static void
residual (mulib_geosolve_kind_t kind, double *values, size_t n,
          double target, const double *expected, size_t rows)
{
  mulib_geosolve_constraint_t c = sequential (kind);
  unsigned char fixed[12];
  double before[12], cost = 0, maximum = 0;
  mulib_geosolve_problem_t p = { values, n, fixed, &c, 1 };
  mulib_geosolve_result_t r;
  c.value = target;
  memset (fixed, 1, sizeof fixed);
  memcpy (before, values, n * sizeof (double));
  for (size_t i = 0; i < rows; ++i)
    { cost += 0.5 * expected[i] * expected[i]; maximum = fmax (maximum, fabs (expected[i])); }
  assert (mulib_geosolve_solve (&p, NULL, &r)
          == (maximum < 1e-8 ? MULIB_GEOSOLVE_SOLVED : MULIB_GEOSOLVE_STALLED));
  assert (near (r.cost, cost) && near (r.max_residual, maximum));
  assert (r.residual_count == rows && r.free_variables == 0 && r.rank == 0
          && r.degrees_of_freedom == 0 && r.worst_constraint == 0);
  assert (!memcmp (before, values, n * sizeof (double)));
}
static void
builtin_residuals (void)
{
  const double one[] = { 1 }, three[] = { 3 }, zero[] = { 0 };
  const double pi = acos (-1.0), quarter[] = { 0.78539816339744830962 };
  double value[] = { 3 }, equal[] = { 4, 1 };
  double distance2[] = { 0, 0, 3, 4 }, distance3[] = { 0, 0, 0, 2, 3, 6 };
  double coincident2[] = { 3, 2, 1, 1 }, coincident3[] = { 3, 2, 1, 1, 1, 0 };
  double midpoint2[] = { 3, 3, 0, 0, 4, 2 }, midpoint3[] = { 3, 3, 3, 0, 0, 0, 4, 2, 2 };
  double line[] = { 2, 3, 0, 0, 4, 0 };
  double plane[] = { 2, 3, 4, 0, 0, 1, 1, 0, 1, 0, 1, 1 };
  double segments2[] = { 0, 0, 1, 0, 0, 0, 0, 1 };
  double segments3[] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0 };
  double equal_length2[] = { 0, 0, 3, 4, 0, 0, 0, 3 };
  double equal_length3[] = { 0, 0, 0, 2, 3, 6, 0, 0, 0, 2, 0, 0 };
  double circle[] = { 3, 4, 0, 0, 4 }, sphere[] = { 2, 3, 6, 0, 0, 0, 6 };
  double tangent[] = { 0, 0, 2, 5, 0, 2 }, triangle[] = { 0, 0, 4, 0, 0, 2 };
  const double delta2[] = { 2, 1 }, delta3[] = { 2, 1, 1 };
  const double mid2[] = { 1, 2 }, mid3[] = { 1, 2, 2 }, cross3[] = { 0, 0, 1 }, four[] = { 4 };
  residual (MULIB_GEOSOLVE_VALUE, value, 1, 2, one, 1);
  residual (MULIB_GEOSOLVE_EQUAL, equal, 2, 2, one, 1);
  residual (MULIB_GEOSOLVE_DISTANCE2, distance2, 4, 4, one, 1);
  residual (MULIB_GEOSOLVE_DISTANCE3, distance3, 6, 6, one, 1);
  residual (MULIB_GEOSOLVE_COINCIDENT2, coincident2, 4, 0, delta2, 2);
  residual (MULIB_GEOSOLVE_COINCIDENT3, coincident3, 6, 0, delta3, 3);
  residual (MULIB_GEOSOLVE_MIDPOINT2, midpoint2, 6, 0, mid2, 2);
  residual (MULIB_GEOSOLVE_MIDPOINT3, midpoint3, 9, 0, mid3, 3);
  residual (MULIB_GEOSOLVE_POINT_ON_LINE2, line, 6, 0, three, 1);
  residual (MULIB_GEOSOLVE_POINT_ON_PLANE3, plane, 12, 0, three, 1);
  residual (MULIB_GEOSOLVE_PARALLEL2, segments2, 8, 0, one, 1);
  residual (MULIB_GEOSOLVE_PARALLEL3, segments3, 12, 0, cross3, 3);
  residual (MULIB_GEOSOLVE_PERPENDICULAR2, segments2, 8, 0, zero, 1);
  residual (MULIB_GEOSOLVE_PERPENDICULAR3, segments3, 12, 0, zero, 1);
  residual (MULIB_GEOSOLVE_ANGLE2, segments2, 8, pi / 4, quarter, 1);
  residual (MULIB_GEOSOLVE_ANGLE3, segments3, 12, pi / 4, quarter, 1);
  residual (MULIB_GEOSOLVE_EQUAL_LENGTH2, equal_length2, 8, 1, one, 1);
  residual (MULIB_GEOSOLVE_EQUAL_LENGTH3, equal_length3, 12, 1, four, 1);
  residual (MULIB_GEOSOLVE_POINT_ON_CIRCLE2, circle, 5, 0, one, 1);
  residual (MULIB_GEOSOLVE_POINT_ON_SPHERE3, sphere, 7, 0, one, 1);
  residual (MULIB_GEOSOLVE_TANGENT_EXTERNAL2, tangent, 6, 0, one, 1);
  tangent[2] = 6;
  residual (MULIB_GEOSOLVE_TANGENT_INTERNAL2, tangent, 6, 0, one, 1);
  residual (MULIB_GEOSOLVE_TRIANGLE_AREA2, triangle, 6, 3, one, 1);
  segments2[6] = -1; segments2[7] = 0;
  residual (MULIB_GEOSOLVE_ANGLE2, segments2, 8, -pi, zero, 1);
}
static void
triangle_solve (void)
{
  double v[] = { 0, -0.0, 3, 0, 1, 2 }, before[4];
  unsigned char fixed[] = { 1, 1, 1, 1, 0, 0 };
  mulib_geosolve_constraint_t c[2];
  mulib_geosolve_problem_t p = { v, 6, fixed, c, 2 };
  mulib_geosolve_result_t r;
  c[0] = sequential (MULIB_GEOSOLVE_DISTANCE2);
  c[0].indices[2] = 4; c[0].indices[3] = 5; c[0].value = 4;
  c[1] = sequential (MULIB_GEOSOLVE_DISTANCE2);
  c[1].indices[0] = 2; c[1].indices[1] = 3;
  c[1].indices[2] = 4; c[1].indices[3] = 5; c[1].value = 5;
  memcpy (before, v, sizeof before);
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (near (v[4], 0) && near (v[5], 4));
  assert (!memcmp (before, v, sizeof before));
  assert (r.rank == 2 && r.degrees_of_freedom == 0 && r.free_variables == 2
          && r.residual_count == 2 && r.iterations > 0 && r.evaluations > r.iterations);
  v[4] = 1; v[5] = -2;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_SOLVED);
  assert (near (v[4], 0) && near (v[5], -4));
}
static void
trilateration (void)
{
  double v[] = { 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 8, 2, 3, 4 };
  const double squared[] = { 14, 22, 26, 30 };
  unsigned char fixed[15] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 };
  mulib_geosolve_constraint_t c[4];
  mulib_geosolve_problem_t p = { v, 15, fixed, c, 4 };
  mulib_geosolve_result_t r;
  for (size_t i = 0; i < 4; ++i)
    {
      c[i] = mulib_geosolve_constraint (MULIB_GEOSOLVE_DISTANCE3);
      for (size_t j = 0; j < 3; ++j)
        { c[i].indices[j] = 3 * i + j; c[i].indices[3 + j] = 12 + j; }
      c[i].value = sqrt (squared[i]);
    }
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (near (v[12], 1) && near (v[13], 2) && near (v[14], 3));
  assert (r.rank == 3 && r.residual_count == 4 && r.degrees_of_freedom == 0);
}
static void
rank_and_weighting (void)
{
  double x = 0;
  mulib_geosolve_constraint_t c[2];
  mulib_geosolve_problem_t p = { &x, 1, NULL, c, 2 };
  mulib_geosolve_result_t r;
  c[0] = mulib_geosolve_constraint (MULIB_GEOSOLVE_VALUE);
  c[1] = c[0];
  c[0].value = 1; c[1].value = 3; c[1].weight = 3;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_STALLED);
  assert (near (x, 2.5) && near (r.cost, 1.5) && near (r.max_residual, 1.5));
  assert (r.rank == 1 && r.degrees_of_freedom == 0 && r.worst_constraint == 0);
  c[0].value = c[1].value = 2;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (near (x, 2) && r.rank == 1 && r.residual_count == 2);
  {
    double point[] = { 0, 0, 3, 1 };
    unsigned char fixed[] = { 1, 1, 0, 0 };
    mulib_geosolve_constraint_t distance = sequential (MULIB_GEOSOLVE_DISTANCE2);
    mulib_geosolve_problem_t q = { point, 4, fixed, &distance, 1 };
    distance.value = 5;
    assert (mulib_geosolve_solve (&q, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
    assert (near (hypot (point[2], point[3]), 5) && r.rank == 1 && r.degrees_of_freedom == 1);
  }
}
static int
custom_equations (const double *x, size_t n, double *r, size_t m, void *user)
{
  (void)user;
  assert (n == 2 && m == 2);
  r[0] = x[0] * x[0] + x[1] * x[1] - 25;
  r[1] = x[0] - x[1] - 1;
  return 1;
}
static int
constant_residuals (const double *x, size_t n, double *r, size_t m, void *user)
{
  (void)x; (void)n;
  for (size_t i = 0; i < m; ++i)
    r[i] = user ? *(const double *)user : 0;
  return 1;
}
static int
bad_callback (const double *x, size_t n, double *r, size_t m, void *user)
{
  (void)x; (void)n; (void)r; (void)m;
  return user != NULL; /* Either reject, or leave poisoned residuals unwritten. */
}
static int
isolated_domain (const double *x, size_t n, double *r, size_t m, void *user)
{
  (void)n; (void)m; (void)user;
  r[0] = 1;
  return x[0] == 0;
}
static void
custom_and_domain (void)
{
  double v[] = { 3, 2 };
  mulib_geosolve_constraint_t c = mulib_geosolve_constraint (MULIB_GEOSOLVE_CUSTOM);
  mulib_geosolve_problem_t p = { v, 2, NULL, &c, 1 };
  mulib_geosolve_result_t r;
  c.residual = custom_equations; c.residual_count = 2;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (near (v[0], 4) && near (v[1], 3) && r.rank == 2);
  c.residual = bad_callback;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_EVALUATION_ERROR);
  assert (near (v[0], 4) && near (v[1], 3));
  c.user = &p;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_EVALUATION_ERROR);
  c.residual = isolated_domain; c.residual_count = 1; v[0] = 0;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_EVALUATION_ERROR);
  assert (v[0] == 0 && near (v[1], 3));
  {
    /* Radius starts on the edge of its nonnegative domain; derivative probing
     * must use the valid side rather than failing the whole solve. */
    double circle[] = { 2, 0, 0, 0, 0 };
    unsigned char fixed[] = { 1, 1, 1, 1, 0 };
    mulib_geosolve_constraint_t on_circle = sequential (MULIB_GEOSOLVE_POINT_ON_CIRCLE2);
    mulib_geosolve_problem_t q = { circle, 5, fixed, &on_circle, 1 };
    assert (mulib_geosolve_solve (&q, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
    assert (near (circle[4], 2));
    circle[4] = -1;
    assert (mulib_geosolve_solve (&q, NULL, &r) == MULIB_GEOSOLVE_EVALUATION_ERROR);
    assert (circle[4] == -1);
  }
}
static void
empty_limits_and_validation (void)
{
  mulib_geosolve_problem_t p = { NULL, 0, NULL, NULL, 0 };
  mulib_geosolve_result_t r;
  mulib_geosolve_options_t o = mulib_geosolve_default_options ();
  mulib_geosolve_constraint_t c = mulib_geosolve_constraint (MULIB_GEOSOLVE_VALUE);
  double x = -0.0, before = x;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (r.rank == 0 && r.degrees_of_freedom == 0 && r.worst_constraint == SIZE_MAX);
  p.values = &x; p.variable_count = 1;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (r.degrees_of_freedom == 1 && !memcmp (&x, &before, sizeof x));
  p.constraints = &c; p.constraint_count = 1; c.value = 1;
  o.max_iterations = 0;
  assert (mulib_geosolve_solve (&p, &o, &r) == MULIB_GEOSOLVE_ITERATION_LIMIT);
  assert (!memcmp (&x, &before, sizeof x) && r.iterations == 0 && near (r.max_residual, 1));
  o.max_iterations = 1;
  assert (mulib_geosolve_solve (&p, &o, &r) == MULIB_GEOSOLVE_ITERATION_LIMIT);
  assert (x > 0 && x < 1 && r.iterations == 1);
  c.indices[0] = 1; before = x;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_INVALID);
  assert (!memcmp (&x, &before, sizeof x));
  c.indices[0] = 0; c.weight = 0;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  c.weight = NAN;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  c.weight = 1; c.kind = (mulib_geosolve_kind_t)999;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  c = mulib_geosolve_constraint (MULIB_GEOSOLVE_VALUE); c.value = INFINITY;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  c.value = 1; x = NAN;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  x = 0; p.values = NULL;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  p.values = &x; p.constraints = NULL;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  p.constraints = &c; p.variable_count = SIZE_MAX;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  p.variable_count = 1; p.constraint_count = SIZE_MAX;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  p.constraint_count = 1;
  o = mulib_geosolve_default_options (); o.difference_step = 0;
  assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_INVALID);
  o = mulib_geosolve_default_options (); o.rank_tolerance = 1;
  assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_INVALID);
  o = mulib_geosolve_default_options (); o.residual_tolerance = NAN;
  assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_INVALID);
  assert (mulib_geosolve_solve (NULL, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  for (int status = -4; status <= 2; ++status)
    assert (mulib_geosolve_status_string ((mulib_geosolve_status_t)status)[0]);
  assert (mulib_geosolve_status_string ((mulib_geosolve_status_t)99)[0]);
  {
    double values[MULIB_GEOSOLVE_MAX_FREE_VARIABLES + 1] = { 0 };
    mulib_geosolve_problem_t q = { values, MULIB_GEOSOLVE_MAX_FREE_VARIABLES, NULL, &c, 1 };
    c.value = 0; c.indices[0] = MULIB_GEOSOLVE_MAX_FREE_VARIABLES - 1;
    assert (mulib_geosolve_solve (&q, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
    assert (r.rank == 1 && r.degrees_of_freedom == MULIB_GEOSOLVE_MAX_FREE_VARIABLES - 1);
    ++q.variable_count;
    assert (mulib_geosolve_solve (&q, NULL, &r) == MULIB_GEOSOLVE_INVALID);
  }
  c = mulib_geosolve_constraint (MULIB_GEOSOLVE_CUSTOM);
  c.residual = constant_residuals; c.residual_count = MULIB_GEOSOLVE_MAX_RESIDUALS;
  p.values = NULL; p.variable_count = 0;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (r.residual_count == MULIB_GEOSOLVE_MAX_RESIDUALS && r.free_variables == 0);
  ++c.residual_count;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_INVALID);
  c.residual_count = 1; c.user = &before; before = 2;
  assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_STALLED);
  assert (near (r.cost, 2) && near (r.max_residual, 2));
  c.residual = NULL;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  c.residual = constant_residuals; c.residual_count = 0;
  assert (mulib_geosolve_solve (&p, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
  {
    double v[12] = { 0 };
    mulib_geosolve_constraint_t invalid = sequential (MULIB_GEOSOLVE_DISTANCE2);
    mulib_geosolve_problem_t q = { v, 12, NULL, &invalid, 1 };
    invalid.value = -1;
    assert (mulib_geosolve_solve (&q, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
    invalid = sequential (MULIB_GEOSOLVE_ANGLE2); invalid.value = 4;
    assert (mulib_geosolve_solve (&q, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
    invalid = sequential (MULIB_GEOSOLVE_ANGLE3); invalid.value = -1;
    assert (mulib_geosolve_solve (&q, NULL, NULL) == MULIB_GEOSOLVE_INVALID);
    invalid = sequential (MULIB_GEOSOLVE_POINT_ON_LINE2);
    assert (mulib_geosolve_solve (&q, NULL, NULL) == MULIB_GEOSOLVE_EVALUATION_ERROR);
    invalid = sequential (MULIB_GEOSOLVE_POINT_ON_PLANE3);
    assert (mulib_geosolve_solve (&q, NULL, NULL) == MULIB_GEOSOLVE_EVALUATION_ERROR);
  }
}
typedef struct allocation_state
{
  size_t calls, fail_at, live;
} allocation_state;
static void *
allocate (void *user, size_t n)
{
  allocation_state *s = (allocation_state *)user;
  void *p;
  if (++s->calls == s->fail_at)
    return NULL;
  p = malloc (n);
  if (p) ++s->live;
  return p;
}
static void
deallocate (void *user, void *p)
{
  allocation_state *s = (allocation_state *)user;
  assert (p && s->live);
  --s->live;
  free (p);
}
static void
allocation_failures (void)
{
  allocation_state state = { 0, 0, 0 };
  mulib_allocator_t allocator = { &state, allocate, NULL, deallocate };
  mulib_geosolve_options_t o = mulib_geosolve_default_options ();
  mulib_geosolve_constraint_t c = mulib_geosolve_constraint (MULIB_GEOSOLVE_VALUE);
  double x = 0;
  mulib_geosolve_problem_t p = { &x, 1, NULL, &c, 1 };
  o.allocator = &allocator; c.value = 2;
  assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_SOLVED);
  assert (near (x, 2) && state.calls == 2 && state.live == 0);
  for (size_t failure = 1; failure <= 2; ++failure)
    {
      state.calls = 0; state.fail_at = failure; x = 0;
      assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_NO_MEMORY);
      assert (x == 0 && state.live == 0);
    }
  state.fail_at = 0; allocator.allocate = NULL;
  assert (mulib_geosolve_solve (&p, &o, NULL) == MULIB_GEOSOLVE_INVALID);
  assert (state.live == 0);
}
static int
linear_system (const double *x, size_t n, double *r, size_t m, void *user)
{
  const double *matrix = (const double *)user;
  for (size_t i = 0; i < m; ++i)
    {
      r[i] = -matrix[i * (n + 1) + n];
      for (size_t j = 0; j < n; ++j)
        r[i] += matrix[i * (n + 1) + j] * x[j];
    }
  return 1;
}
static void
linear_algebra_regressions (void)
{
  /* An overdetermined system with differently scaled columns. Its exact
   * solution is (2,-3,4); the last row is redundant, not an extra freedom. */
  double matrix[] = { 1e-4, 2, 0, -5.9998,
                     2e-4, 0, 3, 12.0004,
                     0, 1, 1, 1,
                     3e-4, 3, 4, 7.0006 };
  double x[3] = { 0, 0, 0 };
  mulib_geosolve_constraint_t c = mulib_geosolve_constraint (MULIB_GEOSOLVE_CUSTOM);
  mulib_geosolve_problem_t p = { x, 3, NULL, &c, 1 };
  mulib_geosolve_result_t r;
  mulib_geosolve_options_t o = mulib_geosolve_default_options ();
  /* Residual tolerances must be tighter than coordinate tolerances when a
   * column has a tiny coefficient. Satisfaction alone is not accuracy in x. */
  o.residual_tolerance = 1e-12;
  c.residual = linear_system; c.residual_count = 4; c.user = matrix;
  assert (mulib_geosolve_solve (&p, &o, &r) == MULIB_GEOSOLVE_SOLVED);
  assert (near (x[0], 2) && near (x[1], -3) && near (x[2], 4));
  assert (r.rank == 3 && r.degrees_of_freedom == 0);
  {
    /* A genuinely rank-deficient system: both rows describe the same plane. */
    double dependent[] = { 1, 2, 3, 14, 2, 4, 6, 28 };
    c.residual_count = 2; c.user = dependent;
    memset (x, 0, sizeof x);
    assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_SOLVED);
    assert (near (x[0] + 2 * x[1] + 3 * x[2], 14));
    assert (r.rank == 1 && r.degrees_of_freedom == 2);
  }
  {
    /* Nonfinite residual arithmetic is an evaluation error, never success. */
    double overflow[] = { DBL_MAX, 0 };
    c.residual_count = 1; c.user = overflow; p.variable_count = 1; x[0] = 2;
    assert (mulib_geosolve_solve (&p, NULL, &r) == MULIB_GEOSOLVE_EVALUATION_ERROR);
    assert (x[0] == 2);
  }
}
static void
analytic2 (void)
{
  mulib_geosolve_line2_t a = { { 0, 0 }, { 2, 0 } }, b = { { 1, -2 }, { 0, 3 } };
  mulib_geometry_point_t p[2] = { { 99, 98 }, { 97, 96 } }, before[2];
  mulib_geometry_point_t origin = { 0, 0 }, end = { 4, 0 }, c = { 2, 0 }, d = { 6, 0 };
  mulib_geosolve_circle2_t circle = { { 0, 0 }, 2 }, other = { { 3, 0 }, 1 }, constructed, saved;
  assert (mulib_geosolve_line_line2 (a, b, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (p[0].x, 1) && near (p[0].y, 0));
  b.direction = a.direction; b.origin.y = 1;
  memcpy (before, p, sizeof p);
  assert (mulib_geosolve_line_line2 (a, b, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  assert (!memcmp (before, p, sizeof p));
  b.origin.y = 0;
  assert (mulib_geosolve_line_line2 (a, b, p) == MULIB_GEOSOLVE_INTERSECTION_INFINITE);
  b.direction.x = 0;
  assert (mulib_geosolve_line_line2 (a, b, p) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  assert (!memcmp (before, p, sizeof p));
  assert (mulib_geosolve_segment_segment2 (origin, end, c, d, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
  assert (p[0].x == 2 && p[1].x == 4);
  assert (mulib_geosolve_segment_segment2 (end, origin, c, d, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
  assert (p[0].x == 4 && p[1].x == 2);
  assert (mulib_geosolve_segment_segment2 (origin, end, end, d, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (p[0].x == 4);
  assert (mulib_geosolve_segment_segment2 (c, c, origin, end, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (p[0].x == 2);
  assert (mulib_geosolve_segment_segment2 (c, c, c, c, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (mulib_geosolve_segment_segment2 (origin, origin, c, c, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  c.x = 2; c.y = -1; d.x = 2; d.y = 1;
  assert (mulib_geosolve_segment_segment2 (origin, end, c, d, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (p[0].x, 2) && near (p[0].y, 0));
  d.y = -0.1;
  assert (mulib_geosolve_segment_segment2 (origin, end, c, d, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  assert (mulib_geosolve_line_circle2 (a, circle, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
  assert (near (p[0].x, -2) && near (p[1].x, 2) && p[0].y == 0);
  a.origin.y = 2;
  assert (mulib_geosolve_line_circle2 (a, circle, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (p[0].x == 0 && p[0].y == 2);
  a.origin.y = 3;
  memcpy (before, p, sizeof p);
  assert (mulib_geosolve_line_circle2 (a, circle, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  assert (!memcmp (before, p, sizeof p));
  /* A distant origin must not turn a miss into a tangent via cancellation. */
  a.origin.x = 1e16;
  assert (mulib_geosolve_line_circle2 (a, circle, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  a.origin.y = 0; a.direction.x = DBL_TRUE_MIN;
  assert (mulib_geosolve_line_circle2 (a, circle, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
  assert (near (p[0].x, -2) && near (p[1].x, 2));
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (p[0].x, 2) && near (p[0].y, 0));
  other.center.x = 2; other.radius = 2;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
  assert (near (p[0].x, 1) && near (p[0].y, sqrt (3)) && near (p[1].y, -sqrt (3)));
  other = circle;
  memcpy (before, p, sizeof p);
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_INFINITE);
  assert (!memcmp (before, p, sizeof p));
  other.radius = 1;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  other.center.x = 1;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (p[0].x, 2) && near (p[0].y, 0));
  other.radius = 0; other.center.x = 2;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  a.origin.y = 0;
  assert (mulib_geosolve_line_circle2 (a, other, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (p[0].x == 2 && p[0].y == 0);
  circle = other;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  other.radius = -1;
  assert (mulib_geosolve_circle_circle2 (circle, other, p) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  c.x = 0; c.y = 2;
  assert (mulib_geosolve_circumcircle2 (origin, end, c, &constructed));
  assert (near (constructed.center.x, 2) && near (constructed.center.y, 1)
          && near (constructed.radius, sqrt (5)));
  saved = constructed; c = end;
  assert (!mulib_geosolve_circumcircle2 (origin, end, c, &constructed));
  assert (!memcmp (&saved, &constructed, sizeof saved));
  c.x = NAN;
  assert (!mulib_geosolve_circumcircle2 (origin, end, c, &constructed));
  assert (mulib_geosolve_segment_segment2 (origin, end, c, d, p) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  assert (mulib_geosolve_line_circle2 (a, circle, NULL) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  assert (mulib_geosolve_line_line2 (a, a, NULL) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  {
    mulib_geosolve_circle2_t huge = { { 0, 0 }, 1e300 }, close = { { 1, 0 }, 1e300 };
    assert (mulib_geosolve_circle_circle2 (huge, close, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
    assert (near (p[0].x, 0.5) && near (p[0].y / huge.radius, 1));
    huge.radius = close.radius = 1; close.center.x = 1e-200;
    assert (mulib_geosolve_circle_circle2 (huge, close, p) == MULIB_GEOSOLVE_INTERSECTION_TWO);
    assert (near (p[0].x / 1e-200, 0.5) && near (p[0].y, 1));
    huge.radius = 3; close.radius = 2; close.center.x = 1;
    assert (mulib_geosolve_circle_circle2 (huge, close, p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
    assert (near (p[0].x, 3) && near (p[0].y, 0));
  }
}
static uint32_t seed = 0x15ab3du;
static int
coordinate (void)
{
  seed = seed * UINT32_C (1664525) + UINT32_C (1013904223);
  return (int)((seed >> 16) % 41) - 20;
}
static void
analytic_properties (void)
{
  for (size_t i = 0; i < 2000; ++i)
    {
      mulib_linalg_vec2_t v[4];
      mulib_geometry_point_t points[4], intersection[2];
      mulib_geosolve_intersection_t status;
      for (size_t j = 0; j < 4; ++j)
        {
          v[j].x = (float)coordinate (); v[j].y = (float)coordinate ();
          points[j].x = v[j].x; points[j].y = v[j].y;
        }
      status = mulib_geosolve_segment_segment2 (points[0], points[1], points[2], points[3], intersection);
      assert (status >= 0);
      assert ((status != MULIB_GEOSOLVE_INTERSECTION_NONE)
              == mulib_cgeom_segments_intersect (v[0], v[1], v[2], v[3]));
      for (int j = 0; j < (int)status; ++j)
        {
          for (size_t k = 0; k < 2; ++k)
            {
              /* Independent distance-sum check that each result lies on both
               * closed segments, not merely their supporting lines. */
              double ab = mulib_geometry_distance (points[2 * k], points[2 * k + 1]);
              double ap = mulib_geometry_distance (points[2 * k], intersection[j]);
              double pb = mulib_geometry_distance (intersection[j], points[2 * k + 1]);
              assert (near (ab, ap + pb));
            }
        }
      {
        mulib_geosolve_circle2_t a = { points[0], abs (coordinate ()) },
                                 b = { points[1], abs (coordinate ()) };
        double dx = a.center.x - b.center.x, dy = a.center.y - b.center.y;
        double squared = dx * dx + dy * dy;
        double sum = a.radius + b.radius, difference = a.radius - b.radius;
        int expected;
        if (squared == 0 && a.radius == b.radius)
          expected = a.radius == 0 ? 1 : 3;
        else if (squared > sum * sum || squared < difference * difference)
          expected = 0;
        else if (squared == sum * sum || squared == difference * difference)
          expected = 1;
        else
          expected = 2;
        status = mulib_geosolve_circle_circle2 (a, b, intersection);
        assert ((int)status == expected);
        if (status == MULIB_GEOSOLVE_INTERSECTION_ONE || status == MULIB_GEOSOLVE_INTERSECTION_TWO)
          for (int j = 0; j < (int)status; ++j)
            {
              assert (near (mulib_geometry_distance (a.center, intersection[j]), a.radius));
              assert (near (mulib_geometry_distance (b.center, intersection[j]), b.radius));
            }
      }
    }
}
static void
analytic3 (void)
{
  mulib_cgeom_triangle_t triangle = { { 0, 0, 2 }, { 1, 0, 2 }, { 0, 1, 2 } };
  mulib_geosolve_plane3_t a, b = { { 2, 0, 0 }, 6 }, saved;
  mulib_cgeom_ray_t ray = { { 1, 1, 0 }, { 0, 0, -2 } }, line, before;
  mulib_linalg_vec3_t p;
  assert (mulib_geosolve_plane_from_triangle3 (triangle, &a));
  assert (near (a.normal.z, 1) && near (a.offset, 2));
  assert (mulib_geosolve_line_plane3 (ray, a, &p) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (p.x, 1) && near (p.y, 1) && near (p.z, 2));
  ray.direction.z = 0; ray.direction.x = 1;
  assert (mulib_geosolve_line_plane3 (ray, a, &p) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  ray.origin.z = 2;
  assert (mulib_geosolve_line_plane3 (ray, a, &p) == MULIB_GEOSOLVE_INTERSECTION_INFINITE);
  assert (mulib_geosolve_plane_plane3 (a, b, &line) == MULIB_GEOSOLVE_INTERSECTION_ONE);
  assert (near (line.origin.x, 3) && near (line.origin.z, 2)
          && near (fabs (line.direction.y), 1));
  before = line; b.normal.x = 0; b.normal.z = -2; b.offset = -4;
  assert (mulib_geosolve_plane_plane3 (a, b, &line) == MULIB_GEOSOLVE_INTERSECTION_INFINITE);
  assert (!memcmp (&before, &line, sizeof before));
  b.offset = -5;
  assert (mulib_geosolve_plane_plane3 (a, b, &line) == MULIB_GEOSOLVE_INTERSECTION_NONE);
  b.normal.z = 0;
  assert (mulib_geosolve_plane_plane3 (a, b, &line) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
  saved = a; triangle.c = triangle.a;
  assert (!mulib_geosolve_plane_from_triangle3 (triangle, &a));
  assert (!memcmp (&saved, &a, sizeof saved));
  ray.direction.x = NAN;
  assert (mulib_geosolve_line_plane3 (ray, a, &p) == MULIB_GEOSOLVE_INTERSECTION_ERROR);
}
int
main (void)
{
  builtin_residuals ();
  triangle_solve ();
  trilateration ();
  rank_and_weighting ();
  custom_and_domain ();
  empty_limits_and_validation ();
  allocation_failures ();
  linear_algebra_regressions ();
  analytic2 ();
  analytic_properties ();
  analytic3 ();
  return 0;
}

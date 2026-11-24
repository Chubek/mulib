/**
 * @file mulib-geosolve.h
 * @brief Extensible 2D/3D geometric constraint solving and analytic constructions.
 * @details The local, double-precision solver minimizes weighted residuals using
 * finite differences and damped, column-pivoted Householder QR. It reports rank,
 * remaining degrees of freedom and unsatisfied constraints; it does not prove
 * global feasibility or enumerate solution branches. Storage is caller-owned.
 * Define MULIB_IMPL_GEOSOLVE, MULIB_IMPL_GEOMETRY, MULIB_IMPL_CGEOM,
 * MULIB_IMPL_LINALG, MULIB_IMPL_SIMDMATH, MULIB_IMPL_ALLOC and MULIB_IMPL_PORT
 * in exactly one C11 translation unit. Link with libm. See the dedicated manual
 * for residual definitions, ownership, resource bounds and numerical limits.
 */
#ifndef MULIB_GEOSOLVE_H
#define MULIB_GEOSOLVE_H
#include "mulib-alloc.h"
#include "mulib-geometry.h"
#include "mulib-cgeom.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Maximum free variables, residual rows and total variables.
   * These bounds apply to each solve, including custom constraints. */
#define MULIB_GEOSOLVE_MAX_FREE_VARIABLES 256
#define MULIB_GEOSOLVE_MAX_RESIDUALS 4096
#define MULIB_GEOSOLVE_MAX_VARIABLES 65536

  /** @brief Constraint kinds. Coordinate indices are grouped by point, in
   * x,y[,z] order; segment constraints use endpoints a,b,c,d. */
  typedef enum mulib_geosolve_kind
  {
    MULIB_GEOSOLVE_VALUE,             /**< x - value. */
    MULIB_GEOSOLVE_EQUAL,             /**< x - y - value. */
    MULIB_GEOSOLVE_DISTANCE2,         /**< |b-a| - value. */
    MULIB_GEOSOLVE_DISTANCE3,
    MULIB_GEOSOLVE_COINCIDENT2,       /**< a - b, componentwise. */
    MULIB_GEOSOLVE_COINCIDENT3,
    MULIB_GEOSOLVE_MIDPOINT2,         /**< p - (a+b)/2. */
    MULIB_GEOSOLVE_MIDPOINT3,
    MULIB_GEOSOLVE_POINT_ON_LINE2,    /**< Signed distance of p to line a,b. */
    MULIB_GEOSOLVE_POINT_ON_PLANE3,   /**< Signed distance of p to plane a,b,c. */
    MULIB_GEOSOLVE_PARALLEL2,         /**< Cross of unit directions ab,cd. */
    MULIB_GEOSOLVE_PARALLEL3,
    MULIB_GEOSOLVE_PERPENDICULAR2,    /**< Dot of unit directions ab,cd. */
    MULIB_GEOSOLVE_PERPENDICULAR3,
    MULIB_GEOSOLVE_ANGLE2,            /**< Wrapped signed angle(ab,cd)-value. */
    MULIB_GEOSOLVE_ANGLE3,            /**< Unsigned angle(ab,cd)-value. */
    MULIB_GEOSOLVE_EQUAL_LENGTH2,     /**< |b-a| - |d-c| - value. */
    MULIB_GEOSOLVE_EQUAL_LENGTH3,
    MULIB_GEOSOLVE_POINT_ON_CIRCLE2,  /**< |p-center|-radius; indices p,c,r. */
    MULIB_GEOSOLVE_POINT_ON_SPHERE3,
    MULIB_GEOSOLVE_TANGENT_EXTERNAL2, /**< |c2-c1|-r1-r2; indices c1,r1,c2,r2. */
    MULIB_GEOSOLVE_TANGENT_INTERNAL2, /**< |c2-c1|-|r1-r2|. */
    MULIB_GEOSOLVE_TRIANGLE_AREA2,    /**< Signed area(a,b,c) - value. */
    MULIB_GEOSOLVE_CUSTOM
  } mulib_geosolve_kind_t;

  /** @brief Fill every residual and return nonzero, or zero outside the
   * callback's domain. Values and user data are borrowed; callbacks must be
   * deterministic and must not modify problem storage during a solve. */
  typedef int (*mulib_geosolve_residual_fn) (const double *values,
                                             size_t variable_count,
                                             double *residuals,
                                             size_t residual_count,
                                             void *user);
  typedef struct mulib_geosolve_constraint
  {
    mulib_geosolve_kind_t kind;
    size_t indices[12];
    double value;                /**< Target distance, angle, area or offset. */
    double weight;               /**< Positive finite least-squares weight. */
    size_t residual_count;       /**< CUSTOM only; built-ins infer their size. */
    mulib_geosolve_residual_fn residual; /**< CUSTOM only. */
    void *user;                  /**< Borrowed callback context. */
  } mulib_geosolve_constraint_t;

  /** @brief Borrowed arrays, never retained. Nonzero fixed[i] locks values[i].
   * NULL fixed means all variables are free. NULL arrays require zero counts.
   * The arrays, result, options and callback context must not overlap. */
  typedef struct mulib_geosolve_problem
  {
    double *values;
    size_t variable_count;
    const unsigned char *fixed;
    const mulib_geosolve_constraint_t *constraints;
    size_t constraint_count;
  } mulib_geosolve_problem_t;

  typedef enum mulib_geosolve_status
  {
    MULIB_GEOSOLVE_SOLVED = 0,          /**< Every weighted residual meets tolerance. */
    MULIB_GEOSOLVE_STALLED = 1,         /**< Stationary, tiny step or no improving step. */
    MULIB_GEOSOLVE_ITERATION_LIMIT = 2,
    MULIB_GEOSOLVE_INVALID = -1,        /**< Malformed descriptors or options. */
    MULIB_GEOSOLVE_NO_MEMORY = -2,
    MULIB_GEOSOLVE_EVALUATION_ERROR = -3, /**< Invalid initial/derivative domain. */
    MULIB_GEOSOLVE_NUMERIC_ERROR = -4   /**< Unrepresentable arithmetic. */
  } mulib_geosolve_status_t;

  typedef struct mulib_geosolve_options
  {
    size_t max_iterations;          /**< Trial steps, including rejected steps. */
    double residual_tolerance;      /**< Maximum absolute weighted residual. */
    double step_tolerance;          /**< Maximum |step_i|/max(1,|x_i|). */
    double gradient_tolerance;      /**< Infinity norm of column-scaled J' r. */
    double difference_step;         /**< Relative finite-difference step, (0,0.1]. */
    double initial_damping;
    double rank_tolerance;          /**< Relative QR pivot threshold, (0,1). */
    const mulib_allocator_t *allocator; /**< Borrowed; NULL uses libc. */
  } mulib_geosolve_options_t;

  typedef struct mulib_geosolve_result
  {
    mulib_geosolve_status_t status;
    size_t iterations, evaluations;
    size_t residual_count, free_variables, rank, degrees_of_freedom;
    size_t worst_constraint;        /**< SIZE_MAX when there are no residuals. */
    double cost;                    /**< Half the sum of squared weighted residuals. */
    double max_residual;
  } mulib_geosolve_result_t;

  /** @brief Default tolerances, 100 trial steps and libc allocation. */
  mulib_geosolve_options_t mulib_geosolve_default_options (void);
  /** @brief Construct a zero-initialized built-in descriptor with weight 1.
   * Set the required indices and target value before solving. */
  mulib_geosolve_constraint_t mulib_geosolve_constraint (mulib_geosolve_kind_t kind);
  /** @brief Solve locally from values. SOLVED/STALLED/ITERATION_LIMIT publish
   * the best accepted iterate; negative statuses preserve all values. Optional
   * result is always initialized, but rank is meaningful only for nonnegative
   * statuses. NULL options uses defaults. max_iterations=0 evaluates/ranks only.
   * Fixed variables are preserved bit-for-bit. No allocations survive the call. */
  mulib_geosolve_status_t mulib_geosolve_solve (
      const mulib_geosolve_problem_t *problem,
      const mulib_geosolve_options_t *options, mulib_geosolve_result_t *result);
  /** @brief Static diagnostic text; never NULL, even for unknown statuses. */
  const char *mulib_geosolve_status_string (mulib_geosolve_status_t status);

  typedef struct mulib_geosolve_line2
  {
    mulib_geometry_point_t origin, direction;
  } mulib_geosolve_line2_t;
  typedef struct mulib_geosolve_circle2
  {
    mulib_geometry_point_t center;
    double radius;
  } mulib_geosolve_circle2_t;
  /** @brief Plane dot(normal,point)=offset; normal need not be unit length. */
  typedef struct mulib_geosolve_plane3
  {
    mulib_linalg_vec3_t normal;
    double offset;
  } mulib_geosolve_plane3_t;
  /** @brief Intersection classification; TWO for segment overlap means the two
   * endpoints of the overlap, not two isolated crossings. */
  typedef enum mulib_geosolve_intersection
  {
    MULIB_GEOSOLVE_INTERSECTION_ERROR = -1,
    MULIB_GEOSOLVE_INTERSECTION_NONE = 0,
    MULIB_GEOSOLVE_INTERSECTION_ONE = 1,
    MULIB_GEOSOLVE_INTERSECTION_TWO = 2,
    MULIB_GEOSOLVE_INTERSECTION_INFINITE = 3
  } mulib_geosolve_intersection_t;
  /** @brief Intersect infinite lines. Nonzero finite directions required.
   * Output is required, and unchanged except for a unique intersection. */
  mulib_geosolve_intersection_t mulib_geosolve_line_line2 (
      mulib_geosolve_line2_t a, mulib_geosolve_line2_t b,
      mulib_geometry_point_t *out);
  /** @brief Closed segment intersection, including point segments. out must
   * have room for two points; only ONE/TWO writes it. Overlap follows a->b. */
  mulib_geosolve_intersection_t mulib_geosolve_segment_segment2 (
      mulib_geometry_point_t a, mulib_geometry_point_t b,
      mulib_geometry_point_t c, mulib_geometry_point_t d,
      mulib_geometry_point_t out[2]);
  /** @brief Intersect a line and nonnegative-radius circle. Two results follow
   * the line direction. Required out[2] is unchanged for NONE/ERROR. */
  mulib_geosolve_intersection_t mulib_geosolve_line_circle2 (
      mulib_geosolve_line2_t line, mulib_geosolve_circle2_t circle,
      mulib_geometry_point_t out[2]);
  /** @brief Circle intersections. Equal positive circles give INFINITE;
   * coincident zero-radius circles give ONE. Required out[2] is unchanged for
   * NONE/INFINITE/ERROR. */
  mulib_geosolve_intersection_t mulib_geosolve_circle_circle2 (
      mulib_geosolve_circle2_t a, mulib_geosolve_circle2_t b,
      mulib_geometry_point_t out[2]);
  /** @brief Circumcircle through three noncollinear finite points; 1/0 success,
   * preserving required output on failure. */
  int mulib_geosolve_circumcircle2 (mulib_geometry_point_t a,
                                    mulib_geometry_point_t b,
                                    mulib_geometry_point_t c,
                                    mulib_geosolve_circle2_t *out);
  /** @brief Plane through a nondegenerate MuLIB triangle; 1/0 success,
   * preserving required output on failure. */
  int mulib_geosolve_plane_from_triangle3 (mulib_cgeom_triangle_t triangle,
                                           mulib_geosolve_plane3_t *out);
  /** @brief Infinite line/plane intersection using a MuLIB ray descriptor;
   * negative ray parameters are allowed. Required out changes only for ONE. */
  mulib_geosolve_intersection_t mulib_geosolve_line_plane3 (
      mulib_cgeom_ray_t line, mulib_geosolve_plane3_t plane,
      mulib_linalg_vec3_t *out);
  /** @brief Intersection line of two planes. Required out changes only for
   * ONE (one line); coincident planes give INFINITE. */
  mulib_geosolve_intersection_t mulib_geosolve_plane_plane3 (
      mulib_geosolve_plane3_t a, mulib_geosolve_plane3_t b,
      mulib_cgeom_ray_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GEOSOLVE_H */

#if defined(MULIB_IMPL_GEOSOLVE) && !defined(MULIB_GEOSOLVE_IMPLEMENTED)
#define MULIB_GEOSOLVE_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_geosolve_options_t
  mulib_geosolve_default_options (void)
  {
    mulib_geosolve_options_t o;
    o.max_iterations = 100;
    o.residual_tolerance = 1e-8;
    o.step_tolerance = 1e-12;
    o.gradient_tolerance = 1e-12;
    o.difference_step = 6e-6;
    o.initial_damping = 1e-3;
    o.rank_tolerance = 1e-9;
    o.allocator = NULL;
    return o;
  }
  mulib_geosolve_constraint_t
  mulib_geosolve_constraint (mulib_geosolve_kind_t kind)
  {
    mulib_geosolve_constraint_t c;
    memset (&c, 0, sizeof c);
    c.kind = kind;
    c.weight = 1;
    return c;
  }
  const char *
  mulib_geosolve_status_string (mulib_geosolve_status_t s)
  {
    switch (s)
      {
      case MULIB_GEOSOLVE_SOLVED: return "solved";
      case MULIB_GEOSOLVE_STALLED: return "stalled with unsatisfied residuals";
      case MULIB_GEOSOLVE_ITERATION_LIMIT: return "iteration limit";
      case MULIB_GEOSOLVE_INVALID: return "invalid problem or options";
      case MULIB_GEOSOLVE_NO_MEMORY: return "allocation failed";
      case MULIB_GEOSOLVE_EVALUATION_ERROR: return "residual or derivative outside domain";
      case MULIB_GEOSOLVE_NUMERIC_ERROR: return "nonfinite numerical result";
      default: return "unknown solver status";
      }
  }
  static int
  mulib_geosolve_shape_ (const mulib_geosolve_constraint_t *c,
                        size_t *variables, size_t *rows)
  {
    size_t n = 0, m = 1;
    switch (c->kind)
      {
      case MULIB_GEOSOLVE_VALUE: n = 1; break;
      case MULIB_GEOSOLVE_EQUAL: n = 2; break;
      case MULIB_GEOSOLVE_DISTANCE2: n = 4; break;
      case MULIB_GEOSOLVE_DISTANCE3: n = 6; break;
      case MULIB_GEOSOLVE_COINCIDENT2: n = 4; m = 2; break;
      case MULIB_GEOSOLVE_COINCIDENT3: n = 6; m = 3; break;
      case MULIB_GEOSOLVE_MIDPOINT2: n = 6; m = 2; break;
      case MULIB_GEOSOLVE_MIDPOINT3: n = 9; m = 3; break;
      case MULIB_GEOSOLVE_POINT_ON_LINE2: n = 6; break;
      case MULIB_GEOSOLVE_POINT_ON_PLANE3: n = 12; break;
      case MULIB_GEOSOLVE_PARALLEL2:
      case MULIB_GEOSOLVE_PERPENDICULAR2:
      case MULIB_GEOSOLVE_ANGLE2:
      case MULIB_GEOSOLVE_EQUAL_LENGTH2: n = 8; break;
      case MULIB_GEOSOLVE_PARALLEL3: n = 12; m = 3; break;
      case MULIB_GEOSOLVE_PERPENDICULAR3:
      case MULIB_GEOSOLVE_ANGLE3:
      case MULIB_GEOSOLVE_EQUAL_LENGTH3: n = 12; break;
      case MULIB_GEOSOLVE_POINT_ON_CIRCLE2: n = 5; break;
      case MULIB_GEOSOLVE_POINT_ON_SPHERE3: n = 7; break;
      case MULIB_GEOSOLVE_TANGENT_EXTERNAL2:
      case MULIB_GEOSOLVE_TANGENT_INTERNAL2: n = 6; break;
      case MULIB_GEOSOLVE_TRIANGLE_AREA2: n = 6; break;
      case MULIB_GEOSOLVE_CUSTOM:
        m = c->residual_count;
        if (!c->residual || !m)
          return 0;
        break;
      default: return 0;
      }
    *variables = n;
    *rows = m;
    return 1;
  }
  static int
  mulib_geosolve_validate_ (const mulib_geosolve_problem_t *p,
                            size_t *rows, size_t *free_count)
  {
    size_t m = 0, k = 0;
    const double pi = 3.14159265358979323846;
    if (!p || (!p->values && p->variable_count)
        || (!p->constraints && p->constraint_count)
        || p->variable_count > MULIB_GEOSOLVE_MAX_VARIABLES
        || p->constraint_count > MULIB_GEOSOLVE_MAX_RESIDUALS)
      return 0;
    for (size_t i = 0; i < p->variable_count; ++i)
      {
        if (!isfinite (p->values[i]))
          return 0;
        k += !p->fixed || !p->fixed[i];
      }
    if (k > MULIB_GEOSOLVE_MAX_FREE_VARIABLES)
      return 0;
    for (size_t i = 0; i < p->constraint_count; ++i)
      {
        const mulib_geosolve_constraint_t *c = &p->constraints[i];
        size_t n, r;
        if (!mulib_geosolve_shape_ (c, &n, &r)
            || r > MULIB_GEOSOLVE_MAX_RESIDUALS - m
            || !isfinite (c->weight) || c->weight <= 0
            || !isfinite (c->value))
          return 0;
        for (size_t j = 0; j < n; ++j)
          if (c->indices[j] >= p->variable_count)
            return 0;
        if (((c->kind == MULIB_GEOSOLVE_DISTANCE2
              || c->kind == MULIB_GEOSOLVE_DISTANCE3) && c->value < 0)
            || (c->kind == MULIB_GEOSOLVE_ANGLE2
                && (c->value < -pi || c->value > pi))
            || (c->kind == MULIB_GEOSOLVE_ANGLE3
                && (c->value < 0 || c->value > pi)))
          return 0;
        m += r;
      }
    *rows = m;
    *free_count = k;
    return 1;
  }
  static double
  mulib_geosolve_length_ (const double *a, const double *b, size_t dim)
  {
    if (dim == 2)
      {
        mulib_geometry_point_t p = { a[0], a[1] }, q = { b[0], b[1] };
        return mulib_geometry_distance (p, q);
      }
    return hypot (hypot (b[0] - a[0], b[1] - a[1]), b[2] - a[2]);
  }
  static void
  mulib_geosolve_cross3_ (const double a[3], const double b[3], double out[3])
  {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
  }
  static int
  mulib_geosolve_unit_ (double a[3], size_t dim)
  {
    double n = hypot (a[0], a[1]);
    if (dim == 3)
      n = hypot (n, a[2]);
    if (!(n > 0) || !isfinite (n))
      return 0;
    for (size_t j = 0; j < dim; ++j)
      a[j] /= n;
    return 1;
  }
  static int
  mulib_geosolve_builtin_ (const mulib_geosolve_constraint_t *c,
                           const double *x, double *r)
  {
    double v[12], u[3] = { 0, 0, 0 }, w[3] = { 0, 0, 0 }, cross[3];
    size_t n = 0, rows = 0, dim;
    (void)mulib_geosolve_shape_ (c, &n, &rows);
    for (size_t j = 0; j < n; ++j)
      v[j] = x[c->indices[j]];
    switch (c->kind)
      {
      case MULIB_GEOSOLVE_VALUE: r[0] = v[0] - c->value; break;
      case MULIB_GEOSOLVE_EQUAL: r[0] = v[0] - v[1] - c->value; break;
      case MULIB_GEOSOLVE_DISTANCE2:
      case MULIB_GEOSOLVE_DISTANCE3:
        dim = n / 2;
        r[0] = mulib_geosolve_length_ (v, v + dim, dim) - c->value;
        break;
      case MULIB_GEOSOLVE_COINCIDENT2:
      case MULIB_GEOSOLVE_COINCIDENT3:
        for (size_t j = 0; j < rows; ++j)
          r[j] = v[j] - v[rows + j];
        break;
      case MULIB_GEOSOLVE_MIDPOINT2:
      case MULIB_GEOSOLVE_MIDPOINT3:
        for (size_t j = 0; j < rows; ++j)
          r[j] = v[j] - (0.5 * v[rows + j] + 0.5 * v[2 * rows + j]);
        break;
      case MULIB_GEOSOLVE_POINT_ON_LINE2:
        u[0] = v[4] - v[2]; u[1] = v[5] - v[3];
        if (!mulib_geosolve_unit_ (u, 2))
          return 0;
        r[0] = u[0] * (v[1] - v[3]) - u[1] * (v[0] - v[2]);
        break;
      case MULIB_GEOSOLVE_POINT_ON_PLANE3:
        for (size_t j = 0; j < 3; ++j)
          { u[j] = v[6 + j] - v[3 + j]; w[j] = v[9 + j] - v[3 + j]; }
        if (!mulib_geosolve_unit_ (u, 3) || !mulib_geosolve_unit_ (w, 3))
          return 0;
        mulib_geosolve_cross3_ (u, w, cross);
        if (!mulib_geosolve_unit_ (cross, 3))
          return 0;
        r[0] = 0;
        for (size_t j = 0; j < 3; ++j)
          r[0] += (v[j] - v[3 + j]) * cross[j];
        break;
      case MULIB_GEOSOLVE_POINT_ON_CIRCLE2:
      case MULIB_GEOSOLVE_POINT_ON_SPHERE3:
        dim = (n - 1) / 2;
        if (v[n - 1] < 0)
          return 0;
        r[0] = mulib_geosolve_length_ (v, v + dim, dim) - v[n - 1];
        break;
      case MULIB_GEOSOLVE_TANGENT_EXTERNAL2:
      case MULIB_GEOSOLVE_TANGENT_INTERNAL2:
        if (v[2] < 0 || v[5] < 0)
          return 0;
        r[0] = mulib_geosolve_length_ (v, v + 3, 2)
               - (c->kind == MULIB_GEOSOLVE_TANGENT_EXTERNAL2
                      ? v[2] + v[5] : fabs (v[2] - v[5]));
        break;
      case MULIB_GEOSOLVE_TRIANGLE_AREA2:
        {
          mulib_geometry_point_t a = { v[0], v[1] }, b = { v[2], v[3] },
                                 d = { v[4], v[5] };
          r[0] = 0.5 * mulib_geometry_cross (a, b, d) - c->value;
        }
        break;
      default:
        dim = n / 4;
        if (c->kind == MULIB_GEOSOLVE_EQUAL_LENGTH2
            || c->kind == MULIB_GEOSOLVE_EQUAL_LENGTH3)
          {
            r[0] = mulib_geosolve_length_ (v, v + dim, dim)
                   - mulib_geosolve_length_ (v + 2 * dim, v + 3 * dim, dim)
                   - c->value;
            break;
          }
        for (size_t j = 0; j < dim; ++j)
          { u[j] = v[dim + j] - v[j]; w[j] = v[3 * dim + j] - v[2 * dim + j]; }
        if (!mulib_geosolve_unit_ (u, dim) || !mulib_geosolve_unit_ (w, dim))
          return 0;
        mulib_geosolve_cross3_ (u, w, cross);
        if (c->kind == MULIB_GEOSOLVE_PARALLEL2)
          r[0] = cross[2];
        else if (c->kind == MULIB_GEOSOLVE_PARALLEL3)
          memcpy (r, cross, 3 * sizeof (double));
        else
          {
            double dot = u[0] * w[0] + u[1] * w[1] + u[2] * w[2];
            if (c->kind == MULIB_GEOSOLVE_ANGLE2)
              {
                double angle = atan2 (cross[2], dot) - c->value;
                r[0] = atan2 (sin (angle), cos (angle));
              }
            else if (c->kind == MULIB_GEOSOLVE_ANGLE3)
              r[0] = atan2 (hypot (hypot (cross[0], cross[1]), cross[2]), dot)
                     - c->value;
            else
              r[0] = dot;
          }
        break;
      }
    return 1;
  }
  static int
  mulib_geosolve_evaluate_ (const mulib_geosolve_problem_t *p, const double *x,
                            double *r, mulib_geosolve_result_t *report)
  {
    size_t offset = 0, worst = SIZE_MAX;
    double norm = 0, maximum = 0;
    if (report->evaluations != SIZE_MAX)
      ++report->evaluations;
    for (size_t i = 0; i < p->constraint_count; ++i)
      {
        const mulib_geosolve_constraint_t *c = &p->constraints[i];
        size_t n = 0, rows = 0;
        double weight = sqrt (c->weight);
        (void)mulib_geosolve_shape_ (c, &n, &rows);
        for (size_t j = 0; j < rows; ++j)
          r[offset + j] = NAN;
        if (c->kind == MULIB_GEOSOLVE_CUSTOM)
          {
            if (!c->residual (x, p->variable_count, r + offset, rows, c->user))
              return 0;
          }
        else if (!mulib_geosolve_builtin_ (c, x, r + offset))
          return 0;
        for (size_t j = 0; j < rows; ++j)
          {
            double v = r[offset + j] * weight;
            if (!isfinite (v))
              return 0;
            r[offset + j] = v;
            norm = hypot (norm, v);
            if (worst == SIZE_MAX || fabs (v) > maximum)
              { maximum = fabs (v); worst = i; }
          }
        offset += rows;
      }
    report->cost = norm * (norm * 0.5);
    report->max_residual = maximum;
    report->worst_constraint = worst;
    return isfinite (report->cost);
  }
  /* Column-pivoted Householder QR, in place. b doubles as backsolve storage.
   * Columns are already normalized by the caller. No normal equations. */
  static int
  mulib_geosolve_qr_ (double *a, double *b, size_t m, size_t n,
                      double tolerance, size_t *perm, double *solution,
                      size_t *rank)
  {
    double first = 0;
    *rank = 0;
    for (size_t j = 0; j < n; ++j)
      { perm[j] = j; solution[j] = 0; }
    for (size_t k = 0; k < n && k < m; ++k)
      {
        size_t pivot = k;
        double best = 0, beta, v0, tau;
        for (size_t j = k; j < n; ++j)
          {
            double norm = 0;
            for (size_t i = k; i < m; ++i)
              norm = hypot (norm, a[i * n + j]);
            if (norm > best)
              { best = norm; pivot = j; }
          }
        if (!isfinite (best))
          return 0;
        if (!k)
          first = best;
        if (best == 0 || best <= tolerance * first)
          break;
        if (pivot != k)
          {
            size_t t = perm[k]; perm[k] = perm[pivot]; perm[pivot] = t;
            for (size_t i = 0; i < m; ++i)
              {
                double d = a[i * n + k];
                a[i * n + k] = a[i * n + pivot]; a[i * n + pivot] = d;
              }
          }
        beta = -copysign (best, a[k * n + k]);
        v0 = a[k * n + k] - beta;
        tau = (beta - a[k * n + k]) / beta;
        a[k * n + k] = beta;
        for (size_t i = k + 1; i < m; ++i)
          a[i * n + k] /= v0;
        for (size_t j = k + 1; j <= n; ++j)
          {
            double dot = j == n ? b[k] : a[k * n + j];
            for (size_t i = k + 1; i < m; ++i)
              dot += a[i * n + k] * (j == n ? b[i] : a[i * n + j]);
            dot *= tau;
            if (j == n)
              {
                b[k] -= dot;
                for (size_t i = k + 1; i < m; ++i)
                  b[i] -= a[i * n + k] * dot;
              }
            else
              {
                a[k * n + j] -= dot;
                for (size_t i = k + 1; i < m; ++i)
                  a[i * n + j] -= a[i * n + k] * dot;
              }
          }
        ++*rank;
      }
    for (size_t i = *rank; i > 0; --i)
      {
        size_t k = i - 1;
        double v = b[k];
        for (size_t j = k + 1; j < *rank; ++j)
          v -= a[k * n + j] * b[j];
        b[k] = v / a[k * n + k];
        if (!isfinite (b[k]))
          return 0;
        solution[perm[k]] = b[k];
      }
    return 1;
  }
  static int
  mulib_geosolve_jacobian_ (const mulib_geosolve_problem_t *p,
                            const mulib_geosolve_options_t *o,
                            double *x, const double *r, size_t m, size_t k,
                            const size_t *free_ids, double *plus, double *minus,
                            double *jac, mulib_geosolve_result_t *report)
  {
    for (size_t j = 0; j < k; ++j)
      {
        size_t id = free_ids[j];
        double saved = x[id], h = o->difference_step * fmax (1, fabs (saved));
        double xp = saved + h, xm = saved - h, hp, hm;
        int valid_plus = 0, valid_minus = 0;
        mulib_geosolve_result_t scratch = *report;
        if (xp == saved)
          xp = nextafter (saved, INFINITY);
        if (xm == saved)
          xm = nextafter (saved, -INFINITY);
        hp = xp - saved; hm = saved - xm;
        if (isfinite (xp))
          {
            x[id] = xp;
            valid_plus = mulib_geosolve_evaluate_ (p, x, plus, &scratch);
          }
        if (isfinite (xm))
          {
            x[id] = xm;
            valid_minus = mulib_geosolve_evaluate_ (p, x, minus, &scratch);
          }
        x[id] = saved;
        report->evaluations = scratch.evaluations;
        if (!valid_plus && !valid_minus)
          return 0;
        for (size_t i = 0; i < m; ++i)
          {
            double v;
            if (valid_plus && valid_minus)
              v = ((plus[i] - r[i]) / hp) * (hm / (hp + hm))
                  + ((r[i] - minus[i]) / hm) * (hp / (hp + hm));
            else if (valid_plus)
              v = (plus[i] - r[i]) / hp;
            else
              v = (r[i] - minus[i]) / hm;
            if (!isfinite (v))
              return 0;
            jac[i * k + j] = v;
          }
      }
    return 1;
  }
  mulib_geosolve_status_t
  mulib_geosolve_solve (const mulib_geosolve_problem_t *p,
                        const mulib_geosolve_options_t *options,
                        mulib_geosolve_result_t *result)
  {
    mulib_geosolve_options_t o = options ? *options : mulib_geosolve_default_options ();
    mulib_geosolve_result_t report;
    double *memory = NULL, *x, *trial, *r, *trial_r, *plus, *minus, *jac;
    double *a, *b, *scales, *step, lambda = o.initial_damping;
    size_t *ids = NULL, *free_ids, *perm, m = 0, k = 0, n, doubles, terms, bytes;
    memset (&report, 0, sizeof report);
    report.status = MULIB_GEOSOLVE_INVALID;
    report.worst_constraint = SIZE_MAX;
    if (!isfinite (o.residual_tolerance) || o.residual_tolerance <= 0
        || !isfinite (o.step_tolerance) || o.step_tolerance <= 0
        || !isfinite (o.gradient_tolerance) || o.gradient_tolerance <= 0
        || !isfinite (o.difference_step) || o.difference_step <= 0
        || o.difference_step > 0.1 || !isfinite (lambda) || lambda <= 0
        || lambda > 1e15 || !isfinite (o.rank_tolerance)
        || o.rank_tolerance <= 0 || o.rank_tolerance >= 1
        || !mulib_alloc_valid (o.allocator)
        || !mulib_geosolve_validate_ (p, &m, &k))
      goto finish;
    n = p->variable_count;
    report.residual_count = m; report.free_variables = k;
    report.degrees_of_freedom = k;
    if (!m)
      { report.status = MULIB_GEOSOLVE_SOLVED; goto finish; }
    /* All terms are bounded above, but retain checked arithmetic for platforms
     * with small size_t and for future changes to the resource limits. */
    if (!mulib_port_mul_size (m + k, k, &terms)
        || !mulib_port_mul_size (m, k, &doubles)
        || !mulib_port_add_size (doubles, terms, &doubles)
        || !mulib_port_mul_size (n, 2, &terms)
        || !mulib_port_add_size (doubles, terms, &doubles)
        || !mulib_port_mul_size (m, 5, &terms)
        || !mulib_port_add_size (doubles, terms, &doubles)
        || !mulib_port_mul_size (k, 3, &terms)
        || !mulib_port_add_size (doubles, terms, &doubles)
        || !mulib_port_mul_size (doubles, sizeof (double), &bytes))
      goto finish;
    memory = (double *)mulib_alloc (o.allocator, bytes);
    if (!memory)
      { report.status = MULIB_GEOSOLVE_NO_MEMORY; goto finish; }
    if (k)
      {
        if (!mulib_port_mul_size (k, 2 * sizeof (size_t), &bytes))
          goto finish;
        ids = (size_t *)mulib_alloc (o.allocator, bytes);
        if (!ids)
          { report.status = MULIB_GEOSOLVE_NO_MEMORY; goto finish; }
      }
    free_ids = ids; perm = k ? ids + k : NULL;
    x = memory; trial = x + n; r = trial + n; trial_r = r + m;
    plus = trial_r + m; minus = plus + m; jac = minus + m;
    a = jac + m * k; b = a + (m + k) * k;
    scales = b + m + k; step = scales + k;
    if (n)
      memcpy (x, p->values, n * sizeof (double));
    for (size_t i = 0, j = 0; i < n; ++i)
      if (!p->fixed || !p->fixed[i])
        free_ids[j++] = i;
    if (!mulib_geosolve_evaluate_ (p, x, r, &report))
      { report.status = MULIB_GEOSOLVE_EVALUATION_ERROR; goto finish; }
    for (;;)
      {
        double gradient = 0;
        int accepted = 0;
        if (!mulib_geosolve_jacobian_ (p, &o, x, r, m, k, free_ids,
                                      plus, minus, jac, &report))
          { report.status = MULIB_GEOSOLVE_EVALUATION_ERROR; goto finish; }
        for (size_t j = 0; j < k; ++j)
          {
            double norm = 0, g = 0;
            for (size_t i = 0; i < m; ++i)
              norm = hypot (norm, jac[i * k + j]);
            if (!isfinite (norm))
              { report.status = MULIB_GEOSOLVE_NUMERIC_ERROR; goto finish; }
            scales[j] = norm > 0 ? norm : 1;
            for (size_t i = 0; i < m; ++i)
              {
                double v = jac[i * k + j] / scales[j];
                a[i * k + j] = v;
                g += v * r[i];
              }
            if (!isfinite (g))
              { report.status = MULIB_GEOSOLVE_NUMERIC_ERROR; goto finish; }
            gradient = fmax (gradient, fabs (g));
          }
        memset (b, 0, m * sizeof (double));
        if (!mulib_geosolve_qr_ (a, b, m, k, o.rank_tolerance, perm, step,
                                 &report.rank))
          { report.status = MULIB_GEOSOLVE_NUMERIC_ERROR; goto finish; }
        report.degrees_of_freedom = k - report.rank;
        if (report.max_residual <= o.residual_tolerance)
          { report.status = MULIB_GEOSOLVE_SOLVED; break; }
        if (!k || gradient <= o.gradient_tolerance)
          { report.status = MULIB_GEOSOLVE_STALLED; break; }
        if (report.iterations >= o.max_iterations)
          { report.status = MULIB_GEOSOLVE_ITERATION_LIMIT; break; }
        for (size_t attempt = 0; attempt < 24
                                 && report.iterations < o.max_iterations; ++attempt)
          {
            size_t rank;
            double relative_step = 0;
            mulib_geosolve_result_t candidate = report;
            for (size_t i = 0; i < m + k; ++i)
              {
                b[i] = i < m ? -r[i] : 0;
                for (size_t j = 0; j < k; ++j)
                  a[i * k + j] = i < m ? jac[i * k + j] / scales[j]
                                       : (i - m == j ? sqrt (lambda) : 0);
              }
            if (!mulib_geosolve_qr_ (a, b, m + k, k, DBL_EPSILON, perm, step, &rank)
                || rank != k)
              { report.status = MULIB_GEOSOLVE_NUMERIC_ERROR; goto finish; }
            if (n)
              memcpy (trial, x, n * sizeof (double));
            for (size_t j = 0; j < k; ++j)
              {
                double delta = step[j] / scales[j];
                size_t id = free_ids[j];
                trial[id] += delta;
                relative_step = fmax (relative_step, fabs (delta) / fmax (1, fabs (x[id])));
              }
            ++report.iterations;
            candidate.iterations = report.iterations;
            if (relative_step <= o.step_tolerance)
              break;
            {
              int finite = 1;
              for (size_t j = 0; j < k; ++j)
                if (!isfinite (trial[free_ids[j]]))
                  finite = 0;
              if (finite && mulib_geosolve_evaluate_ (p, trial, trial_r, &candidate)
                  && candidate.cost < report.cost)
                {
                  if (n)
                    memcpy (x, trial, n * sizeof (double));
                  memcpy (r, trial_r, m * sizeof (double));
                  report = candidate;
                  lambda = fmax (1e-15, lambda / 3);
                  accepted = 1;
                  break;
                }
            }
            report.evaluations = candidate.evaluations;
            if (lambda >= 1e15)
              break;
            lambda = fmin (1e15, lambda * 10);
          }
        if (!accepted)
          {
            report.status = report.iterations >= o.max_iterations
                                ? MULIB_GEOSOLVE_ITERATION_LIMIT : MULIB_GEOSOLVE_STALLED;
            break;
          }
      }
    /* Commit only after the complete solve has avoided hard errors. */
    for (size_t j = 0; j < k; ++j)
      p->values[free_ids[j]] = x[free_ids[j]];
  finish:
    mulib_alloc_free (o.allocator, ids);
    mulib_alloc_free (o.allocator, memory);
    if (result)
      *result = report;
    return report.status;
  }

  static int
  mulib_geosolve_point2_valid_ (mulib_geometry_point_t p)
  {
    return isfinite (p.x) && isfinite (p.y);
  }
  static int
  mulib_geosolve_point2_ (long double x, long double y, mulib_geometry_point_t *out)
  {
    if (!isfinite (x) || !isfinite (y) || fabsl (x) > DBL_MAX || fabsl (y) > DBL_MAX)
      return 0;
    out->x = (double)x; out->y = (double)y;
    return 1;
  }
  static int
  mulib_geosolve_line2_valid_ (mulib_geosolve_line2_t l)
  {
    return mulib_geosolve_point2_valid_ (l.origin)
           && mulib_geosolve_point2_valid_ (l.direction)
           && (l.direction.x != 0 || l.direction.y != 0);
  }
  static int
  mulib_geosolve_circle2_valid_ (mulib_geosolve_circle2_t c)
  {
    return mulib_geosolve_point2_valid_ (c.center) && isfinite (c.radius) && c.radius >= 0;
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_line_line2 (mulib_geosolve_line2_t a, mulib_geosolve_line2_t b,
                            mulib_geometry_point_t *out)
  {
    long double dx, dy, det, cross, t;
    mulib_geometry_point_t p;
    if (!out || !mulib_geosolve_line2_valid_ (a) || !mulib_geosolve_line2_valid_ (b))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    dx = (long double)b.origin.x - a.origin.x;
    dy = (long double)b.origin.y - a.origin.y;
    det = (long double)a.direction.x * b.direction.y - (long double)a.direction.y * b.direction.x;
    cross = dx * a.direction.y - dy * a.direction.x;
    if (!isfinite (det) || !isfinite (cross))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    if (det == 0)
      return cross == 0 ? MULIB_GEOSOLVE_INTERSECTION_INFINITE : MULIB_GEOSOLVE_INTERSECTION_NONE;
    t = (dx * b.direction.y - dy * b.direction.x) / det;
    if (!mulib_geosolve_point2_ (a.origin.x + t * a.direction.x,
                                 a.origin.y + t * a.direction.y, &p))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    *out = p;
    return MULIB_GEOSOLVE_INTERSECTION_ONE;
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_segment_segment2 (mulib_geometry_point_t a, mulib_geometry_point_t b,
                                  mulib_geometry_point_t c, mulib_geometry_point_t d,
                                  mulib_geometry_point_t out[2])
  {
    long double ux, uy, vx, vy, wx, wy, det, t, s;
    mulib_geometry_point_t p[2];
    if (!out || !mulib_geosolve_point2_valid_ (a) || !mulib_geosolve_point2_valid_ (b)
        || !mulib_geosolve_point2_valid_ (c) || !mulib_geosolve_point2_valid_ (d))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    ux = (long double)b.x - a.x; uy = (long double)b.y - a.y;
    vx = (long double)d.x - c.x; vy = (long double)d.y - c.y;
    wx = (long double)c.x - a.x; wy = (long double)c.y - a.y;
    if (ux == 0 && uy == 0)
      {
        long double side = ((long double)a.x - c.x) * vy - ((long double)a.y - c.y) * vx;
        if (!isfinite (side))
          return MULIB_GEOSOLVE_INTERSECTION_ERROR;
        if (side != 0 || a.x < fmin (c.x, d.x) || a.x > fmax (c.x, d.x)
            || a.y < fmin (c.y, d.y) || a.y > fmax (c.y, d.y))
          return MULIB_GEOSOLVE_INTERSECTION_NONE;
        out[0] = a;
        return MULIB_GEOSOLVE_INTERSECTION_ONE;
      }
    det = ux * vy - uy * vx;
    if (!isfinite (det))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    if (det != 0)
      {
        t = (wx * vy - wy * vx) / det;
        s = (wx * uy - wy * ux) / det;
        if (!isfinite (t) || !isfinite (s))
          return MULIB_GEOSOLVE_INTERSECTION_ERROR;
        if (t < 0 || t > 1 || s < 0 || s > 1)
          return MULIB_GEOSOLVE_INTERSECTION_NONE;
        if (!mulib_geosolve_point2_ (a.x + t * ux, a.y + t * uy, &p[0]))
          return MULIB_GEOSOLVE_INTERSECTION_ERROR;
        out[0] = p[0];
        return MULIB_GEOSOLVE_INTERSECTION_ONE;
      }
    if (!isfinite (wx * uy - wy * ux))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    if (wx * uy - wy * ux != 0)
      return MULIB_GEOSOLVE_INTERSECTION_NONE;
    t = fabsl (ux) >= fabsl (uy) ? wx / ux : wy / uy;
    s = fabsl (ux) >= fabsl (uy) ? ((long double)d.x - a.x) / ux : ((long double)d.y - a.y) / uy;
    {
      long double lo = fmaxl (0, fminl (t, s)), hi = fminl (1, fmaxl (t, s));
      if (lo > hi)
        return MULIB_GEOSOLVE_INTERSECTION_NONE;
      if (!mulib_geosolve_point2_ (a.x + lo * ux, a.y + lo * uy, &p[0])
          || !mulib_geosolve_point2_ (a.x + hi * ux, a.y + hi * uy, &p[1]))
        return MULIB_GEOSOLVE_INTERSECTION_ERROR;
      out[0] = p[0];
      if (lo == hi)
        return MULIB_GEOSOLVE_INTERSECTION_ONE;
      out[1] = p[1];
      return MULIB_GEOSOLVE_INTERSECTION_TWO;
    }
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_line_circle2 (mulib_geosolve_line2_t l, mulib_geosolve_circle2_t c,
                              mulib_geometry_point_t out[2])
  {
    long double length, ux, uy, side, height, px, py;
    mulib_geometry_point_t p[2];
    if (!out || !mulib_geosolve_line2_valid_ (l) || !mulib_geosolve_circle2_valid_ (c))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    length = hypotl (l.direction.x, l.direction.y);
    if (!isfinite (length) || length == 0)
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    ux = l.direction.x / length; uy = l.direction.y / length;
    side = ((long double)c.center.y - l.origin.y) * ux
           - ((long double)c.center.x - l.origin.x) * uy;
    if (!isfinite (side))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    if (fabsl (side) > c.radius)
      return MULIB_GEOSOLVE_INTERSECTION_NONE;
    if (c.radius == 0)
      height = 0;
    else
      {
        long double ratio = fabsl (side) / c.radius;
        height = c.radius * sqrtl ((1 - ratio) * (1 + ratio));
      }
    px = c.center.x + side * uy; py = c.center.y - side * ux;
    if (!mulib_geosolve_point2_ (px - height * ux, py - height * uy, &p[0])
        || !mulib_geosolve_point2_ (px + height * ux, py + height * uy, &p[1]))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    out[0] = p[0];
    if (height == 0)
      return MULIB_GEOSOLVE_INTERSECTION_ONE;
    out[1] = p[1];
    return MULIB_GEOSOLVE_INTERSECTION_TWO;
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_circle_circle2 (mulib_geosolve_circle2_t a, mulib_geosolve_circle2_t b,
                                mulib_geometry_point_t out[2])
  {
    long double dx, dy, distance, along, height, ux, uy, px, py;
    long double scale, ra, rb, dd, sum, difference, h1, h2;
    mulib_geometry_point_t p[2];
    if (!out || !mulib_geosolve_circle2_valid_ (a) || !mulib_geosolve_circle2_valid_ (b))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    dx = (long double)b.center.x - a.center.x; dy = (long double)b.center.y - a.center.y;
    distance = hypotl (dx, dy);
    if (!isfinite (distance))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    if (distance == 0)
      {
        if (a.radius != b.radius)
          return MULIB_GEOSOLVE_INTERSECTION_NONE;
        if (a.radius != 0)
          return MULIB_GEOSOLVE_INTERSECTION_INFINITE;
        out[0] = a.center;
        return MULIB_GEOSOLVE_INTERSECTION_ONE;
      }
    if (distance > (long double)a.radius + b.radius
        || distance < fabsl ((long double)a.radius - b.radius))
      return MULIB_GEOSOLVE_INTERSECTION_NONE;
    scale = fmaxl (distance, fmaxl (a.radius, b.radius));
    ra = a.radius / scale; rb = b.radius / scale; dd = distance / scale;
    if (dd == 0)
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    sum = ra + rb; difference = ra - rb;
    along = ((dd + (difference / dd) * sum) / 2) * scale;
    /* Scaled Heron factors avoid r*r-along*along cancellation, fourth-power
     * overflow and the loss of a tiny distance when the radii are equal. */
    h1 = sqrtl (fmaxl (0, sum - dd)) * sqrtl (fmaxl (0, sum + dd));
    h2 = sqrtl (fmaxl (0, dd + difference)) * sqrtl (fmaxl (0, dd - difference));
    height = ((h1 * (h2 / dd)) / 2) * scale;
    if (distance == (long double)a.radius + b.radius
        || distance == fabsl ((long double)a.radius - b.radius))
      height = 0;
    ux = dx / distance; uy = dy / distance;
    px = a.center.x + along * ux; py = a.center.y + along * uy;
    if (!mulib_geosolve_point2_ (px - height * uy, py + height * ux, &p[0])
        || !mulib_geosolve_point2_ (px + height * uy, py - height * ux, &p[1]))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    out[0] = p[0];
    if (height == 0)
      return MULIB_GEOSOLVE_INTERSECTION_ONE;
    out[1] = p[1];
    return MULIB_GEOSOLVE_INTERSECTION_TWO;
  }
  int
  mulib_geosolve_circumcircle2 (mulib_geometry_point_t a, mulib_geometry_point_t b,
                               mulib_geometry_point_t c, mulib_geosolve_circle2_t *out)
  {
    long double bx, by, cx, cy, det, bb, cc, x, y, radius;
    mulib_geosolve_circle2_t circle;
    if (!out || !mulib_geosolve_point2_valid_ (a) || !mulib_geosolve_point2_valid_ (b)
        || !mulib_geosolve_point2_valid_ (c))
      return 0;
    bx = (long double)b.x - a.x; by = (long double)b.y - a.y;
    cx = (long double)c.x - a.x; cy = (long double)c.y - a.y;
    det = 2 * (bx * cy - by * cx);
    if (!isfinite (det) || det == 0)
      return 0;
    bb = bx * bx + by * by; cc = cx * cx + cy * cy;
    x = (bb * cy - cc * by) / det; y = (bx * cc - cx * bb) / det;
    radius = hypotl (x, y);
    if (!mulib_geosolve_point2_ (a.x + x, a.y + y, &circle.center)
        || !isfinite (radius) || radius > DBL_MAX)
      return 0;
    circle.radius = (double)radius;
    *out = circle;
    return 1;
  }
  static int
  mulib_geosolve_vec3_ (const double v[3], mulib_linalg_vec3_t *out)
  {
    for (size_t j = 0; j < 3; ++j)
      if (!isfinite (v[j]) || fabs (v[j]) > FLT_MAX)
        return 0;
    out->x = (float)v[0]; out->y = (float)v[1]; out->z = (float)v[2];
    return 1;
  }
  static int
  mulib_geosolve_plane_valid_ (mulib_geosolve_plane3_t p)
  {
    return mulib_cgeom_vec3_valid (p.normal) && isfinite (p.offset)
           && (p.normal.x != 0 || p.normal.y != 0 || p.normal.z != 0);
  }
  int
  mulib_geosolve_plane_from_triangle3 (mulib_cgeom_triangle_t t,
                                      mulib_geosolve_plane3_t *out)
  {
    double u[3] = { (double)t.b.x - t.a.x, (double)t.b.y - t.a.y, (double)t.b.z - t.a.z };
    double v[3] = { (double)t.c.x - t.a.x, (double)t.c.y - t.a.y, (double)t.c.z - t.a.z }, normal[3];
    mulib_geosolve_plane3_t p;
    if (!out || !mulib_cgeom_vec3_valid (t.a) || !mulib_cgeom_vec3_valid (t.b)
        || !mulib_cgeom_vec3_valid (t.c))
      return 0;
    mulib_geosolve_cross3_ (u, v, normal);
    if (!mulib_geosolve_unit_ (normal, 3) || !mulib_geosolve_vec3_ (normal, &p.normal))
      return 0;
    p.offset = (double)p.normal.x * t.a.x + (double)p.normal.y * t.a.y + (double)p.normal.z * t.a.z;
    *out = p;
    return 1;
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_line_plane3 (mulib_cgeom_ray_t l, mulib_geosolve_plane3_t p,
                             mulib_linalg_vec3_t *out)
  {
    double denominator, numerator, t, point[3];
    mulib_linalg_vec3_t v;
    if (!out || !mulib_cgeom_ray_valid (l) || !mulib_geosolve_plane_valid_ (p))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    denominator = (double)p.normal.x * l.direction.x + (double)p.normal.y * l.direction.y
                  + (double)p.normal.z * l.direction.z;
    numerator = p.offset - ((double)p.normal.x * l.origin.x + (double)p.normal.y * l.origin.y
                            + (double)p.normal.z * l.origin.z);
    if (denominator == 0)
      return numerator == 0 ? MULIB_GEOSOLVE_INTERSECTION_INFINITE : MULIB_GEOSOLVE_INTERSECTION_NONE;
    t = numerator / denominator;
    point[0] = l.origin.x + t * l.direction.x;
    point[1] = l.origin.y + t * l.direction.y;
    point[2] = l.origin.z + t * l.direction.z;
    if (!mulib_geosolve_vec3_ (point, &v))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    *out = v;
    return MULIB_GEOSOLVE_INTERSECTION_ONE;
  }
  mulib_geosolve_intersection_t
  mulib_geosolve_plane_plane3 (mulib_geosolve_plane3_t a, mulib_geosolve_plane3_t b,
                              mulib_cgeom_ray_t *out)
  {
    double u[3] = { a.normal.x, a.normal.y, a.normal.z };
    double v[3] = { b.normal.x, b.normal.y, b.normal.z }, cross[3], first[3], second[3], origin[3];
    double uu, vv, aa, bb, denominator;
    mulib_cgeom_ray_t line;
    if (!out || !mulib_geosolve_plane_valid_ (a) || !mulib_geosolve_plane_valid_ (b))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    /* Keep unnormalized normals for exact parallel/coincident classification. */
    mulib_geosolve_cross3_ (u, v, cross);
    denominator = cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
    if (denominator == 0)
      {
        size_t j = fabs (u[0]) >= fabs (u[1]) ? 0 : 1;
        if (fabs (u[2]) > fabs (u[j])) j = 2;
        return (long double)a.offset * v[j] == (long double)b.offset * u[j]
                   ? MULIB_GEOSOLVE_INTERSECTION_INFINITE : MULIB_GEOSOLVE_INTERSECTION_NONE;
      }
    uu = hypot (hypot (u[0], u[1]), u[2]);
    vv = hypot (hypot (v[0], v[1]), v[2]);
    aa = a.offset / uu; bb = b.offset / vv;
    for (size_t j = 0; j < 3; ++j)
      { u[j] /= uu; v[j] /= vv; }
    mulib_geosolve_cross3_ (u, v, cross);
    denominator = cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
    mulib_geosolve_cross3_ (v, cross, first);
    mulib_geosolve_cross3_ (cross, u, second);
    for (size_t j = 0; j < 3; ++j)
      origin[j] = (aa * first[j] + bb * second[j]) / denominator;
    if (!mulib_geosolve_unit_ (cross, 3) || !mulib_geosolve_vec3_ (origin, &line.origin)
        || !mulib_geosolve_vec3_ (cross, &line.direction))
      return MULIB_GEOSOLVE_INTERSECTION_ERROR;
    *out = line;
    return MULIB_GEOSOLVE_INTERSECTION_ONE;
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GEOSOLVE_IMPLEMENTED */

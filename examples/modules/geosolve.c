/* Solve a triangle and construct its circumcircle using MuLIB geometry types.
 * cc -std=c11 -Iinclude examples/modules/geosolve.c -lm -o geosolve */
#define MULIB_IMPL_GEOSOLVE
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_PORT
#include "mulib-geosolve.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int
main (void)
{
  /* A=(0,0), B=(3,0), C starts at (1,2). Anchor A and B to remove
   * the triangle's translation/rotation freedom. Coordinate indices may be
   * shared freely between constraints. */
  double coordinates[] = { 0, 0, 3, 0, 1, 2 };
  unsigned char fixed[] = { 1, 1, 1, 1, 0, 0 };
  mulib_geosolve_constraint_t constraints[2];
  mulib_geosolve_problem_t problem = { coordinates, 6, fixed, constraints, 2 };
  mulib_geosolve_result_t result;
  mulib_geosolve_circle2_t circle;
  mulib_geometry_point_t a, b, c;
  constraints[0] = mulib_geosolve_constraint (MULIB_GEOSOLVE_DISTANCE2);
  constraints[0].indices[0] = 0; constraints[0].indices[1] = 1;
  constraints[0].indices[2] = 4; constraints[0].indices[3] = 5;
  constraints[0].value = 4;
  constraints[1] = mulib_geosolve_constraint (MULIB_GEOSOLVE_DISTANCE2);
  constraints[1].indices[0] = 2; constraints[1].indices[1] = 3;
  constraints[1].indices[2] = 4; constraints[1].indices[3] = 5;
  constraints[1].value = 5;
  assert (mulib_geosolve_solve (&problem, NULL, &result) == MULIB_GEOSOLVE_SOLVED);
  assert (fabs (coordinates[4]) < 1e-7 && fabs (coordinates[5] - 4) < 1e-7);
  assert (result.rank == 2 && result.degrees_of_freedom == 0);
  a.x = coordinates[0]; a.y = coordinates[1];
  b.x = coordinates[2]; b.y = coordinates[3];
  c.x = coordinates[4]; c.y = coordinates[5];
  assert (mulib_geosolve_circumcircle2 (a, b, c, &circle));
  assert (fabs (circle.radius - 2.5) < 1e-7);
  printf ("C=(%.9g, %.9g), radius=%.9g, %s\n", c.x, c.y, circle.radius,
          mulib_geosolve_status_string (result.status));
  return 0;
}

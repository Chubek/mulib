/* MuLIB: field-mesh. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_MESH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_MESH_H
#define MULIB_FIELD_MESH_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t a,b,c;} mulib_field_triangle;
typedef struct {const mulib_geom2_point *points;size_t n;const mulib_field_triangle *triangles;size_t m;} mulib_field_mesh;
int mulib_field_mesh_valid(const mulib_field_mesh *m);
double mulib_field_mesh_area(const mulib_field_mesh *m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_MESH) && !defined(MULIB_FIELD_MESH_H_IMPLEMENTED)
#define MULIB_FIELD_MESH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_mesh_valid(const mulib_field_mesh *m){if(!m||(!m->points&&m->n)||(!m->triangles&&m->m))return 0;for(size_t i=0;i<m->n;i++)if(!mulib_geom2_point_valid(m->points[i]))return 0;for(size_t i=0;i<m->m;i++){mulib_field_triangle t=m->triangles[i];if(t.a>=m->n||t.b>=m->n||t.c>=m->n||fabs(mulib_geom2_orient(m->points[t.a],m->points[t.b],m->points[t.c]))<=DBL_MIN)return 0;}return 1;}
double mulib_field_mesh_area(const mulib_field_mesh *m){if(!mulib_field_mesh_valid(m))return NAN;double area=0;for(size_t i=0;i<m->m;i++){mulib_field_triangle t=m->triangles[i];area+=.5*fabs(mulib_geom2_orient(m->points[t.a],m->points[t.b],m->points[t.c]));}return area;}
#ifdef __cplusplus
}
#endif
#endif

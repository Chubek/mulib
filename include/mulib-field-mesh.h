/* MuLIB / Amb3r: field-mesh. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_MESH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_MESH_H
#define MULIB_FIELD_MESH_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t a,b,c;} ma_triangle;
typedef struct {const ma_point *points;size_t n;const ma_triangle *triangles;size_t m;} ma_field_mesh;
int mulib_field_mesh_valid(const ma_field_mesh *m);
double mulib_field_mesh_area(const ma_field_mesh *m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_MESH) && !defined(MULIB_FIELD_MESH_H_IMPLEMENTED)
#define MULIB_FIELD_MESH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_mesh_valid(const ma_field_mesh *m){if(!m||(!m->points&&m->n)||(!m->triangles&&m->m))return 0;for(size_t i=0;i<m->n;i++)if(!ma_point_valid(m->points[i]))return 0;for(size_t i=0;i<m->m;i++){ma_triangle t=m->triangles[i];if(t.a>=m->n||t.b>=m->n||t.c>=m->n||fabs(ma_cross(m->points[t.a],m->points[t.b],m->points[t.c]))<=DBL_MIN)return 0;}return 1;}
double mulib_field_mesh_area(const ma_field_mesh *m){if(!mulib_field_mesh_valid(m))return NAN;double area=0;for(size_t i=0;i<m->m;i++){ma_triangle t=m->triangles[i];area+=.5*fabs(ma_cross(m->points[t.a],m->points[t.b],m->points[t.c]));}return area;}
#ifdef __cplusplus
}
#endif
#endif

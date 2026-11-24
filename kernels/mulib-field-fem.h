/* MuLIB: field-fem. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_FEM in exactly one implementation translation unit.
 * Profile: 2-D linear triangular scalar diffusion finite-element assembly; dense matrix.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_FEM_H
#define MULIB_FIELD_FEM_H
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
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#include "mulib-field-mesh.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_fem_assemble(const mulib_field_mesh *mesh,const double *coefficient,const double *source,double *matrix,double *load);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_FEM) && !defined(MULIB_FIELD_FEM_H_IMPLEMENTED)
#define MULIB_FIELD_FEM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_fem_assemble(const mulib_field_mesh *mesh,const double *coefficient,const double *source,double *matrix,double *load){size_t nn;if(!mulib_field_mesh_valid(mesh)||!mulib_storage_size_mul(mesh->n,mesh->n,&nn)||!matrix||!load||(!coefficient&&mesh->m)||(!source&&mesh->m))return 0;for(size_t k=0;k<mesh->m;k++)if(coefficient[k]<=0||!isfinite(coefficient[k])||!isfinite(source[k]))return 0;memset(matrix,0,nn*sizeof(double));memset(load,0,mesh->n*sizeof(double));for(size_t k=0;k<mesh->m;k++){mulib_field_triangle t=mesh->triangles[k];size_t id[3]={t.a,t.b,t.c};mulib_geom2_point p[3]={mesh->points[t.a],mesh->points[t.b],mesh->points[t.c]};double area=.5*fabs(mulib_geom2_orient(p[0],p[1],p[2]));double bx[3]={p[1].y-p[2].y,p[2].y-p[0].y,p[0].y-p[1].y},by[3]={p[2].x-p[1].x,p[0].x-p[2].x,p[1].x-p[0].x};for(size_t i=0;i<3;i++){load[id[i]]+=source[k]*area/3;for(size_t j=0;j<3;j++)matrix[id[i]*mesh->n+id[j]]+=coefficient[k]*(bx[i]*bx[j]+by[i]*by[j])/(4*area);}}return mulib_num_all_finite(matrix,nn)&&mulib_num_all_finite(load,mesh->n);}
#ifdef __cplusplus
}
#endif
#endif

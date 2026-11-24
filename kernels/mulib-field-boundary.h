/* MuLIB: field-boundary. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_BOUNDARY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_BOUNDARY_H
#define MULIB_FIELD_BOUNDARY_H
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
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_boundary_dirichlet(size_t n,double *a,double *b,const unsigned char *fixed,const double *values);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_BOUNDARY) && !defined(MULIB_FIELD_BOUNDARY_H_IMPLEMENTED)
#define MULIB_FIELD_BOUNDARY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_boundary_dirichlet(size_t n,double *a,double *b,const unsigned char *fixed,const double *values){size_t nn;if(!mulib_storage_size_mul(n,n,&nn)||!a||!b||!fixed||!values)return 0;for(size_t i=0;i<n;i++)if(fixed[i]&&!isfinite(values[i]))return 0;for(size_t i=0;i<n;i++)if(fixed[i]){for(size_t j=0;j<n;j++)if(j!=i){b[j]-=a[j*n+i]*values[i];a[j*n+i]=a[i*n+j]=0;}a[i*n+i]=1;b[i]=values[i];}return 1;}
#ifdef __cplusplus
}
#endif
#endif

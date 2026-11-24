/* MuLIB: num-bicgstab. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_BICGSTAB in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_BICGSTAB_H
#define MULIB_NUM_BICGSTAB_H
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
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_bicgstab(const mulib_num_csr_view *a,const double *b,double *x,size_t max_iter,double tol,size_t *iterations);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_BICGSTAB) && !defined(MULIB_NUM_BICGSTAB_H_IMPLEMENTED)
#define MULIB_NUM_BICGSTAB_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_bicgstab(const mulib_num_csr_view *a,const double *b,double *x,size_t max_iter,double tol,size_t *iterations){if(!mulib_num_csr_view_valid(a)||a->rows!=a->cols||!b||!x||tol<=0||!isfinite(tol))return 0;size_t n=a->rows,count;if(!mulib_storage_size_mul(n,7,&count))return 0;double *mem=(double*)mulib_storage_alloc(count,sizeof(double));if(!mem)return 0;double *r=mem,*r0=r+n,*p=r0+n,*v=p+n,*s=v+n,*t=s+n,*z=t+n;memcpy(z,x,n*sizeof(double));mulib_num_csr_view_matvec(a,z,r);for(size_t i=0;i<n;i++)r0[i]=r[i]=b[i]-r[i];double rho_old=1,alpha=1,omega=1,goal=tol*MULIB_STORAGE_MAX(1,mulib_num_norm(b,n));int ok=mulib_num_norm(r,n)<=goal;size_t it=0;while(!ok&&it<max_iter){it++;double rho=mulib_num_dot(r0,r,n);if(rho==0||omega==0||!isfinite(rho))break;double beta=(rho/rho_old)*(alpha/omega);for(size_t i=0;i<n;i++)p[i]=r[i]+beta*(p[i]-omega*v[i]);mulib_num_csr_view_matvec(a,p,v);double den=mulib_num_dot(r0,v,n);if(!den)break;alpha=rho/den;for(size_t i=0;i<n;i++)s[i]=r[i]-alpha*v[i];if(mulib_num_norm(s,n)<=goal){for(size_t i=0;i<n;i++)z[i]+=alpha*p[i];ok=1;break;}mulib_num_csr_view_matvec(a,s,t);den=mulib_num_dot(t,t,n);if(!den)break;omega=mulib_num_dot(t,s,n)/den;for(size_t i=0;i<n;i++){z[i]+=alpha*p[i]+omega*s[i];r[i]=s[i]-omega*t[i];}ok=mulib_num_norm(r,n)<=goal;rho_old=rho;}if(iterations)*iterations=it;if(ok&&mulib_num_all_finite(z,n))memcpy(x,z,n*sizeof(double));else ok=0;free(mem);return ok;}
#ifdef __cplusplus
}
#endif
#endif

/* MuLIB: num-gmres. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_GMRES in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_GMRES_H
#define MULIB_NUM_GMRES_H
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

int mulib_num_gmres(const mulib_num_csr_view *a,const double *b,double *x,size_t restart,size_t max_iter,double tol,size_t *iterations);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_GMRES) && !defined(MULIB_NUM_GMRES_H_IMPLEMENTED)
#define MULIB_NUM_GMRES_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_gmres(const mulib_num_csr_view *a,const double *b,double *x,size_t restart,size_t max_iter,double tol,size_t *iterations){if(!mulib_num_csr_view_valid(a)||a->rows!=a->cols||!b||!x||!restart||restart>SIZE_MAX-1||tol<=0||!isfinite(tol))return 0;size_t n=a->rows,m=MULIB_STORAGE_MIN(restart,n),vn,hn;if(!m||!mulib_storage_size_mul(m+1,n,&vn)||!mulib_storage_size_mul(m+1,m,&hn))return 0;double *V=(double*)mulib_storage_alloc(vn,sizeof(double)),*H=(double*)mulib_storage_alloc(hn,sizeof(double)),*z=(double*)mulib_storage_alloc(n,sizeof(double)),*w=(double*)mulib_storage_alloc(n,sizeof(double)),*g=(double*)mulib_storage_alloc(m+1,sizeof(double)),*c=(double*)mulib_storage_alloc(m,sizeof(double)),*s=(double*)mulib_storage_alloc(m,sizeof(double));if(!V||!H||!z||!w||!g||!c||!s){free(V);free(H);free(z);free(w);free(g);free(c);free(s);return 0;}memcpy(z,x,n*sizeof(double));double target=tol*MULIB_STORAGE_MAX(1,mulib_num_norm(b,n));size_t it=0;int ok=0;while(1){mulib_num_csr_view_matvec(a,z,w);for(size_t i=0;i<n;i++)w[i]=b[i]-w[i];double beta=mulib_num_norm(w,n);if(beta<=target){ok=1;break;}if(it>=max_iter||!isfinite(beta))break;for(size_t i=0;i<n;i++)V[i]=w[i]/beta;memset(H,0,hn*sizeof(double));memset(g,0,(m+1)*sizeof(double));g[0]=beta;size_t k=0;for(;k<m&&it<max_iter;k++,it++){mulib_num_csr_view_matvec(a,V+k*n,w);for(size_t j=0;j<=k;j++){double h=mulib_num_dot(w,V+j*n,n);H[j*m+k]=h;for(size_t i=0;i<n;i++)w[i]-=h*V[j*n+i];}double h=mulib_num_norm(w,n);H[(k+1)*m+k]=h;if(h>DBL_MIN)for(size_t i=0;i<n;i++)V[(k+1)*n+i]=w[i]/h;for(size_t j=0;j<k;j++){double u=H[j*m+k],v=H[(j+1)*m+k];H[j*m+k]=c[j]*u+s[j]*v;H[(j+1)*m+k]=-s[j]*u+c[j]*v;}double r=hypot(H[k*m+k],H[(k+1)*m+k]);if(r==0){k=0;break;}c[k]=H[k*m+k]/r;s[k]=H[(k+1)*m+k]/r;H[k*m+k]=r;H[(k+1)*m+k]=0;g[k+1]=-s[k]*g[k];g[k]*=c[k];if(fabs(g[k+1])<=target||h<=DBL_MIN){k++;it++;break;}}if(!k)break;for(size_t j=k;j-->0;){for(size_t i=j+1;i<k;i++)g[j]-=H[j*m+i]*g[i];g[j]/=H[j*m+j];}for(size_t j=0;j<k;j++)for(size_t i=0;i<n;i++)z[i]+=g[j]*V[j*n+i];}if(iterations)*iterations=it;if(ok&&mulib_num_all_finite(z,n))memcpy(x,z,n*sizeof(double));else ok=0;free(V);free(H);free(z);free(w);free(g);free(c);free(s);return ok;}
#ifdef __cplusplus
}
#endif
#endif

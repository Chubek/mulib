/* MuLIB / Amb3r: num-bicgstab. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_BICGSTAB in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_BICGSTAB_H
#define MULIB_NUM_BICGSTAB_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_bicgstab(const ma_csr *a,const double *b,double *x,size_t max_iter,double tol,size_t *iterations);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_BICGSTAB) && !defined(MULIB_NUM_BICGSTAB_H_IMPLEMENTED)
#define MULIB_NUM_BICGSTAB_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_bicgstab(const ma_csr *a,const double *b,double *x,size_t max_iter,double tol,size_t *iterations){if(!ma_csr_valid(a)||a->rows!=a->cols||!b||!x||tol<=0||!isfinite(tol))return 0;size_t n=a->rows,count;if(!ma_mul(n,7,&count))return 0;double *mem=(double*)ma_alloc(count,sizeof(double));if(!mem)return 0;double *r=mem,*r0=r+n,*p=r0+n,*v=p+n,*s=v+n,*t=s+n,*z=t+n;memcpy(z,x,n*sizeof(double));ma_csr_mv(a,z,r);for(size_t i=0;i<n;i++)r0[i]=r[i]=b[i]-r[i];double rho_old=1,alpha=1,omega=1,goal=tol*MA_MAX(1,ma_norm(b,n));int ok=ma_norm(r,n)<=goal;size_t it=0;while(!ok&&it<max_iter){it++;double rho=ma_dot(r0,r,n);if(rho==0||omega==0||!isfinite(rho))break;double beta=(rho/rho_old)*(alpha/omega);for(size_t i=0;i<n;i++)p[i]=r[i]+beta*(p[i]-omega*v[i]);ma_csr_mv(a,p,v);double den=ma_dot(r0,v,n);if(!den)break;alpha=rho/den;for(size_t i=0;i<n;i++)s[i]=r[i]-alpha*v[i];if(ma_norm(s,n)<=goal){for(size_t i=0;i<n;i++)z[i]+=alpha*p[i];ok=1;break;}ma_csr_mv(a,s,t);den=ma_dot(t,t,n);if(!den)break;omega=ma_dot(t,s,n)/den;for(size_t i=0;i<n;i++){z[i]+=alpha*p[i]+omega*s[i];r[i]=s[i]-omega*t[i];}ok=ma_norm(r,n)<=goal;rho_old=rho;}if(iterations)*iterations=it;if(ok&&ma_finite(z,n))memcpy(x,z,n*sizeof(double));else ok=0;free(mem);return ok;}
#ifdef __cplusplus
}
#endif
#endif

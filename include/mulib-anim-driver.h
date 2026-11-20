/** @file mulib-anim-driver.h
 * Ordered, acyclic scalar dependency execution.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller supplies topology and expression engine.
 */
#ifndef MULIB_ANIM_DRIVER_H
#define MULIB_ANIM_DRIVER_H
#include "mulib-mesh-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef bool (*mulib_anim_driver_fn)(void*,size_t,const double*,size_t,double*);
/* offset_count size_t elements >=n+1; deps[dep_count], values[n], scratch[cap]
 * doubles. All writable/read-only spans disjoint. Validate topology completely
 * before callbacks; deps refer only to earlier results. Trusted synchronous fn
 * must not mutate inputs. Callback failure retains earlier completed values,
 * but does not commit the failed value. Scratch may change. */
static inline bool mulib_anim_driver_eval(const size_t *offset,size_t offset_count,const size_t *deps,size_t dep_count,size_t n,double *values,double *scratch,size_t cap,mulib_anim_driver_fn fn,void *ctx){
 if(!offset||n==SIZE_MAX||offset_count<n+1||n+1>SIZE_MAX/sizeof *offset||n>SIZE_MAX/sizeof *values||dep_count>SIZE_MAX/sizeof *deps||cap>SIZE_MAX/sizeof *scratch||(!values&&n)||!fn||(!deps&&dep_count)||(!scratch&&cap))return false;
 if(!mulib_mesh_disjoint(values,n*sizeof *values,scratch,cap*sizeof *scratch)||!mulib_mesh_disjoint(values,n*sizeof *values,offset,(n+1)*sizeof *offset)||!mulib_mesh_disjoint(values,n*sizeof *values,deps,dep_count*sizeof *deps)||!mulib_mesh_disjoint(scratch,cap*sizeof *scratch,offset,(n+1)*sizeof *offset)||!mulib_mesh_disjoint(scratch,cap*sizeof *scratch,deps,dep_count*sizeof *deps)||offset[0]||offset[n]!=dep_count)return false;
 for(size_t i=0;i<n;i++){if(offset[i+1]<offset[i]||offset[i+1]>dep_count||offset[i+1]-offset[i]>cap)return false;for(size_t j=offset[i];j<offset[i+1];j++)if(deps[j]>=i)return false;}
 for(size_t i=0;i<n;i++){size_t k=offset[i+1]-offset[i];for(size_t j=0;j<k;j++)scratch[j]=values[deps[offset[i]+j]];double result=0;if(!fn(ctx,i,scratch,k,&result)||!isfinite(result))return false;values[i]=result;}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif

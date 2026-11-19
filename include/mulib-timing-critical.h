/* MuLIB / Amb3r: timing-critical. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_CRITICAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_CRITICAL_H
#define MULIB_TIMING_CRITICAL_H
#include "mulib-amb3r-base.h"
#include "mulib-formal-counterexample.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_critical(const double *slack,const size_t *pred,size_t n,size_t *path,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_CRITICAL) && !defined(MULIB_TIMING_CRITICAL_H_IMPLEMENTED)
#define MULIB_TIMING_CRITICAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_critical(const double *slack,const size_t *pred,size_t n,size_t *path,size_t capacity,size_t *length){if(!slack||!pred||!n||!length)return 0;size_t bad=MA_NONE;for(size_t i=0;i<n;i++)if(isfinite(slack[i])&&(bad==MA_NONE||slack[i]<slack[bad]))bad=i;return bad!=MA_NONE&&mulib_formal_counterexample(pred,n,bad,path,capacity,length);}
#ifdef __cplusplus
}
#endif
#endif

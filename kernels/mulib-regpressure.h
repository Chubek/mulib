/* MuLIB compiler tools: Exact peak simultaneous live intervals with inclusive endpoints.
 * ISO C11 single-header module. Define MULIB_IMPL_REGPRESSURE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_REGPRESSURE_H
#define MULIB_REGPRESSURE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-liveintervals.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-liveintervals.h"
int mulib_regpressure_peak(const mulib_liveinterval_t *ranges,size_t n,size_t *maximum);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_REGPRESSURE_H */
#if defined(MULIB_IMPL_REGPRESSURE) && !defined(MULIB_REGPRESSURE_IMPLEMENTED)
#define MULIB_REGPRESSURE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_regpressure_peak(const mulib_liveinterval_t *r,size_t n,size_t *out){if((!r&&n)||!out)return 0;
size_t max=0;
for(size_t i=0;i<n;i++){if(!mulib_liveinterval_valid(r+i))return 0;
size_t peak=0;
for(size_t j=0;j<n;j++)if(r[j].start<=r[i].start&&r[j].end>=r[i].start)peak++;
if(peak>max)max=peak;
}*out=max;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif

/* MuLIB compiler tools: Immutable inclusive live-interval descriptors and overlap calculations.
 * ISO C11 single-header module. Define MULIB_IMPL_LIVEINTERVALS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LIVEINTERVALS_H
#define MULIB_LIVEINTERVALS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t value,start,end;
} mulib_liveinterval_t;
int mulib_liveinterval_valid(const mulib_liveinterval_t *x);
int mulib_liveinterval_overlap(const mulib_liveinterval_t *a,const mulib_liveinterval_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LIVEINTERVALS_H */
#if defined(MULIB_IMPL_LIVEINTERVALS) && !defined(MULIB_LIVEINTERVALS_IMPLEMENTED)
#define MULIB_LIVEINTERVALS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_liveinterval_valid(const mulib_liveinterval_t *x){return x&&x->start<=x->end;
}
int mulib_liveinterval_overlap(const mulib_liveinterval_t *a,const mulib_liveinterval_t *b){return mulib_liveinterval_valid(a)&&mulib_liveinterval_valid(b)&&a->start<=b->end&&b->start<=a->end;
}
#ifdef __cplusplus
}
#endif
#endif

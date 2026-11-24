/* MuLIB: timing-sta. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_STA in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_STA_H
#define MULIB_TIMING_STA_H
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
#include "mulib-graph-core.h"
#include "mulib-timing-propagate.h"
#include "mulib-timing-slack.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_sta(size_t n,const mulib_graph_weighted_edge *edges,size_t m,const double *input,const double *output,double *arrive,double *required,double *slack,size_t *predecessor,double *wns,double *tns);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_STA) && !defined(MULIB_TIMING_STA_H_IMPLEMENTED)
#define MULIB_TIMING_STA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_sta(size_t n,const mulib_graph_weighted_edge *edges,size_t m,const double *input,const double *output,double *arrive,double *required,double *slack,size_t *predecessor,double *wns,double *tns){return mulib_timing_propagate(n,edges,m,input,output,arrive,required,predecessor)&&mulib_timing_slack(n,arrive,required,slack,wns,tns);}
#ifdef __cplusplus
}
#endif
#endif

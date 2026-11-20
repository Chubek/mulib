/** @file mulib-timeseries.h
 * @brief Bounded immutable financial time-series with knowledge-time filtering.
 * Define MULIB_IMPL_TIMESERIES once; C11, -lm. No owner or persistence.
 * Caller owns n points, valid for the call; output must not overlap them.
 * Both clocks are nonnegative Unix microseconds. They are independently
 * filtered; known_at is when the value became usable, not an event clock.
 */
#ifndef MULIB_TIMESERIES_H
#define MULIB_TIMESERIES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_timeseries_point {
    int64_t event_at; /* monotonically strictly increasing Unix microseconds */
    int64_t known_at; /* when point became visible */
    double value;
} mulib_timeseries_point_t;
int mulib_timeseries_validate(const mulib_timeseries_point_t *points,size_t n);
/** Average of latest window visible points whose event_at<=cutoff and
 * known_at<=knowledge_cutoff. window in [1,4096]. Returns 0 for invalid
 * input, insufficient eligible points or overflow.
 * Preserve out on failure. Inherits all caveats of nonuniform sampling. */
int mulib_timeseries_sma_asof(const mulib_timeseries_point_t *points,size_t n,
                              int64_t cutoff,int64_t knowledge_cutoff,
                              size_t window,double *out);
/** Returns 1 if found, 0 if absent, -1 if invalid. Only writes *out on 1. */
int mulib_timeseries_latest_asof(const mulib_timeseries_point_t *points,size_t n,
                                 int64_t cutoff,int64_t knowledge_cutoff,
                                 mulib_timeseries_point_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMESERIES) && !defined(MULIB_TIMESERIES_IMPLEMENTED)
#define MULIB_TIMESERIES_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add storage-backed segmented histories, irregular observation policy, corporate-action-adjusted views and time-zone-safe resampling.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <math.h>
#include <float.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timeseries_validate(const mulib_timeseries_point_t *p,size_t n){
    size_t i;if(!p||!n||n>10000000)return 0;
    for(i=0;i<n;++i)if(p[i].event_at<0||p[i].known_at<0||
        !isfinite(p[i].value)||(i&&p[i].event_at<=p[i-1].event_at))return 0;
    return 1;
}
static size_t mulib_timeseries_upper_bound_(const mulib_timeseries_point_t *p,size_t hi,
                                               int64_t cutoff){
    size_t lo=0;
    while(lo<hi){
        size_t mid=lo+(hi-lo)/2;
        if(p[mid].event_at<=cutoff)lo=mid+1;
        else hi=mid;
    }
    return lo;
}
int mulib_timeseries_sma_asof(const mulib_timeseries_point_t *p,size_t n,
                              int64_t cutoff,int64_t known,size_t window,double *out){
    size_t i,count=0;long double total=0,mean;
    if(!out||cutoff<0||known<0||window==0||window>4096||
       !mulib_timeseries_validate(p,n))return 0;
    for(i=mulib_timeseries_upper_bound_(p,n,cutoff);i>0&&count<window;--i){
        if(p[i-1].known_at<=known){
            total+=p[i-1].value;++count;
        }
    }
    if(count<window)return 0;
    mean=total/count;
    if(!isfinite(mean)||fabsl(mean)>DBL_MAX)return 0;
    *out=(double)mean;return 1;
}
int mulib_timeseries_latest_asof(const mulib_timeseries_point_t *p,size_t n,
                                 int64_t cutoff,int64_t known,
                                 mulib_timeseries_point_t *out){
    size_t i;
    if(!out||cutoff<0||known<0||!mulib_timeseries_validate(p,n))return -1;
    for(i=mulib_timeseries_upper_bound_(p,n,cutoff);i>0;--i)if(p[i-1].known_at<=known){
        *out=p[i-1];return 1;
    }
    return 0;
}
#ifdef __cplusplus
}
#endif
#endif

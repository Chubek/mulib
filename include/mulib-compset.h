/** @file mulib-compset.h
 * @brief Freshness-filtered, property-deduplicated comparable market snapshots.
 * Prices are canonical Money minor units and MUST already describe the same
 * guest/stay scenario; this kernel does not infer accommodation equivalence.
 * Define MULIB_IMPL_COMPSET once, requires mulib-money.h.
 */
#ifndef MULIB_COMPSET_H
#define MULIB_COMPSET_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_compset_offer {
    uint64_t property_id;
    mulib_money_t effective_nightly_price;
    double relevance_weight;
    int64_t observed_at; /* Unix seconds */
    int active;
} mulib_compset_offer_t;
typedef struct mulib_compset_filter {
    int64_t as_of;             /* Unix seconds, required */
    int64_t max_age_seconds;   /* zero: no freshness cutoff */
    size_t minimum_properties; /* at least one */
} mulib_compset_filter_t;
typedef struct mulib_compset_sample {
    uint64_t property_id;
    mulib_money_t price;
    double weight;
    int64_t observed_at;
} mulib_compset_sample_t;
/** Select lowest-priced active offer from each property, ties broken by most
 * recent observation then lowest weight (conservative when identical money
 * and timestamp disagree on relevance). Sorted by property ID. The host must
 * map cross-platform listings to the same canonical property_id; weights are
 * never added across offers. Reject mixed units among freshness-eligible
 * offers, invalid active rows and insufficient matches. On failure preserve
 * output AND count. Input arrays and outputs must not overlap. */
int mulib_compset_build(const mulib_compset_offer_t *offers,size_t n,
                        const mulib_compset_filter_t *filter,
                        mulib_compset_sample_t *samples,size_t capacity,
                        size_t *count);
typedef enum mulib_compset_reason {
    MULIB_COMPSET_SELECTED=1,
    MULIB_COMPSET_INACTIVE,
    MULIB_COMPSET_FUTURE,
    MULIB_COMPSET_STALE,
    MULIB_COMPSET_PROPERTY_DUPLICATE
} mulib_compset_reason_t;
/** As build, additionally writes one reason per input row (capacity >= n).
 * For an exact tie, the earliest input receives the selected reason; the
 * resulting property sample is identical. There are no writes to any
 * output on failure, including insufficient eligible properties. */
int mulib_compset_build_diagnosed(const mulib_compset_offer_t *offers,size_t n,
    const mulib_compset_filter_t *filter,mulib_compset_sample_t *samples,
    size_t capacity,size_t *count,mulib_compset_reason_t *reasons,size_t reason_capacity);
/** Export exactly represented minor-unit numeric values + weights to StatsProc.
 * Reject minor outside [0,2^53]; never silently round large Money values.
 * All arrays must be distinct, non-overlapping borrows of at least n elements.
 * Use StatsProc quantile for unweighted Hyndman–Fan type 7 interpolation
 * (which may return fractional minor units) or StatsProc weighted_quantile
 * for the discrete weighted CDF over the selected offers' weights. Equal
 * weights in the latter do not give the type 7 estimator. Outputs unchanged
 * on failure. */
int mulib_compset_extract(const mulib_compset_sample_t *samples,size_t n,
                          double *prices_minor,double *weights,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COMPSET) && !defined(MULIB_COMPSET_IMPLEMENTED)
#define MULIB_COMPSET_IMPLEMENTED
/* FINPRED_AGENT_TODO: Calibrate property relevance and quality adjustment, validate upstream platform-to-property identity mappings, and define population eligibility beyond the shared caller-normalized scenario.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_compset_candidate_ {
    mulib_compset_sample_t sample;
    size_t index;
} mulib_compset_candidate_;
static int mulib_compset_cmp_(const void *aa,const void *bb){
    const mulib_compset_candidate_ *x=(const mulib_compset_candidate_ *)aa;
    const mulib_compset_candidate_ *y=(const mulib_compset_candidate_ *)bb;
    const mulib_compset_sample_t *a=&x->sample,*b=&y->sample;
    if(a->property_id<b->property_id)return -1;
    if(a->property_id>b->property_id)return 1;
    if(a->price.minor<b->price.minor)return -1;
    if(a->price.minor>b->price.minor)return 1;
    if(a->observed_at>b->observed_at)return -1;
    if(a->observed_at<b->observed_at)return 1;
    if(a->weight<b->weight)return -1;
    if(a->weight>b->weight)return 1;
    return (x->index>y->index)-(x->index<y->index);
}
static int mulib_compset_build_(const mulib_compset_offer_t *offers,size_t n,
    const mulib_compset_filter_t *f,mulib_compset_sample_t *out,size_t cap,
    size_t *count,mulib_compset_reason_t *reasons){
    mulib_compset_candidate_ *temp;
    uint8_t status[4096];
    size_t i,used=0,unique=0;
    mulib_money_t money={0};int have_currency=0;
    if(!offers||!f||!out||!count||n==0||n>4096||cap==0||
       f->as_of<0||f->max_age_seconds<0||f->minimum_properties==0||
       f->minimum_properties>4096)return 0;
    temp=(mulib_compset_candidate_ *)malloc(n*sizeof(*temp));
    if(!temp)return 0;
    for(i=0;i<n;++i){const mulib_compset_offer_t *o=&offers[i];
        if(o->active!=0&&o->active!=1){free(temp);return 0;}
        if(!o->active){status[i]=MULIB_COMPSET_INACTIVE;continue;}
        if(o->property_id==0||!mulib_money_valid(o->effective_nightly_price)||
           o->effective_nightly_price.minor<0||o->observed_at<0||
           !isfinite(o->relevance_weight)||o->relevance_weight<=0){free(temp);return 0;}
        if(o->observed_at>f->as_of){status[i]=MULIB_COMPSET_FUTURE;continue;}
        if(f->max_age_seconds && f->as_of-o->observed_at>f->max_age_seconds){
            status[i]=MULIB_COMPSET_STALE;continue;
        }
        if(!have_currency){money=o->effective_nightly_price;have_currency=1;}
        else if(money.decimals!=o->effective_nightly_price.decimals||
          memcmp(money.currency,o->effective_nightly_price.currency,4)){free(temp);return 0;}
        temp[used].sample.property_id=o->property_id;
        temp[used].sample.price=o->effective_nightly_price;
        temp[used].sample.weight=o->relevance_weight;
        temp[used].sample.observed_at=o->observed_at;
        temp[used].index=i;
        ++used;
    }
    qsort(temp,used,sizeof(*temp),mulib_compset_cmp_);
    for(i=0;i<used;++i){
        if(i==0||temp[i].sample.property_id!=temp[unique-1].sample.property_id){
            status[temp[i].index]=MULIB_COMPSET_SELECTED;
            temp[unique++]=temp[i];
        }else status[temp[i].index]=MULIB_COMPSET_PROPERTY_DUPLICATE;
    }
    if(unique<f->minimum_properties||unique>cap){free(temp);return 0;}
    for(i=0;i<unique;++i)out[i]=temp[i].sample;
    if(reasons)for(i=0;i<n;++i)reasons[i]=(mulib_compset_reason_t)status[i];
    *count=unique;
    free(temp);return 1;
}
int mulib_compset_build(const mulib_compset_offer_t *offers,size_t n,
    const mulib_compset_filter_t *f,mulib_compset_sample_t *out,size_t cap,size_t *count){
    return mulib_compset_build_(offers,n,f,out,cap,count,NULL);
}
int mulib_compset_build_diagnosed(const mulib_compset_offer_t *offers,size_t n,
    const mulib_compset_filter_t *f,mulib_compset_sample_t *out,size_t cap,size_t *count,
    mulib_compset_reason_t *reasons,size_t reason_cap){
    if(!reasons||reason_cap<n)return 0;
    return mulib_compset_build_(offers,n,f,out,cap,count,reasons);
}
int mulib_compset_extract(const mulib_compset_sample_t *s,size_t n,
                          double *p,double *w,size_t cap){
    size_t i;
    if(!s||!p||!w||p==w||!n||n>cap)return 0;
    for(i=0;i<n;++i){
        if(!mulib_money_valid(s[i].price)||s[i].price.minor<0||
           s[i].price.minor>INT64_C(9007199254740992)||
           !isfinite(s[i].weight)||s[i].weight<=0||
           (i && (s[i].price.decimals!=s[0].price.decimals||
                  memcmp(s[i].price.currency,s[0].price.currency,4))))return 0;
    }
    for(i=0;i<n;++i){p[i]=(double)s[i].price.minor;w[i]=s[i].weight;}
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif

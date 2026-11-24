/** @file mulib-competprice.h
 * @brief Deterministic competitor-aware pricing policies with hard safeguards.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_COMPETPRICE in one TU; link -lm.
 * No demand modeling is implied by following a competitor benchmark.
 */
#ifndef MULIB_COMPETPRICE_H
#define MULIB_COMPETPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_competprice_rival {
  double price;   /* >= 0 */
  double weight;  /* >= 0; credibility / market share */
  double quality; /* > 0 for quality-adjusted method */
  int active;     /* zero skips rival */
} mulib_competprice_rival_t;
typedef enum mulib_competprice_policy {
  MULIB_COMPETPRICE_WEIGHTED_MEDIAN,
  MULIB_COMPETPRICE_WEIGHTED_MEAN,
  MULIB_COMPETPRICE_QUANTILE,
  MULIB_COMPETPRICE_LOWEST,
  MULIB_COMPETPRICE_LEADER,
  MULIB_COMPETPRICE_QUALITY_ADJUSTED
} mulib_competprice_policy_t;
typedef struct mulib_competprice_config {
  mulib_competprice_policy_t policy;
  double percentile;        /* [0,1] for quantile */
  double relative_gap;      /* -0.05 = 5 percent under benchmark; > -1 */
  double absolute_gap;      /* subtracted from post-relative quote */
  double own_quality;       /* >0 for quality-adjusted */
  double quality_exponent;  /* [0,4] for quality-adjusted */
  double min_price, max_price; /* inclusive hard bounds */
  double previous_price;    /* >=0; 0 disables movement constraint */
  double max_relative_change; /* [0,1]; 0 = no limit */
} mulib_competprice_config_t;
typedef struct mulib_competprice_result {
  double benchmark, unbounded_price, price;
  size_t competitors_used;
  int clamped;
} mulib_competprice_result_t;
/** Infeasible intersection of price bands fails without touching output.
 * Active rivals must have finite nonnegative price/weight; their zero weights
 * are accepted but cannot support weighted policies. */
int mulib_competprice_recommend(const mulib_competprice_rival_t *rivals, size_t n,
    const mulib_competprice_config_t *options, mulib_competprice_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COMPETPRICE) && !defined(MULIB_COMPETPRICE_IMPLEMENTED)
#define MULIB_COMPETPRICE_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_competprice_recommend(const mulib_competprice_rival_t *r, size_t n,
    const mulib_competprice_config_t *c, mulib_competprice_result_t *out) {
  double *p=NULL,*w=NULL,benchmark=0,lower,upper,quote; size_t i,k=0,leader=0;
  mulib_competprice_result_t answer;
  if (!r || !c || !out || !n || n>SIZE_MAX/sizeof(double) ||
      !isfinite(c->min_price)||!isfinite(c->max_price)||c->min_price<0||c->max_price<c->min_price||
      !isfinite(c->relative_gap)||c->relative_gap<=-1||!isfinite(c->absolute_gap)||
      !isfinite(c->previous_price)||c->previous_price<0||
      !isfinite(c->max_relative_change)||c->max_relative_change<0||c->max_relative_change>1) return 0;
  p=(double *)malloc(n*sizeof(*p));w=(double *)malloc(n*sizeof(*w));
  if (!p||!w) {free(p);free(w);return 0;}
  for (i=0;i<n;++i) if (r[i].active) {
    if (!isfinite(r[i].price)||r[i].price<0||!isfinite(r[i].weight)||r[i].weight<0) goto fail;
    if (c->policy==MULIB_COMPETPRICE_QUALITY_ADJUSTED) {
      if (!isfinite(c->own_quality)||c->own_quality<=0||!isfinite(c->quality_exponent)||
          c->quality_exponent<0||c->quality_exponent>4||!isfinite(r[i].quality)||r[i].quality<=0) goto fail;
      p[k]=c->quality_exponent==0?r[i].price:
        (double)((long double)r[i].price*powl((long double)c->own_quality/r[i].quality,c->quality_exponent));
    } else p[k]=r[i].price;
    if (!isfinite(p[k])) goto fail;
    w[k]=r[i].weight;
    if (k==0 || w[k]>w[leader]) leader=k;
    ++k;
  }
  if (!k) goto fail;
  switch(c->policy) {
    case MULIB_COMPETPRICE_WEIGHTED_MEDIAN:
    case MULIB_COMPETPRICE_QUALITY_ADJUSTED:
      if (!mulib_statsproc_weighted_quantile(p,w,k,0.5,&benchmark)) goto fail;
      break;
    case MULIB_COMPETPRICE_WEIGHTED_MEAN:
      if (!mulib_statsproc_weighted_mean(p,w,k,&benchmark)) goto fail;
      break;
    case MULIB_COMPETPRICE_QUANTILE:
      if (!mulib_statsproc_quantile(p,k,c->percentile,&benchmark)) goto fail;
      break;
    case MULIB_COMPETPRICE_LOWEST:
      benchmark=p[0];for(i=1;i<k;++i) if(p[i]<benchmark)benchmark=p[i];break;
    case MULIB_COMPETPRICE_LEADER:
      benchmark=p[leader];break;
    default: goto fail;
  }
  quote=(double)((long double)benchmark*(1.0L+c->relative_gap)-c->absolute_gap);
  if (!isfinite(quote)) goto fail;
  lower=c->min_price;upper=c->max_price;
  if (c->previous_price>0 && c->max_relative_change>0) {
    double a=c->previous_price*(1-c->max_relative_change),b=c->previous_price*(1+c->max_relative_change);
    if (!isfinite(a)||!isfinite(b)) goto fail;
    if(a>lower)lower=a;
    if(b<upper)upper=b;
  }
  if(lower>upper)goto fail;
  answer.benchmark=benchmark;answer.unbounded_price=quote;
  answer.price=quote<lower?lower:(quote>upper?upper:quote);
  answer.clamped=(answer.price!=quote);answer.competitors_used=k;
  free(p);free(w);*out=answer;return 1;
fail:
  free(p);free(w);return 0;
}
#ifdef __cplusplus
}
#endif
#endif

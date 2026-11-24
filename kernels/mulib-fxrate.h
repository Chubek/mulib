/** @file mulib-fxrate.h
 * @brief Checked rational FX conversion between integer minor-unit amounts.
 * Define MULIB_IMPL_FXRATE once, depends on mulib-money.h.
 * Explicit rate units: target minor units / source minor unit.
 */
#ifndef MULIB_FXRATE_H
#define MULIB_FXRATE_H
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_fxrate {
    char source_currency[4], target_currency[4];
    uint8_t source_decimals,target_decimals;
    int64_t numerator,denominator; /* positive, target_minor/source_minor */
    int64_t observed_at;           /* rate observation time, Unix seconds */
} mulib_fxrate_t;
/** Exactly one rate conversion; rounded by canonical Money scale.
 * Reject future quotes and stale rates (max_age=0 disables cutoff).
 * This legacy form assumes the rate was available at observed_at. No
 * triangulation or live rate discovery. Preserves *out on failure. */
int mulib_fxrate_convert(mulib_money_t source,const mulib_fxrate_t *rate,
                         int64_t as_of,int64_t max_age_seconds,
                         mulib_money_t *out);
/** Also check availability when the rate was received after observation.
 * Require observed_at <= known_at <= as_of, in Unix seconds. The age check
 * still uses observed_at. Preserves *out on failure. */
int mulib_fxrate_convert_asof(mulib_money_t source,const mulib_fxrate_t *rate,
                              int64_t known_at,int64_t as_of,int64_t max_age_seconds,
                              mulib_money_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FXRATE) && !defined(MULIB_FXRATE_IMPLEMENTED)
#define MULIB_FXRATE_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add bid/ask side selection, quote expiry, path/triangulation policy and explicit intermediate rounding; provide audited rate-source provenance.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fxrate_convert_asof(mulib_money_t source,const mulib_fxrate_t *r,
                              int64_t known_at,int64_t as_of,int64_t max_age,mulib_money_t *out){
    mulib_money_t temp,meta;
    if(!r||!out||as_of<0||max_age<0||r->observed_at<0||
       known_at<r->observed_at||known_at>as_of||(max_age&&as_of-r->observed_at>max_age)||
       r->numerator<=0||r->denominator<=0||r->target_currency[3]!=0||
       !mulib_money_valid(source)||
       source.decimals!=r->source_decimals||
       memcmp(source.currency,r->source_currency,4)||
       !mulib_money_make(&meta,0,r->target_currency,r->target_decimals)||
       !mulib_money_scale(source,r->numerator,r->denominator,&temp))return 0;
    return mulib_money_make(out,temp.minor,meta.currency,meta.decimals);
}
int mulib_fxrate_convert(mulib_money_t source,const mulib_fxrate_t *r,
                         int64_t as_of,int64_t max_age,mulib_money_t *out){
    return r&&mulib_fxrate_convert_asof(source,r,r->observed_at,as_of,max_age,out);
}
#ifdef __cplusplus
}
#endif
#endif

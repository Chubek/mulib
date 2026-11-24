/** @file mulib-risk.h
 * @brief Bounded portfolio gross exposure and single-instrument concentration.
 * Define MULIB_IMPL_RISK once; requires mulib-portfolio.h and mulib-money.h.
 * Does not compute VaR, credit risk, VaR confidence bands or broker margin.
 */
#ifndef MULIB_RISK_H
#define MULIB_RISK_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-portfolio.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_risk_limits {
    mulib_money_t max_gross_market_value; /* hard cap, same currency */
    size_t max_positions;
    uint16_t max_single_name_bps; /* 0 disabled; 1..10000 fraction of gross */
} mulib_risk_limits_t;
typedef struct mulib_risk_result {
    mulib_money_t observed_gross_market_value;
    uint64_t worst_instrument_id; /* greatest violating exposure, lowest ID on tie; 0 if none */
    int allowed;
    uint32_t violations; /* 1=gross; 2=count; 4=concentration */
} mulib_risk_result_t;
/** Returns 1 when policy evaluated (even if !allowed), 0 on invalid input.
 * Output unchanged on error. Concentration threshold is exact floor in minor
 * units, so a fractional minor unit never passes as permitted exposure. */
int mulib_risk_check(const mulib_portfolio_position_t *positions,size_t n,
                     const mulib_risk_limits_t *limits,mulib_risk_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RISK) && !defined(MULIB_RISK_IMPLEMENTED)
#define MULIB_RISK_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add position/sector/net limits, pre-trade simulations, stress losses, VaR methodologies and hard kill-switch semantics outside pure calculations.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_risk_check(const mulib_portfolio_position_t *p,size_t n,
                     const mulib_risk_limits_t *lim,mulib_risk_result_t *out){
    mulib_money_t sum;mulib_risk_result_t result;
    size_t i;int64_t threshold=0,worst=0;
    if(!p||!lim||!out||!lim->max_positions||
       lim->max_single_name_bps>10000||
       !mulib_money_valid(lim->max_gross_market_value)||
       lim->max_gross_market_value.minor<0||
       !mulib_portfolio_gross(p,n,&sum)||
       sum.decimals!=lim->max_gross_market_value.decimals||
       memcmp(sum.currency,lim->max_gross_market_value.currency,4))return 0;
    result.observed_gross_market_value=sum;
    result.worst_instrument_id=0;result.violations=0;
    if(n>lim->max_positions)result.violations|=2;
    if(sum.minor>lim->max_gross_market_value.minor)result.violations|=1;
    if(lim->max_single_name_bps){
        int64_t gross=sum.minor;
        int64_t bps=lim->max_single_name_bps;
        /* Avoid overflow of gross*bps via quotient and remainder. */
        threshold=(gross/10000)*bps+((gross%10000)*bps)/10000;
        for(i=0;i<n;++i){
            mulib_money_t value;
            int64_t qty=p[i].quantity<0?-p[i].quantity:p[i].quantity;
            if(!mulib_money_scale(p[i].mark_price,qty,1,&value))return 0;
            if(value.minor>threshold){
                result.violations|=4;
                if(value.minor>worst||
                   (value.minor==worst&&p[i].instrument_id<result.worst_instrument_id)){
                    result.worst_instrument_id=p[i].instrument_id;
                    worst=value.minor;
                }
            }
        }
    }
    result.allowed=result.violations==0;
    *out=result;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

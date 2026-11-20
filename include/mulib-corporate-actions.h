/** @file mulib-corporate-actions.h
 * @brief Exact share split quantity adjustments and checked price/dividend
 * transformations. Define MULIB_IMPL_CORPORATE_ACTIONS once.
 * Not an exchange corporate-action calendar or adjusted-price data provider.
 */
#ifndef MULIB_CORPORATE_ACTIONS_H
#define MULIB_CORPORATE_ACTIONS_H
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_corporate_actions_split {
    int64_t new_shares; /* numerator, 1..INT64_MAX */
    int64_t old_shares; /* denominator, 1..INT64_MAX */
} mulib_corporate_actions_split_t;
/** Adjust nonnegative integer share count EXACTLY; fails if fractional result,
 * overflow or invalid split. Caller must model cash-in-lieu separately.
 * All three transforms return 1 on success; 0 leaves the output unchanged. */
int mulib_corporate_actions_adjust_shares(int64_t current_shares,
    mulib_corporate_actions_split_t split,int64_t *out);
/** Adjust price inversely, using canonical Money rounding. Split-adjusted
 * price times new shares may differ by rounding remainder. */
int mulib_corporate_actions_adjust_price(mulib_money_t price,
    mulib_corporate_actions_split_t split,mulib_money_t *out);
/** Cash dividend amount for nonnegative integral shares, no reinvestment. */
int mulib_corporate_actions_dividend(mulib_money_t per_share,
                                     int64_t share_count,mulib_money_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CORPORATE_ACTIONS) && !defined(MULIB_CORPORATE_ACTIONS_IMPLEMENTED)
#define MULIB_CORPORATE_ACTIONS_IMPLEMENTED
/* FINPRED_AGENT_TODO: Model multi-event dividends, cash-in-lieu, fractional positions, ex/record/pay-date ordering, spinoffs and split-adjusted history.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
static int64_t mulib_corporate_actions_gcd_(int64_t a,int64_t b){
    while(b){int64_t t=a%b;a=b;b=t;}return a;
}
int mulib_corporate_actions_adjust_shares(int64_t shares,
    mulib_corporate_actions_split_t s,int64_t *out){
    int64_t g,n,d;
    if(!out||shares<0||s.new_shares<=0||s.old_shares<=0)return 0;
    g=mulib_corporate_actions_gcd_(s.new_shares,s.old_shares);
    n=s.new_shares/g;d=s.old_shares/g;
    if(shares%d||shares/d>INT64_MAX/n)return 0;
    *out=(shares/d)*n;return 1;
}
int mulib_corporate_actions_adjust_price(mulib_money_t price,
    mulib_corporate_actions_split_t s,mulib_money_t *out){
    if(!out||!mulib_money_valid(price)||price.minor<0||
       s.new_shares<=0||s.old_shares<=0)return 0;
    return mulib_money_scale(price,s.old_shares,s.new_shares,out);
}
int mulib_corporate_actions_dividend(mulib_money_t per_share,
                                     int64_t shares,mulib_money_t *out){
    if(!out||shares<0||!mulib_money_valid(per_share)||per_share.minor<0)return 0;
    return mulib_money_scale(per_share,shares,1,out);
}
#ifdef __cplusplus
}
#endif
#endif

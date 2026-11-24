/** @file mulib-portfolio.h
 * @brief Checked fixed-position mark-to-market P&L using Money minor units.
 * Define MULIB_IMPL_PORTFOLIO once; requires canonical mulib-money.h.
 * Quantities are integral shares/lots in a caller-specified single lot unit.
 */
#ifndef MULIB_PORTFOLIO_H
#define MULIB_PORTFOLIO_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_portfolio_position {
    uint64_t instrument_id; /* nonzero, unique within array */
    int64_t quantity;       /* positive long, negative short, nonzero, != INT64_MIN */
    mulib_money_t average_cost, mark_price; /* price per lot, same currency */
} mulib_portfolio_position_t;
typedef struct mulib_portfolio_summary {
    mulib_money_t unrealized_pnl; /* signed */
    mulib_money_t gross_market_value; /* sum(abs(qty) * mark) */
    size_t positions;
} mulib_portfolio_summary_t;
/** One currency and scale across positions, no fees/corporate actions/realized
 * P&L. Return 1 success, 0 invalid/intermediate or final overflow; output
 * unchanged on failure. Validates at most 4096 caller-owned positions. */
int mulib_portfolio_mark(const mulib_portfolio_position_t *positions,size_t n,
                         mulib_portfolio_summary_t *out);
/** Compute just gross exposure, with the same position validation but no
 * dependency on the representability of a hypothetical P&L. */
int mulib_portfolio_gross(const mulib_portfolio_position_t *positions,size_t n,
                          mulib_money_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PORTFOLIO) && !defined(MULIB_PORTFOLIO_IMPLEMENTED)
#define MULIB_PORTFOLIO_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add realized P&L, FIFO/average-cost tax lots, margin, multi-currency base conversion and transaction costs; separate live broker positions from snapshots.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_portfolio_calculate_(const mulib_portfolio_position_t *p,size_t n,
                                       int want_pnl,mulib_portfolio_summary_t *out){
    mulib_portfolio_summary_t r;mulib_money_t zero,delta,pnl,value;
    size_t i,j;
    if(!p||!out||n==0||n>4096||!mulib_money_valid(p[0].mark_price)||
       !mulib_money_make(&zero,0,p[0].mark_price.currency,p[0].mark_price.decimals))return 0;
    r.unrealized_pnl=zero;r.gross_market_value=zero;r.positions=n;
    for(i=0;i<n;++i){
        const mulib_portfolio_position_t *x=&p[i];
        int64_t qty;
        if(!x->instrument_id||!x->quantity||x->quantity==INT64_MIN||
           !mulib_money_valid(x->mark_price)||!mulib_money_valid(x->average_cost)||
           x->mark_price.minor<0||x->average_cost.minor<0||
           zero.decimals!=x->mark_price.decimals||
           zero.decimals!=x->average_cost.decimals||
           memcmp(zero.currency,x->mark_price.currency,4)||
           memcmp(zero.currency,x->average_cost.currency,4))return 0;
        for(j=0;j<i;++j)if(p[j].instrument_id==x->instrument_id)return 0;
        qty=x->quantity<0?-x->quantity:x->quantity;
        if(want_pnl){
            if(!mulib_money_sub(x->mark_price,x->average_cost,&delta)||
               (x->quantity<0&&!mulib_money_sub(zero,delta,&delta))||
               !mulib_money_scale(delta,qty,1,&pnl)||
               !mulib_money_add(r.unrealized_pnl,pnl,&r.unrealized_pnl))return 0;
        }
        if(!mulib_money_scale(x->mark_price,qty,1,&value)||
           !mulib_money_add(r.gross_market_value,value,&r.gross_market_value))return 0;
    }
    *out=r;return 1;
}
int mulib_portfolio_mark(const mulib_portfolio_position_t *p,size_t n,
                         mulib_portfolio_summary_t *out){
    return mulib_portfolio_calculate_(p,n,1,out);
}
int mulib_portfolio_gross(const mulib_portfolio_position_t *p,size_t n,mulib_money_t *out){
    mulib_portfolio_summary_t result;
    if(!out||!mulib_portfolio_calculate_(p,n,0,&result))return 0;
    *out=result.gross_market_value;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

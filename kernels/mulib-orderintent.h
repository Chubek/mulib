/** @file mulib-orderintent.h
 * @brief Validate pre-execution order intentions and notional guardrails.
 * Define MULIB_IMPL_ORDERINTENT once; requires canonical mulib-money.h.
 * NEVER routes or executes orders; no authorization or exchange guarantees.
 */
#ifndef MULIB_ORDERINTENT_H
#define MULIB_ORDERINTENT_H
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_orderintent_side {MULIB_ORDERINTENT_BUY=1,MULIB_ORDERINTENT_SELL=2} mulib_orderintent_side_t;
typedef struct mulib_orderintent {
    uint64_t client_order_id,instrument_id;
    mulib_orderintent_side_t side;
    int64_t quantity;
    mulib_money_t limit_price;
    int64_t created_at,expires_at; /* Unix seconds, inclusive/expiry-exclusive */
} mulib_orderintent_t;
typedef struct mulib_orderintent_policy {
    int64_t max_quantity;
    mulib_money_t max_notional;
    int64_t min_tick_minor; /* >=1 */
    int allow_buy,allow_sell; /* 0/1 */
} mulib_orderintent_policy_t;
/** Validate expiry, quantity, tick, configured side and max notional.
 * The policy flags are computation limits, not authenticated authorization.
 * Returns 1 on accepted intent, 0 otherwise; leaves *out_notional unchanged
 * on failure. This operation changes no trading state. */
int mulib_orderintent_validate(const mulib_orderintent_t *intent,
                               const mulib_orderintent_policy_t *policy,
                               int64_t now,mulib_money_t *out_notional);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ORDERINTENT) && !defined(MULIB_ORDERINTENT_IMPLEMENTED)
#define MULIB_ORDERINTENT_IMPLEMENTED
/* FINPRED_AGENT_TODO: Model venue tick and lot schedules. Any approval, execution idempotency and order lifecycle belong in a separately authorized external-action layer.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_orderintent_validate(const mulib_orderintent_t *i,
                               const mulib_orderintent_policy_t *p,
                               int64_t now,mulib_money_t *out){
    mulib_money_t total;
    if(!i||!p||!out||now<0||!i->client_order_id||!i->instrument_id||
       i->quantity<=0||p->max_quantity<=0||i->quantity>p->max_quantity||
       i->created_at<0||i->expires_at<=i->created_at||
       now<i->created_at||now>=i->expires_at||
       (i->side!=MULIB_ORDERINTENT_BUY&&i->side!=MULIB_ORDERINTENT_SELL)||
       p->allow_buy<0||p->allow_buy>1||p->allow_sell<0||p->allow_sell>1||
       (i->side==MULIB_ORDERINTENT_BUY&&!p->allow_buy)||
       (i->side==MULIB_ORDERINTENT_SELL&&!p->allow_sell)||
       p->min_tick_minor<=0||!mulib_money_valid(i->limit_price)||
       !mulib_money_valid(p->max_notional)||i->limit_price.minor<=0||
       p->max_notional.minor<0||
       i->limit_price.decimals!=p->max_notional.decimals||
       memcmp(i->limit_price.currency,p->max_notional.currency,4)||
       i->limit_price.minor%p->min_tick_minor!=0||
       !mulib_money_scale(i->limit_price,i->quantity,1,&total)||
       total.minor>p->max_notional.minor)return 0;
    *out=total;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

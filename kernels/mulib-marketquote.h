/** @file mulib-marketquote.h
 * @brief Checked level-1 bid/ask snapshots in canonical Money minor units.
 * Define MULIB_IMPL_MARKETQUOTE once; requires mulib-money.h.
 * This is a quote validation primitive, not a trading venue connector.
 */
#ifndef MULIB_MARKETQUOTE_H
#define MULIB_MARKETQUOTE_H
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_marketquote {
    uint64_t instrument_id, venue_id;
    int64_t quote_at, received_at;  /* Unix microseconds, nonnegative */
    mulib_money_t bid, ask;
    uint64_t bid_size, ask_size;   /* positive size in caller-defined lot unit */
} mulib_marketquote_t;
/** Validate currency match, noncrossed prices and causality received>=quoted. */
int mulib_marketquote_valid(const mulib_marketquote_t *q);
/** Require both quote and receipt by the cutoff (Unix microseconds) and both
 * prices on a positive zero-origin tick (minor units). A max age of zero
 * disables the freshness cutoff; otherwise age is measured from quote_at.
 * Caller supplies the venue tick applicable to both prices. */
int mulib_marketquote_usable_at(const mulib_marketquote_t *q,int64_t cutoff,
                                  int64_t max_age_us,int64_t tick_minor);
/** Exact integer spread, floor midpoint in minor units (no floating error).
 * Outputs must be distinct and are unchanged on failure. */
int mulib_marketquote_spread_mid(const mulib_marketquote_t *q,
                                 mulib_money_t *spread,mulib_money_t *mid);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MARKETQUOTE) && !defined(MULIB_MARKETQUOTE_IMPLEMENTED)
#define MULIB_MARKETQUOTE_IMPLEMENTED
/* FINPRED_AGENT_TODO: Model venue tick schedules and lot units, feed permissions, correction identities and quote condition codes.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_marketquote_valid(const mulib_marketquote_t *q){
    return q&&q->instrument_id&&q->venue_id&&q->quote_at>=0&&
        q->received_at>=q->quote_at&&q->bid_size&&q->ask_size&&
        mulib_money_valid(q->bid)&&mulib_money_valid(q->ask)&&
        q->bid.minor>0&&q->ask.minor>=q->bid.minor&&
        q->bid.decimals==q->ask.decimals&&
        !memcmp(q->bid.currency,q->ask.currency,4);
}
int mulib_marketquote_usable_at(const mulib_marketquote_t *q,int64_t cutoff,
                                  int64_t max_age,int64_t tick){
    return cutoff>=0&&max_age>=0&&tick>0&&mulib_marketquote_valid(q)&&
           q->received_at<=cutoff&&(!max_age||cutoff-q->quote_at<=max_age)&&
           q->bid.minor%tick==0&&q->ask.minor%tick==0;
}
int mulib_marketquote_spread_mid(const mulib_marketquote_t *q,
                                 mulib_money_t *spread,mulib_money_t *mid){
    mulib_money_t s,m;
    if(!spread||!mid||spread==mid||!mulib_marketquote_valid(q)||
       !mulib_money_sub(q->ask,q->bid,&s))return 0;
    m=q->bid;m.minor+=s.minor/2;
    *spread=s;*mid=m;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

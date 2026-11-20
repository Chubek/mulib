/** @file mulib-ruleeval.h
 * @brief Deterministic bounded pricing-rule executor, not a general rule DSL.
 * Define MULIB_IMPL_RULEEVAL once; depends on canonical mulib-money.h.
 */
#ifndef MULIB_RULEEVAL_H
#define MULIB_RULEEVAL_H
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_RULEEVAL_SHORTCUT 1u
#define MULIB_RULEEVAL_FLOOR 2u
#define MULIB_RULEEVAL_CEILING 4u
#define MULIB_RULEEVAL_CHANGE 8u
#define MULIB_RULEEVAL_TICK 16u

typedef struct mulib_ruleeval_policy {
    int enable_shortcut;
    int64_t max_shortcut_difference_minor; /* choose lower if <= threshold */
    int enable_floor, enable_ceiling, enable_previous;
    mulib_money_t floor, ceiling, previous;
    uint16_t max_change_bps;  /* 0 disables; <=10000 */
    int64_t tick_minor;       /* 0 disables; positive otherwise */
} mulib_ruleeval_policy_t;
typedef struct mulib_ruleeval_result {
    mulib_money_t requested, recommended;
    uint32_t fired_flags;
} mulib_ruleeval_result_t;
typedef enum mulib_ruleeval_comparison {
    MULIB_RULEEVAL_BELOW_THRESHOLD=0,
    MULIB_RULEEVAL_AT_MOST_THRESHOLD=1
} mulib_ruleeval_comparison_t;
typedef enum mulib_ruleeval_floor_precedence {
    MULIB_RULEEVAL_CLAMP_TO_FLOOR=0,
    MULIB_RULEEVAL_SKIP_BELOW_FLOOR=1
} mulib_ruleeval_floor_precedence_t;
typedef struct mulib_ruleeval_percentile_shortcut {
    uint16_t target_bps; /* 0..10000, e.g. 5000 for P50 */
    uint16_t offset_bps; /* positive, <= target, e.g. 1000 for P40 */
    mulib_ruleeval_comparison_t comparison;
    mulib_ruleeval_floor_precedence_t floor_precedence;
} mulib_ruleeval_percentile_shortcut_t;
typedef struct mulib_ruleeval_percentile_quote {
    uint16_t percentile_bps;
    mulib_money_t amount;
} mulib_ruleeval_percentile_quote_t;
/** Evaluate shortcut, apply hard floor/ceiling and previous-price movement band,
 * then choose the nearest feasible integer tick (ties select lower tick).
 * Return 0 for infeasible intersection, overflow or invalid parameters.
 * Preserve *out on failure. Legacy shortcut uses inclusive comparison; the
 * movement-band half-width is rounded by Money (ties away from zero). */
int mulib_ruleeval_apply(const mulib_ruleeval_policy_t *policy,
                         mulib_money_t candidate,mulib_money_t lower,
                         mulib_ruleeval_result_t *out);
/** Compute the lower quantile to request from the caller's estimator. */
int mulib_ruleeval_shortcut_percentile(const mulib_ruleeval_percentile_shortcut_t *rule,
                                       uint16_t *out_lower_bps);
/** Apply the price rule to quotes from the same market snapshot and estimator.
 * Labels must match target and target-offset; lower must not exceed candidate.
 * The comparison uses policy.max_shortcut_difference_minor in the quote's
 * currency and scale (e.g. 500 for EUR at scale 2). An enabled floor is
 * always hard: either clamp after applying the shortcut or skip the shortcut
 * if it would start below the floor. Ceiling, movement band and tick must
 * also be feasible. No estimator, independently chosen floor or policy
 * history is inferred by this kernel. Failure preserves *out. */
int mulib_ruleeval_apply_percentiles(const mulib_ruleeval_policy_t *policy,
                         const mulib_ruleeval_percentile_shortcut_t *rule,
                         mulib_ruleeval_percentile_quote_t candidate,
                         mulib_ruleeval_percentile_quote_t lower,
                         mulib_ruleeval_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RULEEVAL) && !defined(MULIB_RULEEVAL_IMPLEMENTED)
#define MULIB_RULEEVAL_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add versioned policy identity and structured rejection evidence outside this primitive; verify compiler integration and policy replay.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_ruleeval_same_(mulib_money_t a,mulib_money_t b){
    return mulib_money_valid(a)&&mulib_money_valid(b)&&
           a.decimals==b.decimals&&memcmp(a.currency,b.currency,4)==0;
}
static int mulib_ruleeval_apply_(const mulib_ruleeval_policy_t *p,
                         mulib_money_t candidate,mulib_money_t lower,
                         mulib_ruleeval_comparison_t comparison,
                         mulib_ruleeval_floor_precedence_t precedence,
                         mulib_ruleeval_result_t *out){
    int64_t lo=0,hi=INT64_MAX,amount,band,prev,tick;
    int64_t original_lo=0,original_hi=INT64_MAX,change_lo=0,change_hi=INT64_MAX;
    mulib_ruleeval_result_t answer;
    mulib_money_t change;
    uint32_t fired=0;
    if(!p||!out||!mulib_ruleeval_same_(candidate,lower)||
       candidate.minor<0||lower.minor<0||p->enable_shortcut<0||p->enable_shortcut>1||
       p->enable_floor<0||p->enable_floor>1||p->enable_ceiling<0||p->enable_ceiling>1||
       p->enable_previous<0||p->enable_previous>1||
       p->max_shortcut_difference_minor<0||p->max_change_bps>10000||p->tick_minor<0)return 0;
    if(p->enable_floor){if(!mulib_ruleeval_same_(candidate,p->floor)||p->floor.minor<0)return 0;
        lo=p->floor.minor;}
    if(p->enable_ceiling){if(!mulib_ruleeval_same_(candidate,p->ceiling)||p->ceiling.minor<0)return 0;
        hi=p->ceiling.minor;}
    original_lo=lo;original_hi=hi;
    if(p->enable_previous){
        if(!mulib_ruleeval_same_(candidate,p->previous)||p->previous.minor<0)return 0;
        prev=p->previous.minor;
        if(p->max_change_bps){
            if(!mulib_money_scale(p->previous,p->max_change_bps,10000,&change))return 0;
            band=change.minor;
            change_lo=prev-band;
            change_hi=band>INT64_MAX-prev?INT64_MAX:prev+band;
            if(change_lo>lo)lo=change_lo;
            if(change_hi<hi)hi=change_hi;
        }
    }
    if(lo>hi)return 0;
    amount=candidate.minor;
    if(p->enable_shortcut && lower.minor<=candidate.minor &&
       (precedence==MULIB_RULEEVAL_CLAMP_TO_FLOOR||lower.minor>=original_lo)){
        int64_t diff=candidate.minor-lower.minor;
        if(diff<p->max_shortcut_difference_minor ||
           (comparison==MULIB_RULEEVAL_AT_MOST_THRESHOLD &&
            diff==p->max_shortcut_difference_minor)){
            amount=lower.minor;fired|=MULIB_RULEEVAL_SHORTCUT;
        }
    }
    if(amount<lo){
        amount=lo;
        fired|=(lo==change_lo&&change_lo>original_lo)?MULIB_RULEEVAL_CHANGE:MULIB_RULEEVAL_FLOOR;
    }
    if(amount>hi){
        amount=hi;
        fired|=(hi==change_hi&&change_hi<original_hi)?MULIB_RULEEVAL_CHANGE:MULIB_RULEEVAL_CEILING;
    }
    tick=p->tick_minor;
    if(tick){
        int64_t first=lo/tick+(lo%tick!=0),last=hi/tick;
        int64_t below,above,best,ri;
        if(first>last)return 0;
        ri=amount/tick;
        if(ri<first)ri=first;
        if(ri>last)ri=last;
        below=ri*tick;
        above=(ri<last)?(ri+1)*tick:below;
        best=below;
        if(above>below&&amount>=below && above-amount<amount-below)best=above;
        if(best!=amount)fired|=MULIB_RULEEVAL_TICK;
        amount=best;
    }
    answer.requested=candidate;answer.recommended=candidate;
    answer.recommended.minor=amount;answer.fired_flags=fired;
    *out=answer;return 1;
}
int mulib_ruleeval_apply(const mulib_ruleeval_policy_t *p,
                         mulib_money_t candidate,mulib_money_t lower,
                         mulib_ruleeval_result_t *out){
    return mulib_ruleeval_apply_(p,candidate,lower,MULIB_RULEEVAL_AT_MOST_THRESHOLD,
                                  MULIB_RULEEVAL_CLAMP_TO_FLOOR,out);
}
int mulib_ruleeval_shortcut_percentile(const mulib_ruleeval_percentile_shortcut_t *r,
                                       uint16_t *out_lower_bps){
    if(!r||!out_lower_bps||r->target_bps>10000||!r->offset_bps||
       r->offset_bps>r->target_bps||
       (r->comparison!=MULIB_RULEEVAL_BELOW_THRESHOLD&&
        r->comparison!=MULIB_RULEEVAL_AT_MOST_THRESHOLD)||
       (r->floor_precedence!=MULIB_RULEEVAL_CLAMP_TO_FLOOR&&
        r->floor_precedence!=MULIB_RULEEVAL_SKIP_BELOW_FLOOR))return 0;
    *out_lower_bps=(uint16_t)(r->target_bps-r->offset_bps);
    return 1;
}
int mulib_ruleeval_apply_percentiles(const mulib_ruleeval_policy_t *p,
                         const mulib_ruleeval_percentile_shortcut_t *r,
                         mulib_ruleeval_percentile_quote_t candidate,
                         mulib_ruleeval_percentile_quote_t lower,
                         mulib_ruleeval_result_t *out){
    uint16_t expected_lower;
    if(!mulib_ruleeval_shortcut_percentile(r,&expected_lower)||
       candidate.percentile_bps!=r->target_bps||lower.percentile_bps!=expected_lower||
       lower.amount.minor>candidate.amount.minor)return 0;
    return mulib_ruleeval_apply_(p,candidate.amount,lower.amount,r->comparison,
                                 r->floor_precedence,out);
}
#ifdef __cplusplus
}
#endif
#endif

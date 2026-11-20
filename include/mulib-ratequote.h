/** @file mulib-ratequote.h
 * @brief Normalize a full-stay accommodation quote with checked Money arithmetic.
 * Define MULIB_IMPL_RATEQUOTE once; depends on canonical mulib-money.h.
 * All amounts have identical currency/decimal metadata, in MINOR units.
 */
#ifndef MULIB_RATEQUOTE_H
#define MULIB_RATEQUOTE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_ratequote_input {
    uint32_t nights;          /* 1..365 */
    uint16_t adults, children;/* at least one guest */
    uint8_t available;
    uint8_t taxes_included;   /* taxes already contained in room_charge */
    uint8_t fees_included;    /* mandatory_fees already in room_charge */
    mulib_money_t room_charge;     /* total room price for FULL STAY */
    mulib_money_t mandatory_fees;  /* full-stay charges */
    mulib_money_t taxes;           /* full-stay taxes */
} mulib_ratequote_input_t;
typedef struct mulib_ratequote_result {
    mulib_money_t total_payable;
    mulib_money_t rounded_nightly_average;
    uint32_t nights;
    int64_t remainder_minor; /* exact total = (total/nights)*nights + remainder */
} mulib_ratequote_result_t;
/** Return 1 on success, 0 on invalid/unavailable/overflow. Output unchanged on
 * failure. Remainder uses integer truncation; average uses Money's rounding. */
int mulib_ratequote_normalize(const mulib_ratequote_input_t *in,
                              mulib_ratequote_result_t *out);
typedef enum mulib_ratequote_meal {
    MULIB_RATEQUOTE_MEAL_UNKNOWN=0,
    MULIB_RATEQUOTE_ROOM_ONLY,
    MULIB_RATEQUOTE_BREAKFAST,
    MULIB_RATEQUOTE_HALF_BOARD,
    MULIB_RATEQUOTE_FULL_BOARD,
    MULIB_RATEQUOTE_ALL_INCLUSIVE
} mulib_ratequote_meal_t;
typedef enum mulib_ratequote_cancellation {
    MULIB_RATEQUOTE_CANCELLATION_UNKNOWN=0,
    MULIB_RATEQUOTE_NONREFUNDABLE,
    MULIB_RATEQUOTE_FREE_UNTIL
} mulib_ratequote_cancellation_t;
typedef struct mulib_ratequote_terms {
    int64_t check_in_day, check_out_day; /* same caller-selected civil-day calendar */
    int64_t observed_at, book_from, book_until; /* Unix seconds; booking window [from,until) */
    uint64_t source_id;                  /* nonzero upstream provenance ID, not authenticated */
    mulib_ratequote_meal_t meal;
    mulib_ratequote_cancellation_t cancellation;
    int64_t free_cancel_before;         /* exclusive Unix cutoff if FREE_UNTIL, else zero */
    const uint8_t *child_ages;           /* ages 0..17 at check-in */
    size_t child_age_count;              /* equals input children; NULL allowed if zero */
    const mulib_money_t *night_room_charges; /* optional room-charge breakdown */
    size_t night_room_charge_count;      /* 0 (absent) or precisely input nights */
} mulib_ratequote_terms_t;
/** As normalize, plus typed terms, exact date and bookability checks at
 * booking_at, and optional reconciliation of the nightly room charges
 * (not tax/fee breakdowns) with the full-stay room_charge. The sum of taxes
 * and fees marked as included must not exceed room_charge. Child ages and
 * nightly charges are call-scoped read-only borrows. Cancellation and meal
 * metadata are validated, not applied as monetary charges. If there is no
 * longer free cancellation at booking time, FREE_UNTIL is still distinct
 * from NONREFUNDABLE: consumers must compare the cutoff as appropriate.
 * Invalid/not bookable/overflow returns 0, preserving *out. */
int mulib_ratequote_normalize_dated(const mulib_ratequote_input_t *in,
    const mulib_ratequote_terms_t *terms,int64_t requested_check_in_day,
    int64_t requested_check_out_day,int64_t booking_at,mulib_ratequote_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RATEQUOTE) && !defined(MULIB_RATEQUOTE_IMPLEMENTED)
#define MULIB_RATEQUOTE_IMPLEMENTED
/* FINPRED_AGENT_TODO: Model service-charge, per-guest fee and tax line items without double counting; validate externally supplied provenance, calendar and cancellation semantics.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ratequote_normalize(const mulib_ratequote_input_t *in,
                              mulib_ratequote_result_t *out) {
    mulib_money_t sum,avg;
    mulib_ratequote_result_t answer;
    if(!in||!out||in->nights==0||in->nights>365||in->available!=1||
       !((unsigned)in->adults+(unsigned)in->children) ||
       in->taxes_included>1||in->fees_included>1||
       !mulib_money_valid(in->room_charge)||!mulib_money_valid(in->mandatory_fees)||
       !mulib_money_valid(in->taxes)||in->room_charge.minor<0||
       in->mandatory_fees.minor<0||in->taxes.minor<0) return 0;
    if(in->room_charge.decimals!=in->mandatory_fees.decimals||
       in->room_charge.decimals!=in->taxes.decimals||
       memcmp(in->room_charge.currency,in->mandatory_fees.currency,4)||
       memcmp(in->room_charge.currency,in->taxes.currency,4)) return 0;
    sum=in->room_charge;
    if(!in->fees_included && !mulib_money_add(sum,in->mandatory_fees,&sum))return 0;
    if(!in->taxes_included && !mulib_money_add(sum,in->taxes,&sum))return 0;
    if(!mulib_money_scale(sum,1,(int64_t)in->nights,&avg))return 0;
    answer.total_payable=sum;
    answer.rounded_nightly_average=avg;
    answer.nights=in->nights;
    answer.remainder_minor=sum.minor % (int64_t)in->nights;
    *out=answer;return 1;
}
int mulib_ratequote_normalize_dated(const mulib_ratequote_input_t *in,
    const mulib_ratequote_terms_t *t,int64_t check_in,int64_t check_out,
    int64_t booking,mulib_ratequote_result_t *out){
    mulib_ratequote_result_t answer;
    int64_t room_less_included_fees;
    size_t i;
    if(!in||!t||!out||check_in>=check_out||check_in!=t->check_in_day||
       check_out!=t->check_out_day||((uint64_t)check_out-(uint64_t)check_in)!=in->nights||
       !t->source_id||t->observed_at<0||t->observed_at>booking||
       t->book_from<0||t->book_until<=t->book_from||
       booking<t->book_from||booking>=t->book_until||
       t->meal<MULIB_RATEQUOTE_MEAL_UNKNOWN||t->meal>MULIB_RATEQUOTE_ALL_INCLUSIVE||
       t->cancellation<MULIB_RATEQUOTE_CANCELLATION_UNKNOWN||
       t->cancellation>MULIB_RATEQUOTE_FREE_UNTIL||
       t->free_cancel_before<0||
       (t->cancellation!=MULIB_RATEQUOTE_FREE_UNTIL&&t->free_cancel_before)||
       t->child_age_count!=in->children||(t->child_age_count&&!t->child_ages)||
       (t->night_room_charge_count&&(!t->night_room_charges||
          t->night_room_charge_count!=in->nights))||
       !mulib_ratequote_normalize(in,&answer))return 0;
    room_less_included_fees=in->room_charge.minor-
        (in->fees_included?in->mandatory_fees.minor:0);
    if(room_less_included_fees<(in->taxes_included?in->taxes.minor:0))return 0;
    for(i=0;i<t->child_age_count;++i)if(t->child_ages[i]>17)return 0;
    if(t->night_room_charge_count){
        mulib_money_t sum=in->room_charge;
        sum.minor=0;
        for(i=0;i<t->night_room_charge_count;++i){
            mulib_money_t night=t->night_room_charges[i];
            if(night.minor<0||!mulib_money_add(sum,night,&sum))return 0;
        }
        if(sum.minor!=in->room_charge.minor)return 0;
    }
    *out=answer;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

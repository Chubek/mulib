/** @file mulib-backtest.h
 * @brief Deterministic point-forecast diagnostics on time-ordered holdouts.
 * Define MULIB_IMPL_BACKTEST once; C11, -lm. Does not fit models, simulate
 * transactions, infer causality or certify the absence of look-ahead.
 */
#ifndef MULIB_BACKTEST_H
#define MULIB_BACKTEST_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_backtest_row {
    int64_t decision_at;        /* when prediction was made */
    int64_t prediction_known_at;/* MUST be <= decision_at */
    int64_t actual_known_at;    /* MUST be >= decision_at */
    double predicted, actual, baseline;
} mulib_backtest_row_t;
typedef struct mulib_backtest_replay_row {
    mulib_backtest_row_t score;
    int64_t inputs_known_at;   /* latest availability across prediction inputs,
                                 training data, parameters and selected policy */
    int64_t baseline_known_at; /* latest availability for the baseline and inputs */
    int64_t actual_event_at;   /* time of the predicted event, after decision */
} mulib_backtest_replay_row_t;
typedef struct mulib_backtest_summary {
    size_t observations;
    double mae, rmse, bias, baseline_mae, wape;
    /* wape=NaN if actual magnitudes sum to zero */
} mulib_backtest_summary_t;
/** Returns 1 on success, 0 on malformed/nonfinite/unsorted records or overflow.
 * Output unchanged on failure. */
int mulib_backtest_score(const mulib_backtest_row_t *rows,size_t n,
                         mulib_backtest_summary_t *out);
/** Score an already generated chronological holdout at a fixed label cutoff.
 * All times use nonnegative Unix seconds. Decision-time inputs, prediction
 * and baseline must be available by the decision, and the actual must satisfy
 * decision < event <= actual_known <= label_cutoff. Unavailable rows are an
 * error, not silently dropped. Callers can score separate walk-forward folds,
 * but must train/replay each fold and attest exact availability themselves.
 * Returns 1 on success, 0 on invalid inputs or overflow; preserves *out. */
int mulib_backtest_score_replay(const mulib_backtest_replay_row_t *rows,size_t n,
                                int64_t label_cutoff,mulib_backtest_summary_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BACKTEST) && !defined(MULIB_BACKTEST_IMPLEMENTED)
#define MULIB_BACKTEST_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add model training/selection and walk-forward fold orchestration; require explicit financial costs/slippage assumptions and independently verified input provenance.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <math.h>
#include <float.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_backtest_accum_ {
    long double abs_err,sq,scale,bias,base,actuals;
    int64_t previous_at;
} mulib_backtest_accum_;
static int mulib_backtest_add_(mulib_backtest_accum_ *acc,const mulib_backtest_row_t *r){
    long double e,ae;
    if(r->decision_at<acc->previous_at||r->prediction_known_at<0||
       r->prediction_known_at>r->decision_at||r->actual_known_at<r->decision_at||
       !isfinite(r->predicted)||!isfinite(r->actual)||!isfinite(r->baseline))return 0;
    e=(long double)r->predicted-r->actual;
    if(!isfinite(e))return 0;
    ae=fabsl(e);
    if(ae>acc->scale){
        long double ratio=acc->scale/ae;
        acc->sq=1+acc->sq*ratio*ratio;
        acc->scale=ae;
    }else if(ae!=0){
        long double ratio=ae/acc->scale;
        acc->sq+=ratio*ratio;
    }
    acc->abs_err+=ae;acc->bias+=e;
    acc->base+=fabsl((long double)r->baseline-r->actual);
    acc->actuals+=fabsl(r->actual);
    acc->previous_at=r->decision_at;
    return 1;
}
static int mulib_backtest_narrow_(long double n,double *out){
    if(!isfinite(n)||fabsl(n)>DBL_MAX)return 0;
    *out=(double)n;return 1;
}
static int mulib_backtest_finish_(const mulib_backtest_accum_ *acc,size_t n,
                                  mulib_backtest_summary_t *out){
    mulib_backtest_summary_t result;
    result.observations=n;
    result.wape=NAN;
    if(!isfinite(acc->actuals)||
       !mulib_backtest_narrow_(acc->abs_err/n,&result.mae)||
       !mulib_backtest_narrow_(acc->scale*sqrtl(acc->sq/n),&result.rmse)||
       !mulib_backtest_narrow_(acc->bias/n,&result.bias)||
       !mulib_backtest_narrow_(acc->base/n,&result.baseline_mae)||
       (acc->actuals!=0&&!mulib_backtest_narrow_(acc->abs_err/acc->actuals,&result.wape)))return 0;
    *out=result;return 1;
}
int mulib_backtest_score(const mulib_backtest_row_t *r,size_t n,
                         mulib_backtest_summary_t *out){
    size_t i;
    mulib_backtest_accum_ acc={0};
    if(!r||!out||n==0||n>10000000)return 0;
    for(i=0;i<n;++i)if(!mulib_backtest_add_(&acc,&r[i]))return 0;
    return mulib_backtest_finish_(&acc,n,out);
}
int mulib_backtest_score_replay(const mulib_backtest_replay_row_t *r,size_t n,
                                int64_t label_cutoff,mulib_backtest_summary_t *out){
    size_t i;
    mulib_backtest_accum_ acc={0};
    if(!r||!out||n==0||n>10000000||label_cutoff<0)return 0;
    for(i=0;i<n;++i){
        const mulib_backtest_row_t *score=&r[i].score;
        if(r[i].inputs_known_at<0||r[i].inputs_known_at>score->decision_at||
           r[i].baseline_known_at<0||r[i].baseline_known_at>score->decision_at||
           r[i].actual_event_at<=score->decision_at||r[i].actual_event_at>score->actual_known_at||
           score->actual_known_at>label_cutoff||!mulib_backtest_add_(&acc,score))return 0;
    }
    return mulib_backtest_finish_(&acc,n,out);
}
#ifdef __cplusplus
}
#endif
#endif

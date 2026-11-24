/** @file mulib-pricecurve.h
 * @brief Bounded deterministic target-percentile curves for time-to-event policies.
 * Define MULIB_IMPL_PRICECURVE exactly once. C11, link -lm.
 */
#ifndef MULIB_PRICECURVE_H
#define MULIB_PRICECURVE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_pricecurve_kind {
    MULIB_PRICECURVE_LINEAR=0,
    MULIB_PRICECURVE_POWER=1,
    MULIB_PRICECURVE_EXPONENTIAL=2,
    MULIB_PRICECURVE_PIECEWISE=3
} mulib_pricecurve_kind_t;
typedef struct mulib_pricecurve_knot {
    double days_to_event; /* >=0, strictly ascending */
    double percentile;    /* [0,1], nondecreasing with days */
} mulib_pricecurve_knot_t;
typedef struct mulib_pricecurve_config {
    mulib_pricecurve_kind_t kind;
    double near_percentile, far_percentile; /* [0,1], near<=far */
    double horizon_days;                    /* >0 */
    double speed;                           /* power exponent >0 or exp slope 0..20 */
    const mulib_pricecurve_knot_t *knots;   /* piecewise only */
    size_t knot_count;                      /* 2..64, starts at 0 */
} mulib_pricecurve_config_t;
/** Reject negative/nonfinite time; clamp at the horizon or last knot.
 * Scalar interpolation uses normalized time u=days/horizon:
 * linear u, power u^speed, and exponential (1-exp(-speed*u))/(1-exp(-speed))
 * (linear at zero speed). Piecewise segments interpolate linearly; knot
 * storage is a read-only call-scoped borrow and scalar fields are ignored.
 * Endpoints and knots return their configured percentiles exactly.
 * Validates the complete configuration. On error *out is unchanged. */
int mulib_pricecurve_evaluate(const mulib_pricecurve_config_t *config,
                              double days_to_event,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PRICECURVE) && !defined(MULIB_PRICECURVE_IMPLEMENTED)
#define MULIB_PRICECURVE_IMPLEMENTED
/* FINPRED_AGENT_TODO: Define and test hysteresis policy and calendar scheduling in callers above this pure curve.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <float.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static double mulib_pricecurve_lerp_(double a,double b,double t){
    if(t<=0)return a;
    if(t>=1)return b;
    return fmin(b,fmax(a,fma(t,b-a,a)));
}
int mulib_pricecurve_evaluate(const mulib_pricecurve_config_t *c,double days,double *out){
    double t,y;
    size_t i;
    if(!c||!out||!isfinite(days)||days<0) return 0;
    if(c->kind==MULIB_PRICECURVE_PIECEWISE){
        const mulib_pricecurve_knot_t *k=c->knots;
        if(!k||c->knot_count<2||c->knot_count>64||k[0].days_to_event!=0)return 0;
        for(i=0;i<c->knot_count;++i) {
            if(!isfinite(k[i].days_to_event)||!isfinite(k[i].percentile)||
               k[i].days_to_event<0||k[i].percentile<0||k[i].percentile>1||
               (i && (k[i].days_to_event<=k[i-1].days_to_event||
                      k[i].percentile<k[i-1].percentile))) return 0;
        }
        y=k[c->knot_count-1].percentile;
        if(days<=k[0].days_to_event)y=k[0].percentile;
        else for(i=1;i<c->knot_count;++i){
            if(days==k[i].days_to_event){y=k[i].percentile;break;}
            if(days<k[i].days_to_event){
                t=(days-k[i-1].days_to_event)/(k[i].days_to_event-k[i-1].days_to_event);
                y=mulib_pricecurve_lerp_(k[i-1].percentile,k[i].percentile,t);
                break;
            }
        }
    } else {
        if(c->kind!=MULIB_PRICECURVE_LINEAR && c->kind!=MULIB_PRICECURVE_POWER &&
           c->kind!=MULIB_PRICECURVE_EXPONENTIAL) return 0;
        if(!isfinite(c->near_percentile)||!isfinite(c->far_percentile)||
           c->near_percentile<0||c->far_percentile>1||
           c->near_percentile>c->far_percentile||
           !isfinite(c->horizon_days)||c->horizon_days<=0||
           !isfinite(c->speed)) return 0;
        if(c->kind==MULIB_PRICECURVE_POWER && c->speed<=0)return 0;
        if(c->kind==MULIB_PRICECURVE_EXPONENTIAL &&
           (c->speed<0||c->speed>20))return 0;
        if(days==0)t=0;
        else if(days>=c->horizon_days)t=1;
        else {
            t=days/c->horizon_days;
            if(c->kind==MULIB_PRICECURVE_POWER){
                /* A tiny normalized time can underflow even when raising it
                 * to a fractional exponent gives a representable result. */
                if(t<DBL_MIN)
                    t=fmin(pow(DBL_MIN,c->speed),
                           exp(c->speed*(log(days)-log(c->horizon_days))));
                else t=pow(t,c->speed);
            }
            if(c->kind==MULIB_PRICECURVE_EXPONENTIAL && c->speed>DBL_EPSILON){
                double x=c->speed*t,den=-expm1(-c->speed);
                /* For subnormal x the numerator is effectively x. Multiply
                 * after normalizing the scale to avoid an early underflow. At
                 * slopes <= machine epsilon, the whole curve uses its linear
                 * limit (absolute error <= roughly one eighth epsilon). */
                if(x<DBL_MIN)t=fmin(DBL_MIN/den,t*(c->speed/den));
                else t=-expm1(-x)/den;
            }
        }
        y=mulib_pricecurve_lerp_(c->near_percentile,c->far_percentile,t);
    }
    if(!isfinite(y)||y<0||y>1) return 0;
    *out=y;return 1;
}
#ifdef __cplusplus
}
#endif
#endif

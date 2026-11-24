/* Separate C11 client of the same canonical archive, not an application adapter. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "mulib-histprice.h"
#include "mulib-priceopt.h"

#ifdef __FAST_MATH__
#error "numerical boundary tests require IEEE floating-point semantics"
#endif

#define TYPE_IS(expr, type) _Generic((expr), type: 1, default: 0)
#define FIELD(type, field, at, field_type) \
    _Static_assert(offsetof(type, field) == at && \
                   TYPE_IS(&((type *)0)->field, field_type *), #type "." #field)
_Static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && _Alignof(int) == 4, "native status");
_Static_assert(TYPE_IS((size_t)0, unsigned long) && sizeof(size_t) == 8 && _Alignof(size_t) == 8, "LP64 size_t");
_Static_assert(sizeof(double) == 8 && _Alignof(double) == 8 &&
               FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "binary64");

typedef int (*ema_fn)(const double *, size_t, double, size_t, mulib_histprice_forecast_t *);
typedef int (*holt_fn)(const double *, size_t, double, double, double, size_t, mulib_histprice_forecast_t *);
typedef int (*optimize_fn)(const mulib_priceopt_demand_model_t *, const mulib_priceopt_constraints_t *,
                           mulib_priceopt_objective_t, mulib_priceopt_result_t *);
typedef int (*candidates_fn)(const double *, const double *, size_t, const mulib_priceopt_constraints_t *,
                             mulib_priceopt_objective_t, mulib_priceopt_result_t *);
_Static_assert(TYPE_IS(&mulib_histprice_ema, ema_fn), "EMA prototype");
_Static_assert(TYPE_IS(&mulib_histprice_holt, holt_fn), "Holt prototype");
_Static_assert(TYPE_IS(&mulib_priceopt_optimize, optimize_fn), "Optimize prototype");
_Static_assert(TYPE_IS(&mulib_priceopt_candidates, candidates_fn), "Candidates prototype");
_Static_assert(TYPE_IS((mulib_priceopt_demand_kind_t)0, unsigned int) &&
               TYPE_IS((mulib_priceopt_objective_t)0, unsigned int) &&
               _Alignof(mulib_priceopt_demand_kind_t) == 4 && _Alignof(mulib_priceopt_objective_t) == 4,
               "native unsigned enum ABI");
_Static_assert(MULIB_PRICEOPT_LINEAR == 0 && MULIB_PRICEOPT_ISOELASTIC == 1 && MULIB_PRICEOPT_LOGISTIC == 2 &&
               MULIB_PRICEOPT_PROFIT == 0 && MULIB_PRICEOPT_REVENUE == 1, "named enumerators");
_Static_assert(sizeof(mulib_histprice_forecast_t) == 16 && _Alignof(mulib_histprice_forecast_t) == 8,
               "forecast layout");
FIELD(mulib_histprice_forecast_t, point, 0, double);
FIELD(mulib_histprice_forecast_t, variance, 8, double);
_Static_assert(sizeof(mulib_priceopt_demand_model_t) == 32 && _Alignof(mulib_priceopt_demand_model_t) == 8,
               "model layout");
FIELD(mulib_priceopt_demand_model_t, kind, 0, mulib_priceopt_demand_kind_t);
FIELD(mulib_priceopt_demand_model_t, scale, 8, double);
FIELD(mulib_priceopt_demand_model_t, shape, 16, double);
FIELD(mulib_priceopt_demand_model_t, reference, 24, double);
_Static_assert(sizeof(mulib_priceopt_constraints_t) == 56 && _Alignof(mulib_priceopt_constraints_t) == 8,
               "constraints layout");
FIELD(mulib_priceopt_constraints_t, min_price, 0, double);
FIELD(mulib_priceopt_constraints_t, max_price, 8, double);
FIELD(mulib_priceopt_constraints_t, unit_cost, 16, double);
FIELD(mulib_priceopt_constraints_t, min_unit_margin, 24, double);
FIELD(mulib_priceopt_constraints_t, previous_price, 32, double);
FIELD(mulib_priceopt_constraints_t, max_relative_change, 40, double);
FIELD(mulib_priceopt_constraints_t, tick, 48, double);
_Static_assert(sizeof(mulib_priceopt_result_t) == 32 && _Alignof(mulib_priceopt_result_t) == 8,
               "decision layout");
FIELD(mulib_priceopt_result_t, price, 0, double);
FIELD(mulib_priceopt_result_t, score, 8, double);
FIELD(mulib_priceopt_result_t, expected_demand, 16, double);
FIELD(mulib_priceopt_result_t, evaluations, 24, size_t);

/* Do not use assert: this oracle also runs with release flags. */
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__); return 0; \
} } while (0)

static int numerical_edges(void) {
    const double window[] = {NAN, -0.0, 2, 6, INFINITY};
    mulib_histprice_forecast_t f = {17, 23};
    CHECK(mulib_histprice_ema(window + 1, 1, 0, 1000000, &f) == 1 &&
          f.point == 0 && signbit(f.point) && isnan(f.variance));
    /* alpha=beta=1 gives level 6, trend 4, and 4*(1/2+1/4) drift. */
    CHECK(mulib_histprice_holt(window + 1, 3, 1, 1, .5, 2, &f) == 1 &&
          f.point == 9 && isnan(f.variance));
    f = (mulib_histprice_forecast_t){17, 23};
    const double falling[] = {2, 1};
    CHECK(mulib_histprice_holt(falling, 2, 1, 1, 1, 2, &f) == 0);
    CHECK(mulib_histprice_ema(window + 1, 1, 0, 1000001, &f) == 0);
    CHECK(mulib_histprice_ema(window + 1, 1, NAN, 1, &f) == 0);
    CHECK(mulib_histprice_ema(window + 1, 4, 1, 1, &f) == 0);
    CHECK(f.point == 17 && f.variance == 23);

    const mulib_priceopt_constraints_t limits = {.min_price=6, .max_price=20, .unit_cost=4, .tick=1};
    const mulib_priceopt_demand_model_t linear = {.kind=MULIB_PRICEOPT_LINEAR, .scale=20, .shape=1};
    const mulib_priceopt_demand_model_t iso = {.kind=MULIB_PRICEOPT_ISOELASTIC, .scale=12, .reference=6};
    const mulib_priceopt_demand_model_t sigmoid = {
        .kind=MULIB_PRICEOPT_LOGISTIC, .scale=12, .shape=1, .reference=10
    };
    const mulib_priceopt_constraints_t fixed = {.min_price=10, .max_price=10, .unit_cost=4, .tick=1};
    mulib_priceopt_result_t d = {-1, -2, -3, 4};
    /* Revenue p*(20-p) has its vertex at 10; search evaluates 15 ticks. */
    CHECK(mulib_priceopt_optimize(&linear, &limits, MULIB_PRICEOPT_REVENUE, &d) == 1 &&
          d.price == 10 && d.score == 100 && d.expected_demand == 10 && d.evaluations == 15);
    CHECK(mulib_priceopt_optimize(&iso, &fixed, MULIB_PRICEOPT_PROFIT, &d) == 1 &&
          d.price == 10 && fabs(d.expected_demand-12) < 1e-12 && fabs(d.score-72) < 1e-12 && d.evaluations == 1);
    CHECK(mulib_priceopt_optimize(&sigmoid, &fixed, MULIB_PRICEOPT_PROFIT, &d) == 1 &&
          d.price == 10 && d.expected_demand == 6 && d.score == 36 && d.evaluations == 1);

    const double prices[] = {NAN, 12, 8, 9.5, 25, INFINITY};
    const double units[] = {NAN, NAN, 4, 8, 1000, 1000, NAN};
    /* Both in-grid prices score 32, so choose 8 even though 12 came first. */
    CHECK(mulib_priceopt_candidates(prices+1, units+2, 4, &limits, MULIB_PRICEOPT_PROFIT, &d) == 1 &&
          d.price == 8 && d.score == 32 && d.expected_demand == 8 && d.evaluations == 2);
    d = (mulib_priceopt_result_t){-1, -2, -3, 4};
    const double invalid_units[] = {4, 8, 1000, NAN};
    CHECK(mulib_priceopt_candidates(prices+1, invalid_units, 4, &limits, MULIB_PRICEOPT_PROFIT, &d) == 0);
    CHECK(mulib_priceopt_candidates(prices+3, units+4, 2, &limits, MULIB_PRICEOPT_PROFIT, &d) == 0);
    CHECK(mulib_priceopt_optimize(&linear, &limits, (mulib_priceopt_objective_t)99, &d) == 0);
    const mulib_priceopt_demand_model_t invalid = {.kind=(mulib_priceopt_demand_kind_t)99, .scale=20, .shape=1};
    CHECK(mulib_priceopt_optimize(&invalid, &limits, MULIB_PRICEOPT_PROFIT, &d) == 0);
    CHECK(d.price == -1 && d.score == -2 && d.expected_demand == -3 && d.evaluations == 4);
    return 1;
}

int main(void) {
    if (!numerical_edges()) return 4;
    const double observations[] = {10, 12, 14};
    mulib_histprice_forecast_t ema, holt;
    if (mulib_histprice_ema(observations, 3, 0, 1, &ema) != 1 ||
        mulib_histprice_holt(observations, 3, .5, .2, 1, 1, &holt) != 1 ||
        !(fabs(holt.point-16) < fabs(ema.point-16))) return 1;
    if (mulib_histprice_ema(observations, 3, 0, 2, &ema) != 1 ||
        mulib_histprice_holt(observations, 3, .5, .2, 1, 2, &holt) != 1) return 1;
    if (holt.point != 18) return 1;
    printf("%.13a\n%.13a\n%s\n", ema.point, holt.point, isnan(holt.variance) ? "true" : "false");

    const mulib_priceopt_demand_model_t linear = {.kind=MULIB_PRICEOPT_LINEAR, .scale=20, .shape=1};
    const mulib_priceopt_constraints_t limits = {.min_price=6, .max_price=20, .unit_cost=4, .tick=1};
    mulib_priceopt_result_t decision;
    if (mulib_priceopt_optimize(&linear, &limits, MULIB_PRICEOPT_PROFIT, &decision) != 1) return 2;
    printf("%.13a\n%.13a\n", decision.price, decision.score);
    const double candidates[] = {10, 12, 15}, demand[] = {12, 9, 4};
    if (mulib_priceopt_candidates(candidates, demand, 3, &limits, MULIB_PRICEOPT_PROFIT, &decision) != 1) return 3;
    printf("%.13a\n%zu\n", decision.price, decision.evaluations);
    /* The checked language interface rejects unequal lengths before calling C.
       Use an invalid size for the independent C failure oracle, not an OOB read. */
    int rejected = mulib_priceopt_candidates(candidates, demand, 0, &limits, MULIB_PRICEOPT_PROFIT, &decision);
    printf("%d\n", rejected);
    return rejected != 0;
}

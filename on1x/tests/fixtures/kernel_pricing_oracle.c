/* Separate C11 client of the same canonical archive, not an application adapter. */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include "mulib-histprice.h"
#include "mulib-priceopt.h"

int main(void) {
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

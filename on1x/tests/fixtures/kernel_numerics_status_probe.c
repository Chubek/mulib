/* Standalone fault-injection probe. Link the legacy adapter object, NOT the
 * MuLIB archive. These replacement symbols are never used in an application. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-histprice.h"
#include "mulib-priceopt.h"

extern bool on1x_kernel_scalar_v1_mulib_histprice_ema(
    const double *, int64_t, int64_t, int64_t, double, int64_t, double *, double *);
extern bool on1x_kernel_scalar_v1_mulib_priceopt_optimize(
    int64_t, double, double, double, double, double, double, double, double, double, double,
    int64_t, double *, double *, double *, uint64_t *);
extern bool on1x_kernel_scalar_v1_mulib_priceopt_candidates(
    const double *, int64_t, int64_t, int64_t, const double *, int64_t, int64_t, int64_t,
    double, double, double, double, double, double, double,
    int64_t, double *, double *, double *, uint64_t *);

enum { DIRTY_ZERO, DIRTY_TWO, PARTIAL_SUCCESS, FULL_SUCCESS };
static int mode, calls;

static int mock_status(void) {
    return mode == DIRTY_ZERO ? 0 : mode == DIRTY_TWO ? 2 : 1;
}

int mulib_histprice_ema(const double *p, size_t n, double a, size_t h, mulib_histprice_forecast_t *out) {
    ++calls;
    if (n != 1 || p[0] != 2 || a != 0 || h != 1) return -1;
    if (mode == PARTIAL_SUCCESS) out->variance = NAN;  /* point must remain poisoned */
    else *out = (mulib_histprice_forecast_t){2, NAN};
    return mock_status();
}

int mulib_histprice_holt(const double *p, size_t n, double a, double b, double phi,
                        size_t h, mulib_histprice_forecast_t *out) {
    (void)b; (void)phi;
    return mulib_histprice_ema(p, n, a, h, out);
}

static int mock_decision(mulib_priceopt_result_t *out) {
    if (mode == PARTIAL_SUCCESS) {
        out->price = 12;
        out->score = 64;
        out->expected_demand = 8;
        /* Missing evaluation count must prevent publishing a Decision. */
    } else *out = (mulib_priceopt_result_t){12, 64, 8, 15};
    return mock_status();
}

int mulib_priceopt_optimize(const mulib_priceopt_demand_model_t *m, const mulib_priceopt_constraints_t *c,
                           mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out) {
    ++calls;
    if (m->kind != MULIB_PRICEOPT_LINEAR || m->scale != 20 || m->shape != 1 || m->reference != 13 ||
        c->min_price != 6 || c->max_price != 20 || c->unit_cost != 4 || c->min_unit_margin != 2 ||
        c->previous_price != 10 || c->max_relative_change != .25 || c->tick != 1 ||
        objective != MULIB_PRICEOPT_REVENUE) return -1;
    return mock_decision(out);
}

int mulib_priceopt_candidates(const double *p, const double *q, size_t n, const mulib_priceopt_constraints_t *c,
                             mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out) {
    ++calls;
    (void)c; (void)objective;
    if (n != 1 || p[0] != 12 || q[0] != 8) return -1;
    return mock_decision(out);
}

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    fprintf(stderr, "%s:%d: mode %d: %s\n", __FILE__, __LINE__, mode, #__VA_ARGS__); return 1; \
} } while (0)

int main(void) {
    const double samples[] = {NAN, 2, NAN}, prices[] = {NAN, 12}, units[] = {NAN, NAN, 8};
    for (mode = DIRTY_ZERO; mode <= FULL_SUCCESS; ++mode) {
        double point = -1, variance = -2, demand = -3, price = -4, score = -5;
        uint64_t evaluations = 42;
        calls = 0;
        const bool success = mode == FULL_SUCCESS;
        CHECK(on1x_kernel_scalar_v1_mulib_histprice_ema(samples, 3, 1, 1, 0, 1, &point, &variance) == success);
        CHECK(calls == 1);
        CHECK(success ? (point == 2 && isnan(variance)) : (point == -1 && variance == -2));
        CHECK(on1x_kernel_scalar_v1_mulib_priceopt_optimize(
            1, 13, 20, 1, 20, .25, 6, 2, 10, 1, 4, 1, &demand, &price, &score, &evaluations) == success);
        CHECK(calls == 2);
        CHECK(success ? (demand == 8 && price == 12 && score == 64 && evaluations == 15) :
                        (demand == -3 && price == -4 && score == -5 && evaluations == 42));
        demand = -3; price = -4; score = -5; evaluations = 42;
        CHECK(on1x_kernel_scalar_v1_mulib_priceopt_candidates(
            prices, 2, 1, 1, units, 3, 2, 1, 20, .25, 6, 2, 10, 1, 4, 1,
            &demand, &price, &score, &evaluations) == success);
        CHECK(calls == 3);
        CHECK(success ? (demand == 8 && price == 12 && score == 64 && evaluations == 15) :
                        (demand == -3 && price == -4 && score == -5 && evaluations == 42));
    }
    /* The last element of a maximum-capacity snapshot is still accessible. */
    calls = 0;
    double point = -1, variance = -2, demand = -3, price = -4, score = -5;
    uint64_t evaluations = 42;
    const double max_capacity[256] = {[255] = 2};
    mode = FULL_SUCCESS;
    CHECK(on1x_kernel_scalar_v1_mulib_histprice_ema(max_capacity, 256, 255, 1, 0, 1, &point, &variance));
    CHECK(calls == 1 && point == 2 && isnan(variance));
    point = -1; variance = -2;

    /* Any native call here is a bug, irrespective of its return value. */
    calls = 0;
    CHECK(!on1x_kernel_scalar_v1_mulib_histprice_ema(max_capacity, 257, 255, 1, 0, 1, &point, &variance));
    CHECK(!on1x_kernel_scalar_v1_mulib_histprice_ema(samples, 3, 1, 1, 0, INT64_MIN, &point, &variance));
    CHECK(!on1x_kernel_scalar_v1_mulib_histprice_ema(samples, 3, INT64_MAX, 1, 0, 1, &point, &variance));
    CHECK(!on1x_kernel_scalar_v1_mulib_priceopt_optimize(
        99, 13, 20, 1, 20, .25, 6, 2, 10, 1, 4, 1, &demand, &price, &score, &evaluations));
    CHECK(!on1x_kernel_scalar_v1_mulib_priceopt_optimize(
        1, 13, 20, 1, 20, .25, 6, 2, 10, 1, 4, 99, &demand, &price, &score, &evaluations));
    CHECK(!on1x_kernel_scalar_v1_mulib_priceopt_candidates(
        prices, 2, 1, 1, units, 3, 1, 2, 20, .25, 6, 2, 10, 1, 4, 1,
        &demand, &price, &score, &evaluations));
    CHECK(calls == 0 && point == -1 && variance == -2 && demand == -3 &&
          price == -4 && score == -5 && evaluations == 42);
    return 0;
}

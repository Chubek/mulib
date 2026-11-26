/* Separately compiled consumer of the published adapter and real MuLIB TU. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mulib-priceopt.h"

typedef int (*candidates_fn)(const double *, const double *, size_t,
    const mulib_priceopt_constraints_t *, mulib_priceopt_objective_t, mulib_priceopt_result_t *);
_Static_assert(_Generic(&mulib_priceopt_candidates, candidates_fn: 1, default: 0),
    "candidate ABI: const price/units arrays, size_t count, constraints, enum, out pointer, int status");
_Static_assert(sizeof(int) == 4 && sizeof(size_t) == 8 && sizeof(double) == 8,
    "candidate ABI: Linux LP64 status/count/binary64 sizes");
_Static_assert(sizeof(mulib_priceopt_objective_t) == 4 &&
    MULIB_PRICEOPT_PROFIT == 0 && MULIB_PRICEOPT_REVENUE == 1, "candidate ABI: objective enum");
_Static_assert(sizeof(mulib_priceopt_result_t) == 32 && _Alignof(mulib_priceopt_result_t) == 8 &&
    offsetof(mulib_priceopt_result_t, price) == 0 && offsetof(mulib_priceopt_result_t, score) == 8 &&
    offsetof(mulib_priceopt_result_t, expected_demand) == 16 &&
    offsetof(mulib_priceopt_result_t, evaluations) == 24, "candidate ABI: result layout");

extern bool on1x_kernel_scalar_v1_mulib_priceopt_candidates(
    const double *, int64_t, int64_t, int64_t, const double *, int64_t, int64_t, int64_t,
    double, double, double, double, double, double, double, int64_t,
    double *, double *, double *, uint64_t *);

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__); return 1; \
} } while (0)

static const mulib_priceopt_result_t sentinel = {-11, -12, -13, 42};

static bool untouched(mulib_priceopt_result_t out) {
    return out.price == sentinel.price && out.score == sentinel.score &&
        out.expected_demand == sentinel.expected_demand && out.evaluations == sentinel.evaluations;
}

static bool adapt(const double *p, int64_t ps, int64_t pn, const double *u, int64_t un,
    int64_t objective, mulib_priceopt_result_t *out) {
    uint64_t evaluations = out->evaluations;
    bool status = on1x_kernel_scalar_v1_mulib_priceopt_candidates(
        p, 4, ps, pn, u, 5, 2, un, 22, .5, 3, 2, 12, 2, 4, objective,
        &out->expected_demand, &out->price, &out->score, &evaluations);
    out->evaluations = evaluations;
    return status;
}

int main(void) {
    double prices[] = {-99, 12, 8, -99}, units[] = {-99, -99, 7, 14, -99};
    double saved_prices[4], saved_units[5];
    memcpy(saved_prices, prices, sizeof prices);
    memcpy(saved_units, units, sizeof units);
    const mulib_priceopt_constraints_t limits = {
        .min_price = 3, .max_price = 22, .unit_cost = 4, .min_unit_margin = 2,
        .previous_price = 12, .max_relative_change = .5, .tick = 2,
    };
    mulib_priceopt_result_t direct = sentinel, adapter = sentinel;
    CHECK(mulib_priceopt_candidates(prices + 1, units + 2, 2, &limits, MULIB_PRICEOPT_PROFIT, &direct) == 1);
    CHECK(adapt(prices, 1, 2, units, 2, 0, &adapter));
    CHECK(direct.price == 8 && direct.score == 56 && direct.expected_demand == 14 && direct.evaluations == 2);
    CHECK(adapter.price == 8 && adapter.score == 56 && adapter.expected_demand == 14 && adapter.evaluations == 2);

    direct = adapter = sentinel;
    /* Last row is invalid after a valid provisional winner. No output escapes. */
    double invalid[] = {-99, -99, 7, -1, -99};
    CHECK(mulib_priceopt_candidates(prices + 1, invalid + 2, 2, &limits, MULIB_PRICEOPT_PROFIT, &direct) == 0);
    CHECK(untouched(direct));
    CHECK(!adapt(prices, 1, 2, invalid, 2, 0, &adapter) && untouched(adapter));

    /* These are adapter rejections; all actual backing buffers are valid. */
    CHECK(!adapt(prices, 1, 2, units, 1, 0, &adapter) && untouched(adapter));
    CHECK(!adapt(prices, 4, 2, units, 2, 0, &adapter) && untouched(adapter));
    CHECK(!adapt(prices, 1, 2, units, 2, 99, &adapter) && untouched(adapter));
    CHECK(!memcmp(prices, saved_prices, sizeof prices) && !memcmp(units, saved_units, sizeof units));
    return 0;
}

/** @file mulib-spice-tran.h
 * @brief Fixed-timestep backward-Euler transient for linear R/C/I/V networks, zero initial
 * capacitor voltages, time-invariant DC sources, no inductors.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_TRAN in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_TRAN_H
#define MULIB_SPICE_TRAN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-netlist.h"
#include "mulib-spice-stamp.h"
#include "mulib-spice-r.h"
#include "mulib-spice-c.h"
#include "mulib-spice-independent.h"
#include "mulib-num-dense.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_tran_fixed(const mulib_spice_netlist_t *net, double dt, size_t steps,
                           uint16_t probe, double *samples);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_TRAN_H */
#if defined(MULIB_IMPL_SPICE_TRAN) && !defined(MULIB_SPICE_TRAN_H_IMPLEMENTED)
#define MULIB_SPICE_TRAN_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_tran_fixed(const mulib_spice_netlist_t *net, double dt, size_t steps,
                           uint16_t probe, double *samples) {
    if (!mulib_spice_netlist_valid(net) || !samples || steps > SIZE_MAX / sizeof(double) ||
        !isfinite(dt) || dt <= 0 || probe == 0 || probe > net->nodes || !steps)
        return 0;
    size_t vs = 0;
    for (size_t i = 0; i < net->parts; i++)
        if (net->parts_data[i].kind == 'V')
            vs++;
    size_t n = net->nodes + vs;
    if (!n)
        return 0;
    double *previous = calloc(net->nodes, sizeof(double)), *current = calloc(n, sizeof(double)),
           *result = malloc(steps * sizeof(double));
    if (!previous || !current || !result) {
        free(previous);
        free(current);
        free(result);
        return 0;
    }
    int ok = 1;
    for (size_t step = 0; step < steps && ok; step++) {
        mulib_spice_stamp_t s = {0};
        ok = mulib_spice_stamp_init(&s, n);
        size_t v = 0;
        for (size_t i = 0; i < net->parts && ok; i++) {
            const mulib_spice_part_t *p = &net->parts_data[i];
            double va = p->a ? previous[p->a - 1] : 0, vb = p->b ? previous[p->b - 1] : 0;
            switch (p->kind) {
            case 'R':
                ok = mulib_spice_r_stamp(&s, p->a, p->b, p->value);
                break;
            case 'C':
                ok = mulib_spice_c_backward_euler(&s, p->a, p->b, p->value, dt, va, vb);
                break;
            case 'I':
                ok = mulib_spice_independent_current(&s, p->a, p->b, p->value);
                break;
            case 'V':
                ok = mulib_spice_independent_voltage(&s, p->a, p->b, net->nodes + v++, p->value);
                break;
            default:
                ok = 0;
            }
        }
        if (ok)
            ok = mulib_num_dense_solve(n, s.a, s.b, current, 1e-15);
        if (ok) {
            memcpy(previous, current, net->nodes * sizeof(double));
            result[step] = previous[probe - 1];
        }
        mulib_spice_stamp_free(&s);
    }
    if (ok)
        memcpy(samples, result, steps * sizeof(double));
    free(previous);
    free(current);
    free(result);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_TRAN */

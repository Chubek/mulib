/** @file mulib-spice-op.h
 * @brief DC operating-point solver for linear R/I/V circuits with open capacitors; dense MNA and
 * partial-pivot Gaussian elimination.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_OP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_OP_H
#define MULIB_SPICE_OP_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-netlist.h"
#include "mulib-spice-stamp.h"
#include "mulib-spice-r.h"
#include "mulib-spice-independent.h"
#include "mulib-num-dense.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_op_solve(const mulib_spice_netlist_t *net, double *node_voltages, size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_OP_H */
#if defined(MULIB_IMPL_SPICE_OP) && !defined(MULIB_SPICE_OP_H_IMPLEMENTED)
#define MULIB_SPICE_OP_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_op_solve(const mulib_spice_netlist_t *net, double *out, size_t n) {
    if (!mulib_spice_netlist_valid(net) || !out || n < net->nodes)
        return 0;
    size_t vs = 0;
    for (size_t i = 0; i < net->parts; i++)
        if (net->parts_data[i].kind == 'V')
            vs++;
    size_t total = net->nodes + vs;
    if (!total)
        return 0;
    mulib_spice_stamp_t s = {0};
    if (!mulib_spice_stamp_init(&s, total))
        return 0;
    size_t v = 0;
    int ok = 1;
    for (size_t i = 0; i < net->parts && ok; i++) {
        const mulib_spice_part_t *p = &net->parts_data[i];
        switch (p->kind) {
        case 'R':
            ok = mulib_spice_r_stamp(&s, p->a, p->b, p->value);
            break;
        case 'I':
            ok = mulib_spice_independent_current(&s, p->a, p->b, p->value);
            break;
        case 'V':
            ok = mulib_spice_independent_voltage(&s, p->a, p->b, net->nodes + v++, p->value);
            break;
        case 'C':
            break;
        default:
            ok = 0;
            break;
        }
    }
    double *sol = malloc(total * sizeof(double));
    if (!sol)
        ok = 0;
    if (ok)
        ok = mulib_num_dense_solve(total, s.a, s.b, sol, 1e-15);
    if (ok)
        memcpy(out, sol, net->nodes * sizeof(double));
    free(sol);
    mulib_spice_stamp_free(&s);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_OP */

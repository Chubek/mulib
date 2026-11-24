/** @file mulib-spice-dc.h
 * @brief Linear DC source sweep over a named independent voltage/current source.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_DC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_DC_H
#define MULIB_SPICE_DC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-op.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_dc_sweep(const mulib_spice_netlist_t *net, const char *source_name, double start,
                         double stop, size_t points, double *nodes_by_point, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_DC_H */
#if defined(MULIB_IMPL_SPICE_DC) && !defined(MULIB_SPICE_DC_H_IMPLEMENTED)
#define MULIB_SPICE_DC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_dc_sweep(const mulib_spice_netlist_t *net, const char *name, double begin,
                         double end, size_t steps, double *out, size_t cap) {
    if (!mulib_spice_netlist_valid(net) || !name || !out || steps < 2 || !isfinite(begin) ||
        !isfinite(end) || !net->nodes || steps > SIZE_MAX / net->nodes ||
        cap < steps * net->nodes || steps > SIZE_MAX / (net->nodes * sizeof(double)))
        return 0;
    mulib_spice_netlist_t *copy = malloc(sizeof *copy);
    if (!copy)
        return 0;
    *copy = *net;
    size_t src = copy->parts;
    for (size_t i = 0; i < copy->parts; i++)
        if (!strcmp(copy->parts_data[i].name, name) &&
            (copy->parts_data[i].kind == 'V' || copy->parts_data[i].kind == 'I')) {
            src = i;
            break;
        }
    if (src == copy->parts) {
        free(copy);
        return 0;
    }
    double *samples = malloc(steps * net->nodes * sizeof(double));
    if (!samples) {
        free(copy);
        return 0;
    }
    int ok = 1;
    for (size_t i = 0; i < steps; i++) {
        copy->parts_data[src].value = begin + (end - begin) * ((double)i / (steps - 1));
        if (!mulib_spice_op_solve(copy, samples + i * net->nodes, net->nodes)) {
            ok = 0;
            break;
        }
    }
    if (ok)
        memcpy(out, samples, steps * net->nodes * sizeof(double));
    free(samples);
    free(copy);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_DC */

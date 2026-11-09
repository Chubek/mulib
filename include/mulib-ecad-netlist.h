/** @file mulib-ecad-netlist.h
 * @brief Deterministic SPICE-card export from supported flat R/C/I/V schematic, retaining names and
 * double values, with bounded caller buffer.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_NETLIST in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_NETLIST_H
#define MULIB_ECAD_NETLIST_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-ecad-schematic.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_netlist_write(const mulib_ecad_schematic_t *s, char *buffer, size_t capacity,
                             size_t *written);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_NETLIST_H */
#if defined(MULIB_IMPL_ECAD_NETLIST) && !defined(MULIB_ECAD_NETLIST_H_IMPLEMENTED)
#define MULIB_ECAD_NETLIST_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_netlist_write(const mulib_ecad_schematic_t *s, char *buffer, size_t cap,
                             size_t *written) {
    if (!mulib_ecad_schematic_valid(s) || !buffer || !cap)
        return 0;
    size_t total = 6;
    for (size_t i = 0; i < s->count; i++) {
        const mulib_ecad_component_t *c = s->parts + i;
        int k = snprintf(NULL, 0, "%s %s %s %.17g\n", c->ref, c->a, c->b, c->value);
        if (k < 0 || (size_t)k > SIZE_MAX - total)
            return 0;
        total += (size_t)k;
    }
    if (total > cap)
        return 0;
    size_t used = 0;
    for (size_t i = 0; i < s->count; i++) {
        const mulib_ecad_component_t *c = s->parts + i;
        int need = snprintf(NULL, 0, "%s %s %s %.17g\n", c->ref, c->a, c->b, c->value);
        if (need < 0 || (size_t)need >= cap - used)
            return 0;
        used += (size_t)snprintf(buffer + used, cap - used, "%s %s %s %.17g\n", c->ref, c->a, c->b,
                                 c->value);
    }
    if (used + 6 > cap)
        return 0;
    memcpy(buffer + used, ".end\n", 6);
    used += 5;
    if (written)
        *written = used;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_NETLIST */

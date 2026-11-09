/** @file mulib-ecad-connect.h
 * @brief Name-based connectivity count for flat two-terminal schematic, with literal ground name 0.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_CONNECT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_CONNECT_H
#define MULIB_ECAD_CONNECT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-ecad-schematic.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_connect_count_nodes(const mulib_ecad_schematic_t *s, size_t *nodes,
                                   size_t *ground_connections);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_CONNECT_H */
#if defined(MULIB_IMPL_ECAD_CONNECT) && !defined(MULIB_ECAD_CONNECT_H_IMPLEMENTED)
#define MULIB_ECAD_CONNECT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_connect_count_nodes(const mulib_ecad_schematic_t *s, size_t *nodes, size_t *ground) {
    if (!mulib_ecad_schematic_valid(s) || !nodes || !ground)
        return 0;
    char names[512][32];
    size_t unique = 0, g = 0;
    for (size_t i = 0; i < s->count; i++) {
        const char *p[2] = {s->parts[i].a, s->parts[i].b};
        for (int k = 0; k < 2; k++) {
            if (!strcmp(p[k], "0")) {
                g++;
                continue;
            }
            size_t j = 0;
            for (; j < unique; j++)
                if (!strcmp(names[j], p[k]))
                    break;
            if (j == unique) {
                if (unique == 512)
                    return 0;
                strcpy(names[unique++], p[k]);
            }
        }
    }
    *nodes = unique;
    *ground = g;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_CONNECT */

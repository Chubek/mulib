/** @file mulib-ecad-erc.h
 * @brief Flat schematic ERC diagnostics: self-shorted components, single-use (dangling) named
 * terminals, and missing ground node.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_ERC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_ERC_H
#define MULIB_ECAD_ERC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-ecad-schematic.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_erc_result {
    size_t isolated, duplicate_shorts, unconnected_ground;
} mulib_ecad_erc_result_t;
int mulib_ecad_erc_run(const mulib_ecad_schematic_t *s, mulib_ecad_erc_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_ERC_H */
#if defined(MULIB_IMPL_ECAD_ERC) && !defined(MULIB_ECAD_ERC_H_IMPLEMENTED)
#define MULIB_ECAD_ERC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_erc_run(const mulib_ecad_schematic_t *s, mulib_ecad_erc_result_t *out) {
    if (!mulib_ecad_schematic_valid(s) || !out)
        return 0;
    mulib_ecad_erc_result_t r = {0};
    int grounded = 0;
    for (size_t i = 0; i < s->count; i++) {
        const mulib_ecad_component_t *c = &s->parts[i];
        if (!strcmp(c->a, c->b))
            r.duplicate_shorts++;
        if (!strcmp(c->a, "0") || !strcmp(c->b, "0"))
            grounded = 1;
        for (int pin = 0; pin < 2; pin++) {
            const char *name = pin ? c->b : c->a;
            if (!strcmp(name, "0"))
                continue;
            size_t uses = 0;
            for (size_t j = 0; j < s->count; j++)
                uses += !strcmp(name, s->parts[j].a) + !strcmp(name, s->parts[j].b);
            if (uses == 1)
                r.isolated++;
        }
    }
    r.unconnected_ground = grounded ? 0 : 1;
    *out = r;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_ERC */

/** @file mulib-ecad-schematic.h
 * @brief Bounded 256-component flat schematic for R/C/I/V 2-terminal devices, duplicate reference
 * detection and fixed 32-byte names.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_SCHEMATIC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_SCHEMATIC_H
#define MULIB_ECAD_SCHEMATIC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_component {
    char ref[32];
    char device;
    char a[32], b[32];
    double value;
} mulib_ecad_component_t;
typedef struct mulib_ecad_schematic {
    mulib_ecad_component_t parts[256];
    size_t count;
} mulib_ecad_schematic_t;
int mulib_ecad_schematic_add(mulib_ecad_schematic_t *s, const mulib_ecad_component_t *c);
int mulib_ecad_schematic_valid(const mulib_ecad_schematic_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_SCHEMATIC_H */
#if defined(MULIB_IMPL_ECAD_SCHEMATIC) && !defined(MULIB_ECAD_SCHEMATIC_H_IMPLEMENTED)
#define MULIB_ECAD_SCHEMATIC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_ecad_token_(const char *s, size_t cap);
int mulib_ecad_schematic_add(mulib_ecad_schematic_t *s, const mulib_ecad_component_t *c) {
    if (!s || !c || s->count >= 256 || !memchr(c->ref, 0, 32) || !memchr(c->a, 0, 32) ||
        !memchr(c->b, 0, 32) || !c->ref[0] || !c->a[0] || !c->b[0] || !isfinite(c->value) ||
        (c->device != 'R' && c->device != 'C' && c->device != 'I' && c->device != 'V'))
        return 0;
    if (!mulib_ecad_token_(c->ref, 32) || !mulib_ecad_token_(c->a, 32) ||
        !mulib_ecad_token_(c->b, 32) || c->ref[0] != c->device ||
        (c->device == 'R' && c->value <= 0) || (c->device == 'C' && c->value < 0))
        return 0;
    for (size_t i = 0; i < s->count; i++)
        if (!strcmp(s->parts[i].ref, c->ref))
            return 0;
    s->parts[s->count++] = *c;
    return 1;
}
static int mulib_ecad_token_(const char *s, size_t cap) {
    const char *end = memchr(s, 0, cap);
    if (!end || end == s)
        return 0;
    for (const char *p = s; p < end; p++)
        if ((unsigned char)*p <= 32 || *p == ';' || *p == '*')
            return 0;
    return 1;
}
int mulib_ecad_schematic_valid(const mulib_ecad_schematic_t *s) {
    if (!s || s->count > 256)
        return 0;
    for (size_t i = 0; i < s->count; i++) {
        const mulib_ecad_component_t *c = s->parts + i;
        if (!mulib_ecad_token_(c->ref, 32) || !mulib_ecad_token_(c->a, 32) ||
            !mulib_ecad_token_(c->b, 32) || !isfinite(c->value) || c->ref[0] != c->device)
            return 0;
        if (c->device != 'R' && c->device != 'C' && c->device != 'I' && c->device != 'V')
            return 0;
        if ((c->device == 'R' && c->value <= 0) || (c->device == 'C' && c->value < 0))
            return 0;
        for (size_t j = 0; j < i; j++)
            if (!strcmp(c->ref, s->parts[j].ref))
                return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_SCHEMATIC */

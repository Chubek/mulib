/** @file mulib-ecad-symbol.h
 * @brief Bounded symbol metadata with 32 typed pins and integer drawing-space anchor positions.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_SYMBOL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_SYMBOL_H
#define MULIB_ECAD_SYMBOL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_ecad_pin_kind {
    MULIB_ECAD_PASSIVE = 0,
    MULIB_ECAD_INPUT = 1,
    MULIB_ECAD_OUTPUT = 2,
    MULIB_ECAD_POWER_IN = 3,
    MULIB_ECAD_POWER_OUT = 4
} mulib_ecad_pin_kind_t;
typedef struct mulib_ecad_pin {
    char name[32];
    int x, y;
    mulib_ecad_pin_kind_t kind;
} mulib_ecad_pin_t;
typedef struct mulib_ecad_symbol {
    char name[48];
    size_t pins;
    mulib_ecad_pin_t pin[32];
} mulib_ecad_symbol_t;
int mulib_ecad_symbol_pin(mulib_ecad_symbol_t *s, const char *name, int x, int y,
                          mulib_ecad_pin_kind_t kind);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_SYMBOL_H */
#if defined(MULIB_IMPL_ECAD_SYMBOL) && !defined(MULIB_ECAD_SYMBOL_H_IMPLEMENTED)
#define MULIB_ECAD_SYMBOL_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_symbol_pin(mulib_ecad_symbol_t *s, const char *name, int x, int y,
                          mulib_ecad_pin_kind_t kind) {
    if (!s || !name || !name[0] || strlen(name) >= 32 || s->pins >= 32 || kind < 0 || kind > 4)
        return 0;
    for (size_t i = 0; i < s->pins; i++)
        if (!strcmp(s->pin[i].name, name))
            return 0;
    mulib_ecad_pin_t *p = &s->pin[s->pins++];
    strcpy(p->name, name);
    p->x = x;
    p->y = y;
    p->kind = kind;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_SYMBOL */

/** @file mulib-pcb-bom.h
 * @brief Deterministic fixed-size BOM aggregator by manufacturer and exact part number, with
 * checked size_t quantity accumulation.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_BOM in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_BOM_H
#define MULIB_PCB_BOM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_bom_line {
    char mfr[48], part_number[48];
    size_t quantity;
} mulib_pcb_bom_line_t;
typedef struct mulib_pcb_bom {
    mulib_pcb_bom_line_t lines[256];
    size_t count;
} mulib_pcb_bom_t;
int mulib_pcb_bom_add(mulib_pcb_bom_t *b, const char *manufacturer, const char *part_number,
                      size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_BOM_H */
#if defined(MULIB_IMPL_PCB_BOM) && !defined(MULIB_PCB_BOM_H_IMPLEMENTED)
#define MULIB_PCB_BOM_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_bom_add(mulib_pcb_bom_t *b, const char *m, const char *part, size_t q) {
    if (!b || b->count > 256 || !m || !part || !m[0] || !part[0] || strlen(m) >= 48 ||
        strlen(part) >= 48 || !q)
        return 0;
    for (size_t i = 0; i < b->count; i++)
        if (!strcmp(m, b->lines[i].mfr) && !strcmp(part, b->lines[i].part_number)) {
            if (q > SIZE_MAX - b->lines[i].quantity)
                return 0;
            b->lines[i].quantity += q;
            return 1;
        }
    if (b->count >= 256)
        return 0;
    mulib_pcb_bom_line_t *l = b->lines + b->count++;
    strcpy(l->mfr, m);
    strcpy(l->part_number, part);
    l->quantity = q;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_BOM */

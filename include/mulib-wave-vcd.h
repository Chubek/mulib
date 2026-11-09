/** @file mulib-wave-vcd.h
 * @brief VCD writer for one scalar four-state signal with a fixed identifier (!) and 1ns timescale.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_VCD in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_VCD_H
#define MULIB_WAVE_VCD_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_wave_vcd {
    FILE *stream;
    uint64_t time;
    int started;
} mulib_wave_vcd_t;
int mulib_wave_vcd_begin(mulib_wave_vcd_t *v, FILE *fp, const char *signal_name);
int mulib_wave_vcd_at(mulib_wave_vcd_t *v, uint64_t time, mulib_dsim_logic4_t value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_VCD_H */
#if defined(MULIB_IMPL_WAVE_VCD) && !defined(MULIB_WAVE_VCD_H_IMPLEMENTED)
#define MULIB_WAVE_VCD_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_vcd_begin(mulib_wave_vcd_t *v, FILE *fp, const char *name) {
    if (!v || !fp || !name || !name[0] || strpbrk(name, " \t\n\r$"))
        return 0;
    if (fprintf(fp,
                "$timescale 1ns $end\n$scope module amb3r $end\n$var wire 1 ! %s $end\n$upscope "
                "$end\n$enddefinitions $end\n#0\nx!\n",
                name) < 0)
        return 0;
    v->stream = fp;
    v->time = 0;
    v->started = 1;
    return 1;
}
int mulib_wave_vcd_at(mulib_wave_vcd_t *v, uint64_t t, mulib_dsim_logic4_t value) {
    if (!v || !v->started || !v->stream || t < v->time || value < MULIB_DSIM_0 ||
        value > MULIB_DSIM_Z)
        return 0;
    if (t != v->time && fprintf(v->stream, "#%llu\n", (unsigned long long)t) < 0)
        return 0;
    if (fprintf(v->stream, "%c!\n", mulib_dsim_logic4_char(value)) < 0)
        return 0;
    v->time = t;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_WAVE_VCD */

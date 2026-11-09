/** @file mulib-fpga-lut.h
 * @brief Pure combinational lookup-table element evaluator and truth-table mapping for up to six
 * inputs.
 * @details C11 single-header implementation; define MULIB_IMPL_FPGA_LUT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FPGA_LUT_H
#define MULIB_FPGA_LUT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-synth-truthtable.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fpga_lut {
    uint8_t inputs;
    uint64_t configuration;
} mulib_fpga_lut_t;
int mulib_fpga_lut_eval(mulib_fpga_lut_t lut, uint8_t input_bits, int *out);
int mulib_fpga_lut_from_truth(mulib_synth_truth_t truth, mulib_fpga_lut_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FPGA_LUT_H */
#if defined(MULIB_IMPL_FPGA_LUT) && !defined(MULIB_FPGA_LUT_H_IMPLEMENTED)
#define MULIB_FPGA_LUT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_lut_eval(mulib_fpga_lut_t l, uint8_t x, int *out) {
    if (!out || l.inputs > 6 || x >= (1u << l.inputs))
        return 0;
    *out = (int)((l.configuration >> x) & 1);
    return 1;
}
int mulib_fpga_lut_from_truth(mulib_synth_truth_t t, mulib_fpga_lut_t *out) {
    if (!out || t.variables > 6)
        return 0;
    out->inputs = t.variables;
    out->configuration = t.bits;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FPGA_LUT */

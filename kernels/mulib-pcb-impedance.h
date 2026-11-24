/** @file mulib-pcb-impedance.h
 * @brief PCB convenience API for approximate zero-thickness quasi-static microstrip impedance.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_IMPEDANCE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_IMPEDANCE_H
#define MULIB_PCB_IMPEDANCE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-rf-microstrip.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_microstrip_impedance(double copper_width_mm, double dielectric_height_mm,
                                   double epsilon_r, double *ohms);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_IMPEDANCE_H */
#if defined(MULIB_IMPL_PCB_IMPEDANCE) && !defined(MULIB_PCB_IMPEDANCE_H_IMPLEMENTED)
#define MULIB_PCB_IMPEDANCE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_microstrip_impedance(double w, double h, double er, double *ohms) {
    return mulib_rf_microstrip_z0(w, h, er, ohms);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_IMPEDANCE */

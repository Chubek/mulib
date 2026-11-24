/* MuLIB: fpga-constraints. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_CONSTRAINTS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_CONSTRAINTS_H
#define MULIB_FPGA_CONSTRAINTS_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t signal;size_t pin;unsigned bank;} mulib_fpga_pin_assignment;
int mulib_fpga_constraints_pins(const mulib_fpga_pin_assignment *a,size_t n,size_t pin_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_CONSTRAINTS) && !defined(MULIB_FPGA_CONSTRAINTS_H_IMPLEMENTED)
#define MULIB_FPGA_CONSTRAINTS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_constraints_pins(const mulib_fpga_pin_assignment *a,size_t n,size_t pin_count){if(!a&&n)return 0;for(size_t i=0;i<n;i++){if(!a[i].signal||a[i].pin>=pin_count)return 0;for(size_t j=0;j<i;j++)if(a[j].pin==a[i].pin||a[j].signal==a[i].signal)return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif

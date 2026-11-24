/** @file mulib-kernel-types.h
 * Scalar type and packed vector byte-size helpers.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: GPU alignment is backend dependent; no matrix/image types.
 */
#ifndef MULIB_KERNEL_TYPES_H
#define MULIB_KERNEL_TYPES_H
#include "mulib-kernel-ir.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline unsigned mulib_kernel_types_size(mulib_kernel_type t){return t==MULIB_KERNEL_BOOL||t==MULIB_KERNEL_F32||t==MULIB_KERNEL_I32||t==MULIB_KERNEL_U32?4:0;}
static inline bool mulib_kernel_types_match(mulib_kernel_type a,mulib_kernel_type b){return mulib_kernel_types_size(a)&&a==b;}
static inline bool mulib_kernel_types_vector_bytes(mulib_kernel_type t,unsigned lanes,size_t *out){unsigned n=mulib_kernel_types_size(t);if(!out||!n||lanes<1||lanes>4)return false;*out=n*lanes;return true;}
#ifdef __cplusplus
}
#endif
#endif

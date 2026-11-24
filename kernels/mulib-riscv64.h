/** @file mulib-riscv64.h
 * @brief Owned RV64 profile implementing the MPU contract.
 * Define MULIB_IMPL_RISCV64 and MPU's dependency closure. Encodings and
 * register aliases come from the embedded riscv64 ArchML profile.
 */
#ifndef MULIB_RISCV64_H
#define MULIB_RISCV64_H
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mpu_t mulib_riscv64_t;
/** Zero initialize first. Return 1 on success, 0 on failure/live context. */
int mulib_riscv64_init (mulib_riscv64_t *);
/** Diagnostic variant with the same Boolean return convention. */
int mulib_riscv64_init_ex (mulib_riscv64_t *, mulib_archml_error_t *);
/** Release/reset owned profile; NULL/repeated free permitted. */
void mulib_riscv64_free (mulib_riscv64_t *);
/** MPU contract aliases. Encoders consume numeric register IDs/immediates,
 * e.g. add {10,11,12}; signed and scattered branch operands are byte offsets.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_riscv64_document mulib_mpu_document
#define mulib_riscv64_find mulib_mpu_find
#define mulib_riscv64_register_info mulib_mpu_register_info
#define mulib_riscv64_enabled mulib_mpu_enabled
#define mulib_riscv64_encode mulib_mpu_encode
#define mulib_riscv64_decode mulib_mpu_decode
#define mulib_riscv64_emit mulib_mpu_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RISCV64) && !defined(MULIB_RISCV64_IMPLEMENTED)
#define MULIB_RISCV64_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_riscv64_init (mulib_riscv64_t *c) { return mulib_riscv64_init_ex (c, NULL); }
int mulib_riscv64_init_ex (mulib_riscv64_t *c, mulib_archml_error_t *e)
{ return mulib_mpu_init (c, "riscv64", e) == MULIB_ARCHML_OK; }
void mulib_riscv64_free (mulib_riscv64_t *c) { mulib_mpu_free (c); }
#ifdef __cplusplus
}
#endif
#endif

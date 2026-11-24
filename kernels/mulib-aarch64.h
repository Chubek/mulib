/** @file mulib-aarch64.h
 * @brief Owned AArch64 profile implementing the MPU contract.
 * Define MULIB_IMPL_AARCH64 and MPU's dependency closure. Concrete forms and
 * aliases come from the embedded arm64 ArchML profile; all hosts supported.
 */
#ifndef MULIB_AARCH64_H
#define MULIB_AARCH64_H
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mpu_t mulib_aarch64_t;
/** Zero initialize first. Return 1 on success, 0 on failure/live context. */
int mulib_aarch64_init (mulib_aarch64_t *);
/** Diagnostic variant with the same Boolean return convention. */
int mulib_aarch64_init_ex (mulib_aarch64_t *, mulib_archml_error_t *);
/** Release/reset owned profile; NULL/repeated free permitted. */
void mulib_aarch64_free (mulib_aarch64_t *);
/** MPU contract aliases: codecs/emit return MULIB_ARCHML_OK (1), not Boolean.
 * Operands are numeric profile values, e.g. add64 {0,1,2}; branches use byte
 * displacements. See mulib-mpu.h and mulib-unit.h for full signatures. */
#define mulib_aarch64_document mulib_mpu_document
#define mulib_aarch64_find mulib_mpu_find
#define mulib_aarch64_register_info mulib_mpu_register_info
#define mulib_aarch64_enabled mulib_mpu_enabled
#define mulib_aarch64_encode mulib_mpu_encode
#define mulib_aarch64_decode mulib_mpu_decode
#define mulib_aarch64_emit mulib_mpu_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AARCH64) && !defined(MULIB_AARCH64_IMPLEMENTED)
#define MULIB_AARCH64_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_aarch64_init (mulib_aarch64_t *c) { return mulib_aarch64_init_ex (c, NULL); }
int mulib_aarch64_init_ex (mulib_aarch64_t *c, mulib_archml_error_t *e)
{ return mulib_mpu_init (c, "aarch64", e) == MULIB_ARCHML_OK; }
void mulib_aarch64_free (mulib_aarch64_t *c) { mulib_mpu_free (c); }
#ifdef __cplusplus
}
#endif
#endif

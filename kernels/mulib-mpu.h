/** @file mulib-mpu.h
 * @brief Extensible microprocessor profile, query and code emission contract.
 * Define MULIB_IMPL_MPU and UNIT's dependency closure. An MPU is an owned
 * ArchML profile; this API emits code and metadata, not host CPU instructions.
 */
#ifndef MULIB_MPU_H
#define MULIB_MPU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_mpu_t;
/** Initialize amd64, aarch64 or riscv64. Zero initialize first; success is 1. */
mulib_archml_result mulib_mpu_init (mulib_mpu_t *, const char *, mulib_archml_error_t *);
/** Extend the contract with an owned, caller-supplied ArchML MPU profile. */
mulib_archml_result mulib_mpu_parse (mulib_mpu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Free and reset; NULL/repeated free permitted. */
void mulib_mpu_free (mulib_mpu_t *);
/** Shared queries and transactional codecs; see mulib-unit.h for signatures.
 * Concrete form names and numeric operand order come from the loaded profile.
 * Future MPUs implement the same contract by loading their own ArchML data. */
#define mulib_mpu_document mulib_unit_document
#define mulib_mpu_find mulib_unit_find
#define mulib_mpu_register_info mulib_unit_register_info
#define mulib_mpu_enabled mulib_unit_enabled
#define mulib_mpu_encode mulib_unit_encode
#define mulib_mpu_decode mulib_unit_decode
#define mulib_mpu_emit mulib_unit_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MPU) && !defined(MULIB_MPU_IMPLEMENTED)
#define MULIB_MPU_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_archml_result mulib_mpu_init (mulib_mpu_t *c, const char *name, mulib_archml_error_t *e)
{ return mulib_unit_init (c, MULIB_UNIT_MPU, name, e); }
mulib_archml_result mulib_mpu_parse (mulib_mpu_t *c, const char *s, size_t n,
    const mulib_archml_options_t *o, mulib_archml_error_t *e)
{ return mulib_unit_parse (c, MULIB_UNIT_MPU, s, n, o, e); }
void mulib_mpu_free (mulib_mpu_t *c) { mulib_unit_free (c); }
#ifdef __cplusplus
}
#endif
#endif

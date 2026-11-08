/** @file mulib-xtensa.h
 * @brief Owned little-endian Xtensa core 24-bit MCU implementation.
 * Define MULIB_IMPL_XTENSA and MCU's dependency closure. Only the embedded
 * profile's integer forms and visible a0..a15 registers are supported.
 */
#ifndef MULIB_XTENSA_H
#define MULIB_XTENSA_H
#include "mulib-mcu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mcu_t mulib_xtensa_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_xtensa_init (mulib_xtensa_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_xtensa_init_ex (mulib_xtensa_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_xtensa_free (mulib_xtensa_t *);
/** MCU contract aliases; numeric operands follow the profile.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_xtensa_document mulib_mcu_document
#define mulib_xtensa_find mulib_mcu_find
#define mulib_xtensa_register_info mulib_mcu_register_info
#define mulib_xtensa_enabled mulib_mcu_enabled
#define mulib_xtensa_encode mulib_mcu_encode
#define mulib_xtensa_decode mulib_mcu_decode
#define mulib_xtensa_emit mulib_mcu_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XTENSA) && !defined(MULIB_XTENSA_IMPLEMENTED)
#define MULIB_XTENSA_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_xtensa_init (mulib_xtensa_t *c) { return mulib_xtensa_init_ex (c, NULL); }
int mulib_xtensa_init_ex (mulib_xtensa_t *c, mulib_archml_error_t *e)
{ return mulib_mcu_init (c, "xtensa", e) == MULIB_ARCHML_OK; }
void mulib_xtensa_free (mulib_xtensa_t *c) { mulib_mcu_free (c); }
#ifdef __cplusplus
}
#endif
#endif

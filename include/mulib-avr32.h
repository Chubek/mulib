/** @file mulib-avr32.h
 * @brief Owned big-endian AVR32A selected-form MCU implementation.
 * Define MULIB_IMPL_AVR32 and MCU's dependency closure. This is 32-bit AVR32A,
 * not 8-bit AVR. Profile covers selected 16-bit register/immediate forms.
 */
#ifndef MULIB_AVR32_H
#define MULIB_AVR32_H
#include "mulib-mcu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mcu_t mulib_avr32_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_avr32_init (mulib_avr32_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_avr32_init_ex (mulib_avr32_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_avr32_free (mulib_avr32_t *);
/** MCU contract aliases; numeric operands follow the profile, e.g. add {2,3}.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_avr32_document mulib_mcu_document
#define mulib_avr32_find mulib_mcu_find
#define mulib_avr32_register_info mulib_mcu_register_info
#define mulib_avr32_enabled mulib_mcu_enabled
#define mulib_avr32_encode mulib_mcu_encode
#define mulib_avr32_decode mulib_mcu_decode
#define mulib_avr32_emit mulib_mcu_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AVR32) && !defined(MULIB_AVR32_IMPLEMENTED)
#define MULIB_AVR32_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_avr32_init (mulib_avr32_t *c) { return mulib_avr32_init_ex (c, NULL); }
int mulib_avr32_init_ex (mulib_avr32_t *c, mulib_archml_error_t *e)
{ return mulib_mcu_init (c, "avr32", e) == MULIB_ARCHML_OK; }
void mulib_avr32_free (mulib_avr32_t *c) { mulib_mcu_free (c); }
#ifdef __cplusplus
}
#endif
#endif

/** @file mulib-mcu.h
 * @brief Extensible microcontroller architecture and encoding contract.
 * Define MULIB_IMPL_MCU and UNIT's dependency closure. Device peripherals,
 * flash programming, interrupts and board initialization are outside this API.
 */
#ifndef MULIB_MCU_H
#define MULIB_MCU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_mcu_t;
/** Initialize avr32 or xtensa. Zero initialize first; success is 1. */
mulib_archml_result mulib_mcu_init (mulib_mcu_t *, const char *, mulib_archml_error_t *);
/** Load a future/custom MCU profile with explicit endian and register data. */
mulib_archml_result mulib_mcu_parse (mulib_mcu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release all owned profile data and reset. NULL/repeated free permitted. */
void mulib_mcu_free (mulib_mcu_t *);
/** Shared queries/codecs inherit mulib-unit.h ownership and result contracts. */
#define mulib_mcu_document mulib_unit_document
#define mulib_mcu_find mulib_unit_find
#define mulib_mcu_register_info mulib_unit_register_info
#define mulib_mcu_enabled mulib_unit_enabled
#define mulib_mcu_encode mulib_unit_encode
#define mulib_mcu_decode mulib_unit_decode
#define mulib_mcu_emit mulib_unit_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MCU) && !defined(MULIB_MCU_IMPLEMENTED)
#define MULIB_MCU_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_archml_result mulib_mcu_init (mulib_mcu_t *c, const char *name, mulib_archml_error_t *e)
{ return mulib_unit_init (c, MULIB_UNIT_MCU, name, e); }
mulib_archml_result mulib_mcu_parse (mulib_mcu_t *c, const char *s, size_t n,
    const mulib_archml_options_t *o, mulib_archml_error_t *e)
{ return mulib_unit_parse (c, MULIB_UNIT_MCU, s, n, o, e); }
void mulib_mcu_free (mulib_mcu_t *c) { mulib_unit_free (c); }
#ifdef __cplusplus
}
#endif
#endif

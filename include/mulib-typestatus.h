/** @file mulib-typestatus.h
 * @brief Stable error codes and human-readable diagnostics for publishing modules.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPESTATUS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typestatus.md for scope and limitations.
 */
#ifndef MULIB_TYPESTATUS_H
#define MULIB_TYPESTATUS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_typestatus_code { mulib_typestatus_OK=0,mulib_typestatus_INVALID=-1,mulib_typestatus_NOMEM=-2,mulib_typestatus_LIMIT=-3,mulib_typestatus_UNSUPPORTED=-4,mulib_typestatus_FORMAT=-5 } mulib_typestatus_code_t;
const char *mulib_typestatus_name(mulib_typestatus_code_t status);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPESTATUS_H */
#if defined(MULIB_IMPL_TYPESTATUS) && !defined(MULIB_TYPESTATUS_H_IMPLEMENTED)
#define MULIB_TYPESTATUS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
const char *mulib_typestatus_name(mulib_typestatus_code_t e){switch(e){case mulib_typestatus_OK:return "ok";case mulib_typestatus_INVALID:return "invalid argument";case mulib_typestatus_NOMEM:return "allocation failure";case mulib_typestatus_LIMIT:return "limit exceeded";case mulib_typestatus_UNSUPPORTED:return "unsupported feature";case mulib_typestatus_FORMAT:return "invalid format";default:return "unknown";}}
#ifdef __cplusplus
}
#endif
#endif

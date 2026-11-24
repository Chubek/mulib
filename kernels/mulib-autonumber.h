/** @file mulib-autonumber.h
 * @brief Monotonic hierarchical document numbering.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_AUTONUMBER once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-autonumber.md for scope and limitations.
 */
#ifndef MULIB_AUTONUMBER_H
#define MULIB_AUTONUMBER_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_autonumber_counter { unsigned level[8]; } mulib_autonumber_counter_t;
int mulib_autonumber_next(mulib_autonumber_counter_t *counter,unsigned level,unsigned *value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_AUTONUMBER_H */
#if defined(MULIB_IMPL_AUTONUMBER) && !defined(MULIB_AUTONUMBER_H_IMPLEMENTED)
#define MULIB_AUTONUMBER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_autonumber_next(mulib_autonumber_counter_t *c,unsigned level,unsigned *out){if(!c||!out||level>=8||c->level[level]==UINT_MAX)return 0;c->level[level]++;for(unsigned i=level+1;i<8;i++)c->level[i]=0;*out=c->level[level];return 1;}
#ifdef __cplusplus
}
#endif
#endif

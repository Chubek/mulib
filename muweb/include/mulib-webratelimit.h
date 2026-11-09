/** @file mulib-webratelimit.h
 * @brief Deterministic caller-clocked minimum-interval request scheduler.
 * C11 single-header implementation. Define MULIB_IMPL_WEBRATELIMIT in one translation unit.
 */
#ifndef MULIB_WEBRATELIMIT_H
#define MULIB_WEBRATELIMIT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_webratelimit { uint64_t interval_ms,next_ms; } mulib_webratelimit_t;
void mulib_webratelimit_init(mulib_webratelimit_t *r,uint64_t delay_ms);
/* 1 if permitted and slot consumed, 0 otherwise. */
int mulib_webratelimit_take(mulib_webratelimit_t *r,uint64_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBRATELIMIT) && !defined(MULIB_IMPL_WEBRATELIMIT_IMPLEMENTED)
#define MULIB_IMPL_WEBRATELIMIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

void mulib_webratelimit_init(mulib_webratelimit_t *r,uint64_t ms){if(r){r->interval_ms=ms;r->next_ms=0;}}
int mulib_webratelimit_take(mulib_webratelimit_t *r,uint64_t now){if(!r||now<r->next_ms||UINT64_MAX-now<r->interval_ms)return 0;r->next_ms=now+r->interval_ms;return 1;}

#ifdef __cplusplus
}
#endif
#endif

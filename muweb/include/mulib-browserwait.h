/** @file mulib-browserwait.h
 * @brief Caller-clocked deterministic browser condition timeout helper.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERWAIT in one translation unit.
 */
#ifndef MULIB_BROWSERWAIT_H
#define MULIB_BROWSERWAIT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_browserwait {uint64_t deadline_ms,interval_ms,next_poll_ms;} mulib_browserwait_t;
int mulib_browserwait_init(mulib_browserwait_t *w,uint64_t now,uint64_t timeout,uint64_t interval);
/* 1 poll, 0 not yet, -1 timed out. */
int mulib_browserwait_poll(mulib_browserwait_t *w,uint64_t now);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERWAIT) && !defined(MULIB_IMPL_BROWSERWAIT_IMPLEMENTED)
#define MULIB_IMPL_BROWSERWAIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_browserwait_init(mulib_browserwait_t *w,uint64_t now,uint64_t timeout,uint64_t interval){if(!w||UINT64_MAX-now<timeout||!interval)return 0;w->deadline_ms=now+timeout;w->interval_ms=interval;w->next_poll_ms=now;return 1;}
int mulib_browserwait_poll(mulib_browserwait_t *w,uint64_t now){if(!w)return -1;if(now>w->deadline_ms)return -1;if(now<w->next_poll_ms)return 0;w->next_poll_ms=UINT64_MAX-now<w->interval_ms?UINT64_MAX:now+w->interval_ms;return 1;}

#ifdef __cplusplus
}
#endif
#endif

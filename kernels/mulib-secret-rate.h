/** @file mulib-secret-rate.h
 * @brief Bounded exponential authentication backoff using trusted monotonic time.
 * Define MULIB_IMPL_SECRET_RATE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_RATE_H
#define MULIB_SECRET_RATE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint64_t next, last;
    uint32_t failures;
} mulib_srate_state;

/* Zero-initialize. All times/delays use one trusted monotonic clock domain.
 * The broker must apply and persist separate account/client limits, serialize
 * admission and outcome, and reconcile clock epochs on restart. check does
 * not reserve an attempt; last tracks only recorded outcomes. Pass wait
 * outside state storage. wait is set on OK/DENIED, unchanged on error. A
 * terminal block at the clock ceiling returns DENIED with wait = 1. */
int mulib_srate_check(const mulib_srate_state *s, uint64_t now, uint64_t *wait);
/* Call only for an admitted failed attempt. Outstanding attempts cannot
 * shorten the existing block. An unrepresentable deadline saturates to
 * UINT64_MAX, a terminal block until trusted broker recovery. */
int mulib_srate_failure(mulib_srate_state *s, uint64_t now, uint32_t base,
                        uint32_t maximum);
/* Reset only after successful authentication; never to clear a block. */
int mulib_srate_success(mulib_srate_state *s, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_RATE) && !defined(MULIB_SECRET_RATE_IMPLEMENTED)
#define MULIB_SECRET_RATE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_srate_valid_(const mulib_srate_state *s, uint64_t now)
{
    if (!s)
        return MULIB_SC_INVALID;
    if (now < s->last || (s->failures && s->next <= s->last && s->next != UINT64_MAX) ||
        (!s->failures && s->next != s->last))
        return MULIB_SC_STATE;
    return MULIB_SC_OK;
}

int mulib_srate_check(const mulib_srate_state *s, uint64_t now, uint64_t *wait)
{
    int rc;
    if (!wait || mulib_sc_overlap(s, sizeof *s, wait, sizeof *wait))
        return MULIB_SC_INVALID;
    rc = mulib_srate_valid_(s, now);
    if (rc)
        return rc;
    *wait = now < s->next ? s->next - now : 0;
    if (!*wait && s->failures && s->next == UINT64_MAX)
        *wait = 1; /* Terminal clock sentinel; no positive interval is representable. */
    return *wait ? MULIB_SC_DENIED : MULIB_SC_OK;
}

int mulib_srate_failure(mulib_srate_state *s, uint64_t now, uint32_t base, uint32_t maximum)
{
    uint64_t delay = base, next;
    int rc;
    if (!base || maximum < base)
        return MULIB_SC_INVALID;
    rc = mulib_srate_valid_(s, now);
    if (rc)
        return rc;

    /* At most 32 doublings, even when the failure counter is saturated. */
    for (uint32_t i = 0; i < s->failures && delay < maximum; ++i) {
        delay *= 2u;
        if (delay > maximum)
            delay = maximum;
    }
    next = now > UINT64_MAX - delay ? UINT64_MAX : now + delay;
    if (s->failures < UINT32_MAX)
        ++s->failures;
    if (s->next < next)
        s->next = next;
    s->last = now;
    return MULIB_SC_OK;
}

int mulib_srate_success(mulib_srate_state *s, uint64_t now)
{
    uint64_t wait;
    int rc = mulib_srate_check(s, now, &wait);
    if (rc)
        return rc;
    s->failures = 0;
    s->last = now;
    s->next = now;
    return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif

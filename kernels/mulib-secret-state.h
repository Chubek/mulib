/** @file mulib-secret-state.h
 * @brief Monotonic counter reservation and optimistic persistent transition contract.
 * Define MULIB_IMPL_SECRET_STATE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_STATE_H
#define MULIB_SECRET_STATE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t revision,counter; unsigned char id[16]; } mulib_ss_counter;
/* CAS must compare the entire logical old state (ID, revision, counter) under
 * an interprocess transaction and return OK only after durable commit.
 * Return CONFLICT for a mismatching snapshot and UNCERTAIN if the outcome is
 * unknown; other errors must imply that nothing committed. No state argument
 * may be retained or changed by the callback. Callers serialize access to s.
 *
 * CONFLICT or UNCERTAIN sets s->revision to zero. Do not reuse this snapshot
 * until it has been replaced by fresh authenticated durable state; an unknown
 * durable outcome may require journal reconciliation first. A snapshot with
 * zero revision returns STATE without invoking CAS. reserved must not overlap
 * s, and is valid only on OK (unchanged otherwise). */
typedef int (*mulib_ss_cas)(void *, const mulib_ss_counter *, const mulib_ss_counter *);
int mulib_ss_reserve(mulib_ss_counter *s, mulib_ss_cas cas, void *ctx, uint64_t *reserved);
int mulib_ss_accept(mulib_ss_counter *s, uint64_t received, mulib_ss_cas cas, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_STATE) && !defined(MULIB_SECRET_STATE_IMPLEMENTED)
#define MULIB_SECRET_STATE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_ss_commit_(mulib_ss_counter *s, uint64_t next,
                            mulib_ss_cas cas, void *ctx)
{
    const mulib_ss_counter old = *s;
    mulib_ss_counter t = old;
    ++t.revision;
    t.counter = next;
    int r = cas(ctx, &old, &t);
    if (r > MULIB_SC_OK || r < MULIB_SC_NOTFOUND)
        r = MULIB_SC_UNCERTAIN;
    if (r == MULIB_SC_OK)
        *s = t;
    else if (r == MULIB_SC_CONFLICT || r == MULIB_SC_UNCERTAIN)
        s->revision = 0;
    return r;
}

int mulib_ss_reserve(mulib_ss_counter *s, mulib_ss_cas cas, void *ctx, uint64_t *reserved)
{
    if (!s || !cas || !reserved)
        return MULIB_SC_INVALID;
    if (mulib_sc_overlap(s, sizeof *s, reserved, sizeof *reserved))
        return MULIB_SC_INVALID;
    if (!s->revision)
        return MULIB_SC_STATE;
    if (s->revision == UINT64_MAX || s->counter == UINT64_MAX)
        return MULIB_SC_LIMIT;
    uint64_t current = s->counter;
    int r = mulib_ss_commit_(s, current + 1, cas, ctx);
    if (r == MULIB_SC_OK)
        *reserved = current;
    return r;
}

int mulib_ss_accept(mulib_ss_counter *s, uint64_t received, mulib_ss_cas cas, void *ctx)
{
    if (!s || !cas)
        return MULIB_SC_INVALID;
    if (!s->revision)
        return MULIB_SC_STATE;
    if (received < s->counter)
        return MULIB_SC_AUTH;
    if (received == UINT64_MAX || s->revision == UINT64_MAX)
        return MULIB_SC_LIMIT;
    return mulib_ss_commit_(s, received + 1, cas, ctx);
}
#ifdef __cplusplus
}
#endif
#endif

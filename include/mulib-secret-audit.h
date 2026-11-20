/** @file mulib-secret-audit.h
 * @brief HMAC-chained fixed-format audit records, with externally anchored chain head.
 * Define MULIB_IMPL_SECRET_AUDIT once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_AUDIT_H
#define MULIB_SECRET_AUDIT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SAUD_SIZE 120u
#define MULIB_SAUD_MAX_KEY 1024u
typedef struct {
    uint64_t sequence;
    unsigned char head[32];
} mulib_saud_state;
typedef struct {
    uint64_t time;
    unsigned char object[16], subject_id[16];
    uint32_t operation;
    int32_t status;
} mulib_saud_event;

/* Use a dedicated, vault/domain-separated binary HMAC key (32-1024 bytes),
 * not a password. Log only pseudonymous identifiers and numeric codes;
 * never raw request, response or secret, even on errors. Time, operation
 * and status are broker-defined metadata, not trusted authorization inputs.
 * Start with a zero state. Stage outputs, commit transactionally, and
 * publish/anchor through the broker. Outputs and input state are unchanged
 * on failure. No overlaps; old == next is allowed only for in-memory use. */
int mulib_saud_append(const mulib_scr_provider *p, mulib_sc_view key,
                      const mulib_saud_state *old, const mulib_saud_event *event,
                      unsigned char wire[MULIB_SAUD_SIZE], mulib_saud_state *next);
int mulib_saud_verify(const mulib_scr_provider *p, mulib_sc_view key,
                      mulib_saud_state *state, const unsigned char wire[MULIB_SAUD_SIZE]);
/* Prefer the length-aware form for untrusted reads. State advances only on
 * authentication success; accepting a prefix alone cannot detect truncation. */
int mulib_saud_verify_record(const mulib_scr_provider *p, mulib_sc_view key,
                             mulib_saud_state *state, mulib_sc_view wire);
/* Test exact equality against an independently authenticated, current external
 * anchor after replay. This does not authenticate or persist the anchor. */
int mulib_saud_check_anchor(const mulib_saud_state *state,
                            const mulib_saud_state *trusted_anchor);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_AUDIT) && !defined(MULIB_SECRET_AUDIT_IMPLEMENTED)
#define MULIB_SECRET_AUDIT_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void mulib_saud_u32_(unsigned char *p, uint32_t x)
{
    for (unsigned i = 0; i < 4; ++i)
        p[3 - i] = (unsigned char)(x >> (8 * i));
}

static int mulib_saud_state_valid_(const mulib_saud_state *s)
{
    const unsigned char genesis[32] = {0};
    return s && (s->sequence || mulib_sc_equal(s->head, genesis, sizeof genesis));
}

static int mulib_saud_key_valid_(mulib_sc_view key)
{
    return key.ptr && key.len >= 32 && key.len <= MULIB_SAUD_MAX_KEY;
}

int mulib_saud_append(const mulib_scr_provider *p, mulib_sc_view key,
                      const mulib_saud_state *old, const mulib_saud_event *event,
                      unsigned char wire[MULIB_SAUD_SIZE], mulib_saud_state *next)
{
    unsigned char tmp[MULIB_SAUD_SIZE];
    mulib_sc_view body = {tmp, 88};
    int rc;
    if (!mulib_saud_state_valid_(old) || !event || !wire || !next ||
        !mulib_saud_key_valid_(key) ||
        mulib_sc_overlap(wire, MULIB_SAUD_SIZE, next, sizeof *next) ||
        mulib_sc_overlap(wire, MULIB_SAUD_SIZE, old, sizeof *old) ||
        mulib_sc_overlap(wire, MULIB_SAUD_SIZE, event, sizeof *event) ||
        mulib_sc_overlap(wire, MULIB_SAUD_SIZE, key.ptr, key.len) ||
        mulib_sc_overlap(next, sizeof *next, event, sizeof *event) ||
        mulib_sc_overlap(next, sizeof *next, key.ptr, key.len) ||
        (old != next && mulib_sc_overlap(next, sizeof *next, old, sizeof *old)))
        return MULIB_SC_INVALID;
    if (old->sequence == UINT64_MAX)
        return MULIB_SC_LIMIT;

    mulib_sc_put64(tmp, old->sequence + 1u);
    mulib_sc_put64(tmp + 8, event->time);
    memcpy(tmp + 16, event->object, sizeof event->object);
    memcpy(tmp + 32, event->subject_id, sizeof event->subject_id);
    mulib_saud_u32_(tmp + 48, event->operation);
    mulib_saud_u32_(tmp + 52, (uint32_t)event->status);
    memcpy(tmp + 56, old->head, sizeof old->head);
    rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, key, body, tmp + 88, 32);
    if (!rc) {
        memcpy(wire, tmp, sizeof tmp);
        next->sequence = old->sequence + 1u;
        memcpy(next->head, tmp + 88, sizeof next->head);
    }
    mulib_sc_wipe(tmp, sizeof tmp);
    return rc;
}

int mulib_saud_verify_record(const mulib_scr_provider *p, mulib_sc_view key,
                             mulib_saud_state *s, mulib_sc_view wire)
{
    unsigned char mac[32];
    mulib_sc_view body = {wire.ptr, 88};
    int rc;
    if (!mulib_saud_state_valid_(s) || !mulib_saud_key_valid_(key) || !wire.ptr ||
        wire.len != MULIB_SAUD_SIZE || mulib_sc_overlap(s, sizeof *s, wire.ptr, wire.len) ||
        mulib_sc_overlap(s, sizeof *s, key.ptr, key.len))
        return MULIB_SC_INVALID;
    if (s->sequence == UINT64_MAX)
        return MULIB_SC_LIMIT;
    if (mulib_sc_get64(wire.ptr) != s->sequence + 1u ||
        !mulib_sc_equal(wire.ptr + 56, s->head, sizeof s->head))
        return MULIB_SC_AUTH;

    rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, key, body, mac, sizeof mac);
    if (!rc && !mulib_sc_equal(wire.ptr + 88, mac, sizeof mac))
        rc = MULIB_SC_AUTH;
    if (!rc) {
        ++s->sequence;
        memcpy(s->head, mac, sizeof mac);
    }
    mulib_sc_wipe(mac, sizeof mac);
    return rc;
}

int mulib_saud_verify(const mulib_scr_provider *p, mulib_sc_view key,
                      mulib_saud_state *state, const unsigned char wire[MULIB_SAUD_SIZE])
{
    mulib_sc_view record = {wire, MULIB_SAUD_SIZE};
    return mulib_saud_verify_record(p, key, state, record);
}

int mulib_saud_check_anchor(const mulib_saud_state *state,
                            const mulib_saud_state *trusted_anchor)
{
    if (!mulib_saud_state_valid_(state) || !mulib_saud_state_valid_(trusted_anchor))
        return MULIB_SC_INVALID;
    if (state->sequence != trusted_anchor->sequence ||
        !mulib_sc_equal(state->head, trusted_anchor->head, sizeof state->head))
        return MULIB_SC_AUTH;
    return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif

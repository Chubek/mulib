/** @file mulib-secret-catalog.h
 * @brief Canonical catalogue codec and optimistic record revision management.
 * Define MULIB_IMPL_SECRET_CATALOG once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CATALOG_H
#define MULIB_SECRET_CATALOG_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-policy.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SCAT_ENTRY_SIZE 56u
#define MULIB_SCAT_MAX 4096u
/* Sorted by ID, no duplicates. The codec does not authenticate data.
 * Validate the encrypted/authenticated catalogue (using a dedicated derived
 * root and an anchored revision) before policy use. Commit updates jointly
 * with their corresponding records in the caller's transaction.
 *
 * Mutable outputs must not overlap each other or inputs; failures leave them
 * unchanged. find expects a validated, sorted catalogue. */
typedef struct { mulib_sp_object policy; uint32_t kind; uint64_t created; } mulib_scat_entry;
int mulib_scat_encode(const mulib_scat_entry *entries, size_t count, mulib_sc_buf out,
                       size_t *written);
int mulib_scat_decode(mulib_sc_view in, mulib_scat_entry *entries, size_t cap,
                       size_t *count);
int mulib_scat_find(const mulib_scat_entry *entries, size_t count, const unsigned char id[16],
                     size_t *index);
int mulib_scat_update(mulib_scat_entry *entry, uint64_t expected_revision,
                       uint32_t flags, uint32_t caps, uint64_t expires);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CATALOG) && !defined(MULIB_SECRET_CATALOG_IMPLEMENTED)
#define MULIB_SECRET_CATALOG_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void mulib_scat_put32_(unsigned char *out, uint32_t x)
{
    for (unsigned i = 0; i < 4; ++i)
        out[3 - i] = (unsigned char)(x >> (8 * i));
}

static uint32_t mulib_scat_get32_(const unsigned char *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16)
         | ((uint32_t)in[2] << 8) | in[3];
}

static int mulib_scat_valid_(const mulib_scat_entry *e)
{
    /* MSC1 fixes the bits allocated to flags and capabilities. */
    return e->kind && e->policy.kind == e->kind && e->policy.revision &&
           !(e->policy.flags & ~UINT32_C(7)) && !(e->policy.capabilities & ~UINT32_C(1023)) &&
           (!e->policy.expires || e->policy.expires >= e->created);
}

int mulib_scat_encode(const mulib_scat_entry *e, size_t n, mulib_sc_buf o, size_t *w)
{
    if ((!e && n) || n > MULIB_SCAT_MAX || !o.ptr || !w ||
        o.cap < 8 + n * MULIB_SCAT_ENTRY_SIZE ||
        mulib_sc_overlap(o.ptr, o.cap, w, sizeof *w) ||
        mulib_sc_overlap(o.ptr, o.cap, e, n * sizeof *e) ||
        mulib_sc_overlap(w, sizeof *w, e, n * sizeof *e))
        return MULIB_SC_INVALID;
    for (size_t i = 0; i < n; ++i)
        if (!mulib_scat_valid_(e + i) || (i && memcmp(e[i - 1].policy.id, e[i].policy.id, 16) >= 0))
            return MULIB_SC_INVALID;

    memcpy(o.ptr, "MSC1", 4);
    mulib_scat_put32_(o.ptr + 4, (uint32_t)n);
    for (size_t i = 0; i < n; ++i) {
        unsigned char *q = o.ptr + 8 + i * MULIB_SCAT_ENTRY_SIZE;
        memcpy(q, e[i].policy.id, 16);
        mulib_sc_put64(q + 16, e[i].policy.revision);
        mulib_sc_put64(q + 24, e[i].policy.expires);
        mulib_sc_put64(q + 32, e[i].created);
        mulib_scat_put32_(q + 40, e[i].kind);
        mulib_scat_put32_(q + 44, e[i].policy.flags);
        mulib_scat_put32_(q + 48, e[i].policy.capabilities);
        memset(q + 52, 0, 4);
    }
    *w = 8 + n * MULIB_SCAT_ENTRY_SIZE;
    return MULIB_SC_OK;
}

static mulib_scat_entry mulib_scat_read_(const unsigned char *q)
{
    mulib_scat_entry t;
    memset(&t, 0, sizeof t);
    memcpy(t.policy.id, q, 16);
    t.policy.revision = mulib_sc_get64(q + 16);
    t.policy.expires = mulib_sc_get64(q + 24);
    t.created = mulib_sc_get64(q + 32);
    t.kind = t.policy.kind = mulib_scat_get32_(q + 40);
    t.policy.flags = mulib_scat_get32_(q + 44);
    t.policy.capabilities = mulib_scat_get32_(q + 48);
    return t;
}

int mulib_scat_decode(mulib_sc_view in, mulib_scat_entry *e, size_t cap, size_t *count)
{
    if (!in.ptr || in.len < 8 || memcmp(in.ptr, "MSC1", 4) || !count)
        return MULIB_SC_INVALID;
    uint32_t n = mulib_scat_get32_(in.ptr + 4);
    if (n > MULIB_SCAT_MAX || n > cap || (!e && n) || in.len != 8 + n * MULIB_SCAT_ENTRY_SIZE ||
        mulib_sc_overlap(e, n * sizeof *e, in.ptr, in.len) ||
        mulib_sc_overlap(e, n * sizeof *e, count, sizeof *count) ||
        mulib_sc_overlap(count, sizeof *count, in.ptr, in.len))
        return MULIB_SC_INVALID;

    /* Validate the entire input before touching any output. */
    for (size_t i = 0; i < n; ++i) {
        const unsigned char *q = in.ptr + 8 + i * MULIB_SCAT_ENTRY_SIZE;
        mulib_scat_entry t = mulib_scat_read_(q);
        if (!mulib_scat_valid_(&t) || mulib_scat_get32_(q + 52) ||
            (i && memcmp(q - MULIB_SCAT_ENTRY_SIZE, q, 16) >= 0))
            return MULIB_SC_INVALID;
    }
    for (size_t i = 0; i < n; ++i)
        e[i] = mulib_scat_read_(in.ptr + 8 + i * MULIB_SCAT_ENTRY_SIZE);
    *count = n;
    return MULIB_SC_OK;
}

int mulib_scat_find(const mulib_scat_entry *e, size_t n, const unsigned char id[16],
                     size_t *idx)
{
    if ((!e && n) || !id || !idx || n > MULIB_SCAT_MAX ||
        mulib_sc_overlap(idx, sizeof *idx, e, n * sizeof *e) ||
        mulib_sc_overlap(idx, sizeof *idx, id, 16))
        return MULIB_SC_INVALID;
    size_t l = 0, r = n;
    while (l < r) {
        size_t m = l + (r - l) / 2;
        if (memcmp(e[m].policy.id, id, 16) < 0)
            l = m + 1;
        else
            r = m;
    }
    if (l == n || memcmp(e[l].policy.id, id, 16))
        return MULIB_SC_NOTFOUND;
    *idx = l;
    return MULIB_SC_OK;
}

int mulib_scat_update(mulib_scat_entry *e, uint64_t rev, uint32_t flags, uint32_t caps,
                       uint64_t exp)
{
    if (!e || !mulib_scat_valid_(e) || !rev)
        return MULIB_SC_INVALID;
    if (e->policy.revision != rev)
        return MULIB_SC_CONFLICT;
    if (rev == UINT64_MAX)
        return MULIB_SC_LIMIT;
    uint32_t sticky = MULIB_SP_REVOKED | MULIB_SP_NONEXPORT;
    if ((e->policy.flags & sticky) & ~flags)
        return MULIB_SC_DENIED;
    mulib_scat_entry t = *e;
    t.policy.flags = flags;
    t.policy.capabilities = caps;
    t.policy.expires = exp;
    ++t.policy.revision;
    if (!mulib_scat_valid_(&t))
        return MULIB_SC_INVALID;
    *e = t;
    return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif

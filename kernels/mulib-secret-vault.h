/** @file mulib-secret-vault.h
 * @brief Password-wrapped random vault root, lock/unlock and authenticated profile.
 * Define MULIB_IMPL_SECRET_VAULT once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_VAULT_H
#define MULIB_SECRET_VAULT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-record.h"
#include "mulib-secret-memory.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SV_HEADER 128u
/* Format MSV1: version/alg/KDF/lanes, iterations/memory BE32, salt32,
 * nonce24, reserved8, encrypted root32, tag16. Root never stored plaintext.
 * No server-side password verifier, vault UUID or rollback protection.
 * Sessions must be zero-initialized, synchronized by the caller and not copied.
 * Mutable outputs must not overlap inputs or each other. Create, unlock and
 * rewrap leave caller outputs unchanged on error. No ordinary production
 * access should use the unpinned compatibility unlock. */
typedef struct { mulib_sm_buffer root; unsigned algorithm, require_lock; } mulib_sv_session;
typedef struct { unsigned algorithm; mulib_scr_kdf_params kdf; } mulib_sv_profile;
int mulib_sv_create(const mulib_scr_provider *p, unsigned alg, mulib_scr_kdf_params params,
                    mulib_sc_view password, unsigned char header[MULIB_SV_HEADER],
                    mulib_sv_session *s, int require_lock);
int mulib_sv_unlock(const mulib_scr_provider *p, mulib_sc_view password,
                    const unsigned char header[MULIB_SV_HEADER],
                    mulib_sv_session *s, int require_lock);
/* Pass an exact approved profile from trusted configuration, never from the
 * input header itself. A mismatch returns DENIED before allocation or KDF.
 * The profile is still authenticated by the subsequent successful AEAD open.
 * Map detailed errors to a generic unlock failure at untrusted boundaries. */
int mulib_sv_unlock_pinned(const mulib_scr_provider *p, mulib_sv_profile expected,
                           mulib_sc_view password, const unsigned char header[MULIB_SV_HEADER],
                           mulib_sv_session *s, int require_lock);
/* Keeps the root and AEAD algorithm. Caller must approve params, commit the
 * new header durably and update the pinned profile as needed. Old headers
 * are not revoked. Rewrap uses the session's original lock policy. */
int mulib_sv_rewrap(const mulib_scr_provider *p, const mulib_sv_session *s,
                    mulib_scr_kdf_params params, mulib_sc_view new_password,
                    unsigned char header[MULIB_SV_HEADER]);
void mulib_sv_lock(mulib_sv_session *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_VAULT) && !defined(MULIB_SECRET_VAULT_IMPLEMENTED)
#define MULIB_SECRET_VAULT_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void mulib_sv_put32_(unsigned char *out, uint32_t x)
{
    for (unsigned i = 0; i < 4; ++i)
        out[3 - i] = (unsigned char)(x >> (8 * i));
}

static uint32_t mulib_sv_get32_(const unsigned char *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16)
         | ((uint32_t)in[2] << 8) | in[3];
}

static int mulib_sv_kdf_eq_(mulib_scr_kdf_params a, mulib_scr_kdf_params b)
{
    return a.algorithm == b.algorithm && a.iterations == b.iterations &&
           a.memory_kib == b.memory_kib && a.lanes == b.lanes;
}

static int mulib_sv_wire_profile_(unsigned a, mulib_scr_kdf_params q)
{
    return mulib_scr_key_size(a) && q.algorithm <= UINT8_MAX && q.lanes <= UINT8_MAX;
}

static int mulib_sv_header_alias_(const mulib_scr_provider *p, mulib_sc_view pw,
                                  const unsigned char *h, const mulib_sv_session *s)
{
    return mulib_sc_overlap(h, MULIB_SV_HEADER, p, sizeof *p)
        || mulib_sc_overlap(h, MULIB_SV_HEADER, pw.ptr, pw.len)
        || mulib_sc_overlap(h, MULIB_SV_HEADER, s, sizeof *s);
}

static int mulib_sv_session_alias_(const mulib_scr_provider *p, mulib_sc_view pw,
                                   const mulib_sv_session *s)
{
    return mulib_sc_overlap(s, sizeof *s, p, sizeof *p)
        || mulib_sc_overlap(s, sizeof *s, pw.ptr, pw.len);
}

static int mulib_sv_wrap_(const mulib_scr_provider *p, unsigned a,
                          mulib_scr_kdf_params q, mulib_sc_view pw,
                          const unsigned char *root, unsigned char *h, int lock)
{
    unsigned char tmp[MULIB_SV_HEADER] = {0};
    size_t kn = mulib_scr_key_size(a), nn = mulib_scr_nonce_size(a);
    memcpy(tmp, "MSV1", 4);
    tmp[4] = 1;
    tmp[5] = (unsigned char)a;
    tmp[6] = (unsigned char)q.algorithm;
    tmp[7] = (unsigned char)q.lanes;
    mulib_sv_put32_(tmp + 8, q.iterations);
    mulib_sv_put32_(tmp + 12, q.memory_kib);

    /* This protects the derived key, not any workspace internal to the KDF. */
    mulib_sm_buffer k;
    memset(&k, 0, sizeof k);
    int r = mulib_sm_alloc(&k, 32, lock);
    if (r == MULIB_SC_OK)
        r = mulib_scr_random(p, tmp + 16, 32 + nn);
    if (r == MULIB_SC_OK) {
        mulib_sc_view salt = {tmp + 16, 32};
        r = mulib_scr_kdf(p, q, pw, salt, k.ptr);
    }
    if (r == MULIB_SC_OK) {
        mulib_sc_view key = {k.ptr, kn}, nonce = {tmp + 48, nn}, ad = {tmp, 80};
        mulib_sc_view plain = {root, 32};
        mulib_sc_buf out = {tmp + 80, 32};
        r = mulib_scr_seal(p, a, key, nonce, ad, plain, out, tmp + 112);
    }
    if (r == MULIB_SC_OK)
        memcpy(h, tmp, sizeof tmp);
    mulib_sm_clear(&k);
    mulib_sc_wipe(tmp, sizeof tmp);
    return r;
}

int mulib_sv_create(const mulib_scr_provider *p, unsigned a, mulib_scr_kdf_params q,
                    mulib_sc_view pw, unsigned char h[MULIB_SV_HEADER],
                    mulib_sv_session *s, int lock)
{
    if (!h || !s || !mulib_sc_view_ok(pw) || !mulib_sv_wire_profile_(a, q) ||
        mulib_sv_header_alias_(p, pw, h, s) || mulib_sv_session_alias_(p, pw, s) ||
        s->root.ptr)
        return MULIB_SC_INVALID;

    mulib_sm_buffer root;
    memset(&root, 0, sizeof root);
    int r = mulib_sm_alloc(&root, 32, lock);
    if (r == MULIB_SC_OK)
        r = mulib_scr_random(p, root.ptr, 32);
    if (r == MULIB_SC_OK)
        r = mulib_sv_wrap_(p, a, q, pw, root.ptr, h, lock);
    if (r != MULIB_SC_OK) {
        mulib_sm_clear(&root);
        return r;
    }
    root.len = 32;
    s->root = root;
    s->algorithm = a;
    s->require_lock = lock != 0;
    return MULIB_SC_OK;
}

static int mulib_sv_unlock_(const mulib_scr_provider *p, const mulib_sv_profile *expected,
                            mulib_sc_view pw, const unsigned char *h,
                            mulib_sv_session *s, int lock)
{
    if (!h || !s || !mulib_sc_view_ok(pw) || mulib_sv_session_alias_(p, pw, s) ||
        mulib_sc_overlap(s, sizeof *s, h, MULIB_SV_HEADER) || s->root.ptr ||
        memcmp(h, "MSV1", 4) || h[4] != 1)
        return MULIB_SC_INVALID;

    unsigned a = h[5];
    mulib_scr_kdf_params q = {h[6], mulib_sv_get32_(h + 8), mulib_sv_get32_(h + 12), h[7]};
    if (expected && (a != expected->algorithm || !mulib_sv_kdf_eq_(q, expected->kdf)))
        return MULIB_SC_DENIED;
    size_t kn = mulib_scr_key_size(a), nn = mulib_scr_nonce_size(a);
    if (!kn)
        return MULIB_SC_UNSUPPORTED;
    for (size_t i = 48 + nn; i < 80; ++i)
        if (h[i])
            return MULIB_SC_INVALID;

    mulib_sm_buffer root, k;
    memset(&root, 0, sizeof root);
    memset(&k, 0, sizeof k);
    int r = mulib_sm_alloc(&root, 32, lock);
    if (r == MULIB_SC_OK)
        r = mulib_sm_alloc(&k, 32, lock);
    if (r == MULIB_SC_OK) {
        mulib_sc_view salt = {h + 16, 32};
        r = mulib_scr_kdf(p, q, pw, salt, k.ptr);
    }
    if (r == MULIB_SC_OK) {
        mulib_sc_view key = {k.ptr, kn}, nonce = {h + 48, nn}, ad = {h, 80};
        mulib_sc_view cipher = {h + 80, 32};
        mulib_sc_buf out = {root.ptr, 32};
        r = mulib_scr_open(p, a, key, nonce, ad, cipher, h + 112, out);
    }
    mulib_sm_clear(&k);
    if (r != MULIB_SC_OK) {
        mulib_sm_clear(&root);
        return r;
    }
    root.len = 32;
    s->root = root;
    s->algorithm = a;
    s->require_lock = lock != 0;
    return MULIB_SC_OK;
}

int mulib_sv_unlock(const mulib_scr_provider *p, mulib_sc_view pw,
                    const unsigned char h[MULIB_SV_HEADER], mulib_sv_session *s, int lock)
{
    return mulib_sv_unlock_(p, NULL, pw, h, s, lock);
}

int mulib_sv_unlock_pinned(const mulib_scr_provider *p, mulib_sv_profile expected,
                           mulib_sc_view pw, const unsigned char h[MULIB_SV_HEADER],
                           mulib_sv_session *s, int lock)
{
    if (!mulib_sv_wire_profile_(expected.algorithm, expected.kdf))
        return MULIB_SC_INVALID;
    return mulib_sv_unlock_(p, &expected, pw, h, s, lock);
}

int mulib_sv_rewrap(const mulib_scr_provider *p, const mulib_sv_session *s,
                    mulib_scr_kdf_params q, mulib_sc_view pw,
                    unsigned char h[MULIB_SV_HEADER])
{
    if (!s || !s->root.ptr || s->root.len != 32)
        return MULIB_SC_STATE;
    if (!h || !mulib_sc_view_ok(pw) || !mulib_sv_wire_profile_(s->algorithm, q) ||
        mulib_sv_header_alias_(p, pw, h, s) ||
        mulib_sc_overlap(h, MULIB_SV_HEADER, s->root.ptr, s->root.cap))
        return MULIB_SC_INVALID;
    return mulib_sv_wrap_(p, s->algorithm, q, pw, s->root.ptr, h, s->require_lock != 0);
}

void mulib_sv_lock(mulib_sv_session *s)
{
    if (s) {
        mulib_sm_clear(&s->root);
        s->algorithm = 0;
        s->require_lock = 0;
    }
}
#ifdef __cplusplus
}
#endif
#endif

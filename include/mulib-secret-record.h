/** @file mulib-secret-record.h
 * @brief Authenticated encrypted record envelope with per-write HKDF key and random nonce.
 * Define MULIB_IMPL_SECRET_RECORD once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_RECORD_H
#define MULIB_SECRET_RECORD_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SR_HEADER 96u
#define MULIB_SR_OVERHEAD 112u
#define MULIB_SR_MAX (16u*1024u*1024u)
typedef struct { unsigned char id[16]; uint64_t revision; uint32_t kind; } mulib_sr_meta;
/* Root is exactly 32 bytes. Mutable outputs must not overlap each other or
 * input spans. On error, written is unchanged. Validation failures leave
 * output untouched; crypto failures erase the exact result span. The
 * unused output tail is never modified. Metadata is public but authenticated.
 * Wire: magic 4, version 1, alg 1, reserved 2, id 16, rev 8, kind 4,
 * length 4, salt 32, nonce 24, ciphertext, tag 16. Integers are big endian.
 * MSR1 does not bind a vault UUID; do not reuse roots across vaults. */
int mulib_sr_seal(const mulib_scr_provider *p, unsigned alg, const unsigned char root[32],
                  const mulib_sr_meta *meta, mulib_sc_view data, mulib_sc_buf out,
                  size_t *written);
/* Compatibility open authenticates the encoded algorithm, but does not pin
 * it. Use pinned open with the algorithm from a trusted vault profile for
 * ordinary access. A profile mismatch returns DENIED before key derivation. */
int mulib_sr_open(const mulib_scr_provider *p, const unsigned char root[32],
                  const mulib_sr_meta *expected, mulib_sc_view wire, mulib_sc_buf out,
                  size_t *written);
int mulib_sr_open_pinned(const mulib_scr_provider *p, unsigned expected_alg,
                         const unsigned char root[32], const mulib_sr_meta *expected,
                         mulib_sc_view wire, mulib_sc_buf out, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_RECORD) && !defined(MULIB_SECRET_RECORD_IMPLEMENTED)
#define MULIB_SECRET_RECORD_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void mulib_sr_put32_(unsigned char *out, uint32_t x)
{
    for (unsigned i = 0; i < 4; ++i)
        out[3 - i] = (unsigned char)(x >> (8 * i));
}

static uint32_t mulib_sr_get32_(const unsigned char *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16)
         | ((uint32_t)in[2] << 8) | in[3];
}

static int mulib_sr_alias_(const mulib_scr_provider *p, const unsigned char *root,
                           const mulib_sr_meta *m, mulib_sc_view in,
                           mulib_sc_buf out, const size_t *written)
{
    return mulib_sc_overlap(out.ptr, out.cap, written, sizeof *written)
        || mulib_sc_overlap(out.ptr, out.cap, p, sizeof *p)
        || mulib_sc_overlap(out.ptr, out.cap, root, 32)
        || mulib_sc_overlap(out.ptr, out.cap, m, sizeof *m)
        || mulib_sc_overlap(out.ptr, out.cap, in.ptr, in.len)
        || mulib_sc_overlap(written, sizeof *written, p, sizeof *p)
        || mulib_sc_overlap(written, sizeof *written, root, 32)
        || mulib_sc_overlap(written, sizeof *written, m, sizeof *m)
        || mulib_sc_overlap(written, sizeof *written, in.ptr, in.len);
}

static int mulib_sr_key_(const mulib_scr_provider *p, const unsigned char root[32],
                          const unsigned char *w, unsigned char k[32], size_t kn)
{
    unsigned char info[36];
    memcpy(info, "MuLIB.record.v1", 15);
    memcpy(info + 15, w + 8, 16);
    info[31] = w[5];
    memcpy(info + 32, w + 32, 4);
    mulib_sc_view salt = {w + 40, 32}, ikm = {root, 32}, label = {info, sizeof info};
    return mulib_scr_hkdf(p, MULIB_SCR_SHA256, salt, ikm, label, k, kn);
}

int mulib_sr_seal(const mulib_scr_provider *p, unsigned a, const unsigned char root[32],
                  const mulib_sr_meta *m, mulib_sc_view d, mulib_sc_buf o, size_t *written)
{
    size_t kn = mulib_scr_key_size(a), nn = mulib_scr_nonce_size(a);
    if (!root || !m || !m->revision || !m->kind || !written || !kn || !o.ptr ||
        !mulib_sc_view_ok(d) || mulib_sr_alias_(p, root, m, d, o, written))
        return MULIB_SC_INVALID;
    if (d.len > MULIB_SR_MAX || o.cap < d.len + MULIB_SR_OVERHEAD)
        return MULIB_SC_LIMIT;

    unsigned char *w = o.ptr;
    memset(w, 0, MULIB_SR_HEADER);
    memcpy(w, "MSR1", 4);
    w[4] = 1;
    w[5] = (unsigned char)a;
    memcpy(w + 8, m->id, 16);
    mulib_sc_put64(w + 24, m->revision);
    mulib_sr_put32_(w + 32, m->kind);
    mulib_sr_put32_(w + 36, (uint32_t)d.len);

    unsigned char k[32];
    int r = mulib_scr_random(p, w + 40, 32 + nn);
    if (r == MULIB_SC_OK)
        r = mulib_sr_key_(p, root, w, k, kn);
    if (r == MULIB_SC_OK) {
        mulib_sc_view key = {k, kn}, nonce = {w + 72, nn}, ad = {w, MULIB_SR_HEADER};
        mulib_sc_buf cipher = {w + MULIB_SR_HEADER, d.len};
        r = mulib_scr_seal(p, a, key, nonce, ad, d, cipher, w + MULIB_SR_HEADER + d.len);
    }
    mulib_sc_wipe(k, sizeof k);
    if (r != MULIB_SC_OK) {
        mulib_sc_wipe(w, d.len + MULIB_SR_OVERHEAD);
        return r;
    }
    *written = d.len + MULIB_SR_OVERHEAD;
    return MULIB_SC_OK;
}

static int mulib_sr_open_(const mulib_scr_provider *p, unsigned expected_alg,
                           const unsigned char root[32], const mulib_sr_meta *m,
                           mulib_sc_view wire, mulib_sc_buf o, size_t *written)
{
    const unsigned char *w = wire.ptr;
    if (!root || !m || !m->revision || !m->kind || !w || wire.len < MULIB_SR_OVERHEAD ||
        (!o.ptr && o.cap) || !written || mulib_sr_alias_(p, root, m, wire, o, written))
        return MULIB_SC_INVALID;
    if (memcmp(w, "MSR1", 4) || w[4] != 1 || w[6] || w[7])
        return MULIB_SC_INVALID;
    if (expected_alg && w[5] != expected_alg)
        return MULIB_SC_DENIED;
    size_t kn = mulib_scr_key_size(w[5]), nn = mulib_scr_nonce_size(w[5]);
    uint32_t n = mulib_sr_get32_(w + 36);
    if (!kn)
        return MULIB_SC_UNSUPPORTED;
    if (n > MULIB_SR_MAX || wire.len != n + MULIB_SR_OVERHEAD)
        return MULIB_SC_INVALID;
    if (n > o.cap)
        return MULIB_SC_LIMIT;
    for (size_t i = nn; i < 24; ++i)
        if (w[72 + i])
            return MULIB_SC_INVALID;
    if (memcmp(w + 8, m->id, 16) || mulib_sc_get64(w + 24) != m->revision ||
        mulib_sr_get32_(w + 32) != m->kind)
        return MULIB_SC_AUTH;

    unsigned char k[32];
    int r = mulib_sr_key_(p, root, w, k, kn);
    if (r == MULIB_SC_OK) {
        mulib_sc_view key = {k, kn}, nonce = {w + 72, nn}, ad = {w, MULIB_SR_HEADER};
        mulib_sc_view cipher = {w + MULIB_SR_HEADER, n};
        mulib_sc_buf plain = {o.ptr, n};
        r = mulib_scr_open(p, w[5], key, nonce, ad, cipher, w + MULIB_SR_HEADER + n, plain);
    }
    mulib_sc_wipe(k, sizeof k);
    if (r != MULIB_SC_OK) {
        mulib_sc_wipe(o.ptr, n);
        return r;
    }
    *written = n;
    return MULIB_SC_OK;
}

int mulib_sr_open(const mulib_scr_provider *p, const unsigned char root[32],
                  const mulib_sr_meta *m, mulib_sc_view wire, mulib_sc_buf o, size_t *written)
{
    return mulib_sr_open_(p, 0, root, m, wire, o, written);
}

int mulib_sr_open_pinned(const mulib_scr_provider *p, unsigned expected_alg,
                         const unsigned char root[32], const mulib_sr_meta *m,
                         mulib_sc_view wire, mulib_sc_buf o, size_t *written)
{
    if (!mulib_scr_key_size(expected_alg))
        return MULIB_SC_INVALID;
    return mulib_sr_open_(p, expected_alg, root, m, wire, o, written);
}
#ifdef __cplusplus
}
#endif
#endif

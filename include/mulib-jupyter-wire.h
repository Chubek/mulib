/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_JUPYTER_WIRE_H
#define MULIB_JUPYTER_WIRE_H
#include "mulib-authcrypto.h"
#include "mulib-json.h"
#include <string.h>

/* This envelope borrows the parts and their contents. Framing is separate
 * from authentication and JSON decoding. Only the four JSON byte frames
 * (header, parent header, metadata, content) are signed; buffers are not. */
typedef struct {
    const unsigned char *p;
    size_t n;
} mulib_jupyter_part;

typedef int (*mulib_jupyter_hmac_fn)(void *, const mulib_jupyter_part *,
                                     size_t, unsigned char digest[32]);

typedef struct {
    const mulib_jupyter_part *identity;
    size_t identities;
    mulib_jupyter_part signature, json[4];
    const mulib_jupyter_part *buffers;
    size_t nbuf;
} mulib_jupyter_wire;

typedef struct {
    const void *bytes; /* borrowed for the duration of signing or verification */
    size_t length;
} mulib_jupyter_hmac_key;

/* Optional callback using MuLIB's (not independently audited) authcrypto HMAC.
 * Link its implementation once. An empty key is not signed mode. */
static inline int mulib_jupyter_hmac_sha256(void *user, const mulib_jupyter_part *p,
                                           size_t n, unsigned char digest[32])
{
    const mulib_jupyter_hmac_key *key = (const mulib_jupyter_hmac_key *)user;
    if (!key || !key->bytes || !key->length || !p || n != 4)
        return 0;
    mulib_authcrypto_part parts[4];
    for (size_t i = 0; i < n; ++i)
        parts[i] = (mulib_authcrypto_part){p[i].p, p[i].n};
    return mulib_authcrypto_hmac_parts(MULIB_AUTHCRYPTO_SHA256, key->bytes,
                                      key->length, parts, n, digest, 32);
}

static inline int mulib_jupyter_parse(const mulib_jupyter_part *p, size_t n,
                                     size_t limit, mulib_jupyter_wire *out)
{
    if (!p || !out || n < 6 || n > 1024)
        return 0;
    size_t bytes = 0, delimiter = SIZE_MAX;
    for (size_t i = 0; i < n; ++i) {
        if ((!p[i].p && p[i].n) || p[i].n > limit - bytes)
            return 0;
        bytes += p[i].n;
        if (delimiter == SIZE_MAX && p[i].n == 9 && !memcmp(p[i].p, "<IDS|MSG>", 9))
            delimiter = i;
    }
    if (delimiter == SIZE_MAX || n - delimiter < 6 ||
        (p[delimiter + 1].n != 0 && p[delimiter + 1].n != 64))
        return 0;
    mulib_jupyter_wire wire = {
        .identity = p, .identities = delimiter, .signature = p[delimiter + 1],
        .buffers = p + delimiter + 6, .nbuf = n - delimiter - 6
    };
    for (size_t i = 0; i < 4; ++i) {
        wire.json[i] = p[delimiter + 2 + i];
        if (!wire.json[i].n)
            return 0;
    }
    *out = wire;
    return 1;
}

static inline int mulib_jupyter_sign(const mulib_jupyter_part json[4],
                                     mulib_jupyter_hmac_fn fn, void *user, char hex[65])
{
    if (!json || !fn || !hex)
        return 0;
    for (size_t i = 0; i < 4; ++i)
        if (!json[i].p || !json[i].n)
            return 0;
    unsigned char digest[32];
    if (!fn(user, json, 4, digest))
        return 0;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof digest; ++i) {
        hex[2 * i] = digits[digest[i] >> 4];
        hex[2 * i + 1] = digits[digest[i] & 15];
    }
    hex[64] = 0;
    return 1;
}

static inline int mulib_jupyter_verify(const mulib_jupyter_wire *w,
                                       mulib_jupyter_hmac_fn fn, void *user, int allow_unsigned)
{
    if (!w)
        return 0;
    if (!w->signature.n)
        return !fn && allow_unsigned;
    if (!fn || w->signature.n != 64 || !w->signature.p)
        return 0;
    char expected[65];
    if (!mulib_jupyter_sign(w->json, fn, user, expected))
        return 0;
    unsigned diff = 0;
    for (size_t i = 0; i < 64; ++i)
        diff |= (unsigned char)expected[i] ^ w->signature.p[i];
    return diff == 0;
}

/* Decode the four JSON objects, in wire order, after verifying the signature.
 * The caller supplies a total JSON-byte budget and an empty output array.
 * On success the caller owns four objects (destroy each with mulib_json_destroy).
 * On failure out is unchanged. Message schemas, routing and replay are not checked. */
static inline int mulib_jupyter_decode_json(const mulib_jupyter_wire *w,
                                           size_t limit, mulib_json_t *out[4])
{
    if (!w || !out)
        return 0;
    for (size_t i = 0; i < 4; ++i) {
        if (!w->json[i].p || !w->json[i].n || w->json[i].n > limit)
            return 0;
        limit -= w->json[i].n;
    }
    mulib_json_t *decoded[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        decoded[i] = mulib_json_parse((const char *)w->json[i].p, w->json[i].n, NULL);
        if (!decoded[i] || mulib_json_typeof(decoded[i]) != MULIB_JSON_OBJECT) {
            for (size_t j = 0; j <= i; ++j)
                mulib_json_destroy(decoded[j]);
            return 0;
        }
    }
    memcpy(out, decoded, sizeof decoded);
    return 1;
}
#endif

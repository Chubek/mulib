/** @file mulib-secret-selftest.h
 * @brief Backend conformance smoke tests with RFC vectors and authenticated storage rejection tests.
 * Define MULIB_IMPL_SECRET_SELFTEST once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_SELFTEST_H
#define MULIB_SECRET_SELFTEST_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-vault.h"
#include "mulib-secret-otp.h"
#include "mulib-secret-codec.h"
#include "mulib-secret-recovery.h"
#include "mulib-secret-jose.h"
#include "mulib-secret-otp-uri.h"
#include "mulib-secret-catalog.h"
#include "mulib-secret-lease.h"
#include "mulib-secret-audit.h"
#include "mulib-secret-aws.h"
#include "mulib-secret-oauth.h"
#include "mulib-secret-random.h"
#include "mulib-secret-passphrase.h"
#include "mulib-secret-rate.h"
#include "mulib-secret-ssh.h"
#include "mulib-secret-spec.h"
#include "mulib-secret-provider.h"
#include "mulib-secret-webauthn.h"
#ifdef __cplusplus
extern "C" {
#endif
/* For development/CI: return 0 or a stable failing case number. Uses synthetic
 * secrets only. Tests SHA1/256/512, HKDF, chosen AEAD, vault, OTP and sharing.
 * A passing result is not a security audit. */
int mulib_secret_selftest(const mulib_scr_provider *p,unsigned a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_SELFTEST) && !defined(MULIB_SECRET_SELFTEST_IMPLEMENTED)
#define MULIB_SECRET_SELFTEST_IMPLEMENTED
#include <string.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_sst_hex_(const unsigned char *bytes, size_t n, const char *expected)
{
    char hex[129];
    size_t written = 0;
    mulib_sc_view view = {bytes, n};
    return n <= 64 &&
        !mulib_scode_encode(MULIB_SCODE_HEX, view, hex, sizeof hex, &written) &&
        !strcmp(hex, expected);
}

static int mulib_sst_cas_(void *ctx, const mulib_ss_counter *old,
                          const mulib_ss_counter *next)
{
    mulib_ss_counter *current = (mulib_ss_counter *)ctx;
    if (current->revision != old->revision || current->counter != old->counter ||
        memcmp(current->id, old->id, sizeof current->id))
        return MULIB_SC_CONFLICT;
    *current = *next;
    return 0;
}

static int mulib_sst_run_(void *ctx, const mulib_sp_object *obj,
                          const mulib_spro_request *request, mulib_sc_buf buf, size_t *written)
{
    (void)ctx;
    (void)obj;
    if (request->input.len > buf.cap)
        return MULIB_SC_LIMIT;
    if (request->input.len)
        memcpy(buf.ptr, request->input.ptr, request->input.len);
    *written = request->input.len;
    return 0;
}

static int mulib_sst_vault_(const mulib_scr_provider *p, unsigned alg)
{
    static const unsigned char password[] = "test password not real", other[] = "wrong";
    mulib_sc_view pw = {password, sizeof password - 1}, wrong = {other, sizeof other - 1};
    mulib_scr_kdf_params kdf = {MULIB_SCR_PBKDF2_SHA256, 600000, 0, 0};
    mulib_sv_profile profile = {alg, kdf}, unapproved = profile;
    mulib_sv_session first, unlock;
    unsigned char header[MULIB_SV_HEADER];
    int result = 0;
    memset(&first, 0, sizeof first);
    memset(&unlock, 0, sizeof unlock);
    ++unapproved.kdf.iterations;

    if (mulib_sv_create(p, alg, kdf, pw, header, &first, 0))
        result = 14;
    else if (mulib_sv_unlock_pinned(p, unapproved, pw, header, &unlock, 0) != MULIB_SC_DENIED)
        result = 43;
    else if (mulib_sv_unlock_pinned(p, profile, wrong, header, &unlock, 0) != MULIB_SC_AUTH)
        result = 15;
    else if (mulib_sv_unlock_pinned(p, profile, pw, header, &unlock, 0) ||
             !mulib_sc_equal(first.root.ptr, unlock.root.ptr, 32))
        result = 16;
    else {
        mulib_sv_lock(&unlock);
        if (mulib_sv_rewrap(p, &first, kdf, wrong, header) ||
            mulib_sv_unlock_pinned(p, profile, wrong, header, &unlock, 0) ||
            !mulib_sc_equal(first.root.ptr, unlock.root.ptr, 32))
            result = 17;
    }
    mulib_sv_lock(&first);
    mulib_sv_lock(&unlock);
    return result;
}

static int mulib_sst_gcm_block_(const mulib_scr_provider *p, unsigned alg)
{
    /* GCMVS: zero key, 96-bit zero IV, one zero block, and no AAD. */
    unsigned char zero[32] = {0}, cipher[16], tag[16], plain[16];
    mulib_sc_view key = {zero, mulib_scr_key_size(alg)};
    mulib_sc_view nonce = {zero, 12}, input = {zero, 16}, aad = {NULL, 0};
    mulib_sc_view ciphertext = {cipher, sizeof cipher};
    mulib_sc_buf sealed = {cipher, sizeof cipher}, opened = {plain, sizeof plain};
    const char *expected_cipher = alg == MULIB_SCR_AES128_GCM
        ? "0388dace60b6a392f328c2b971b2fe78" : "cea7403d4d606b6e074ec5d3baf39d18";
    const char *expected_tag = alg == MULIB_SCR_AES128_GCM
        ? "ab6e47d42cec13bdf53a67b21257bddf" : "d0d1c8a799996bf0265b98b5d48ab919";

    if (mulib_scr_seal(p, alg, key, nonce, aad, input, sealed, tag) ||
        !mulib_sst_hex_(cipher, sizeof cipher, expected_cipher) ||
        !mulib_sst_hex_(tag, sizeof tag, expected_tag))
        return 40;
    if (mulib_scr_open(p, alg, key, nonce, aad, ciphertext, tag, opened) ||
        !mulib_sc_equal(plain, zero, sizeof plain))
        return 41;

    tag[0] ^= 1;
    memset(plain, 0xA5, sizeof plain);
    if (mulib_scr_open(p, alg, key, nonce, aad, ciphertext, tag, opened) != MULIB_SC_AUTH ||
        !mulib_sc_equal(plain, zero, sizeof plain))
        return 42;
    return 0;
}

int mulib_secret_selftest(const mulib_scr_provider *p, unsigned alg)
{
    unsigned char digest[64], key[32], root[32], wire[512], plain[512];
    char code[9], text[512];
    size_t n = 0, w = 0;
    mulib_sc_view abc = {(const unsigned char *)"abc", 3};
    mulib_sc_buf out = {plain, sizeof plain}, encoded = {wire, sizeof wire};
    if (mulib_scr_hash(p, MULIB_SCR_SHA256, abc, digest, sizeof digest) ||
        !mulib_sst_hex_(digest, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"))
        return 1;

    unsigned char ikm[22], salt[13], info[10];
    memset(ikm, 0x0b, sizeof ikm);
    for (unsigned i = 0; i < sizeof salt; ++i)
        salt[i] = (unsigned char)i;
    for (unsigned i = 0; i < sizeof info; ++i)
        info[i] = (unsigned char)(0xf0 + i);
    mulib_sc_view iv = {ikm, sizeof ikm}, sv = {salt, sizeof salt}, inf = {info, sizeof info};
    if (mulib_scr_hkdf(p, MULIB_SCR_SHA256, sv, iv, inf, digest, 42) ||
        !mulib_sst_hex_(digest, 42, "3cb25f25faacd57a90434f64d0362f2a2"
                                   "d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"))
        return 2;

    mulib_sc_view otp = {(const unsigned char *)"12345678901234567890", 20};
    mulib_sc_view otp256 = {(const unsigned char *)"12345678901234567890123456789012", 32};
    mulib_sc_view otp512 = {
        (const unsigned char *)"1234567890123456789012345678901234567890123456789012345678901234", 64
    };
    if (mulib_sotp_hotp(p, MULIB_SCR_SHA1, otp, 0, 6, code, sizeof code) || strcmp(code, "755224"))
        return 3;
    if (mulib_sotp_totp(p, MULIB_SCR_SHA1, otp, 59, 0, 30, 8, code, sizeof code, NULL) ||
        strcmp(code, "94287082"))
        return 4;
    if (mulib_sotp_totp(p, MULIB_SCR_SHA256, otp256, 59, 0, 30, 8, code, sizeof code, NULL) ||
        strcmp(code, "46119246"))
        return 5;
    if (mulib_sotp_totp(p, MULIB_SCR_SHA512, otp512, 59, 0, 30, 8, code, sizeof code, NULL) ||
        strcmp(code, "90693936"))
        return 6;
    if (mulib_scode_encode(MULIB_SCODE_BASE64URL, abc, text, sizeof text, &n) ||
        strcmp(text, "YWJj") || mulib_scode_decode(MULIB_SCODE_BASE64URL, text, n, out, &w) ||
        w != abc.len || memcmp(plain, abc.ptr, w) ||
        !mulib_scode_decode(MULIB_SCODE_BASE64URL, "Zh", 2, out, &w))
        return 7;
    if (mulib_scr_random(p, root, sizeof root) || mulib_scr_random(p, key, sizeof key))
        return 8;

    int gcm = alg == MULIB_SCR_AES128_GCM || alg == MULIB_SCR_AES256_GCM;
    if (gcm) {
        unsigned char zero[32] = {0}, tag[16];
        mulib_sc_view k = {zero, mulib_scr_key_size(alg)}, nonce = {zero, 12}, empty = {NULL, 0};
        if (mulib_scr_seal(p, alg, k, nonce, empty, empty, out, tag) ||
            !mulib_sst_hex_(tag, 16, alg == MULIB_SCR_AES128_GCM
                ? "58e2fccefa7e3061367f1d57a4e7455a" : "530f8afbc74536b9a963b4f1c4cb738b"))
            return 9;
    }

    mulib_sr_meta meta = {{7}, 1, 4};
    if (mulib_sr_seal(p, alg, root, &meta, abc, encoded, &n))
        return 10;
    mulib_sc_view record = {wire, n};
    if (mulib_sr_open_pinned(p, alg, root, &meta, record, out, &w) ||
        w != abc.len || memcmp(plain, abc.ptr, w))
        return 11;
    wire[n - 1] ^= 1;
    if (mulib_sr_open_pinned(p, alg, root, &meta, record, out, &w) != MULIB_SC_AUTH)
        return 12;
    wire[n - 1] ^= 1;
    ++meta.revision;
    if (mulib_sr_open_pinned(p, alg, root, &meta, record, out, &w) != MULIB_SC_AUTH)
        return 13;
    int result = mulib_sst_vault_(p, alg);
    if (result)
        return result;

    mulib_sc_view kv = {key, sizeof key};
    if (mulib_sjose_sign(p, kv, abc, text, sizeof text, &n) ||
        mulib_sjose_verify(p, kv, text, n, out, &w) || w != abc.len || memcmp(plain, abc.ptr, w))
        return 18;
    text[n - 3] = text[n - 3] == 'A' ? 'B' : 'A';
    if (!mulib_sjose_verify(p, kv, text, n, out, &w))
        return 19;

    if (mulib_srec_split(p, kv, 3, 5, encoded))
        return 20;
    unsigned char subset[99];
    memcpy(subset, wire, 33);
    memcpy(subset + 33, wire + 66, 33);
    memcpy(subset + 66, wire + 132, 33);
    mulib_sc_view shares = {subset, sizeof subset};
    if (mulib_srec_join(shares, sizeof key, 3, 3, out) || memcmp(plain, key, sizeof key))
        return 21;
    subset[33] = subset[0];
    if (!mulib_srec_join(shares, sizeof key, 3, 3, out))
        return 22;

    const char *url = "otpauth://totp/Test%3Aalice?secret=JBSWY3DPEHPK3PXP&issuer=Test";
    mulib_sotp_uri uri;
    if (mulib_sotp_uri_parse(url, strlen(url), &uri))
        return 23;
    int parsed = uri.key_length == 10 && !strcmp(uri.issuer, "Test");
    mulib_sotp_uri_clear(&uri);
    if (!parsed)
        return 23;
    mulib_ss_counter counter = {1, 0, {0}}, committed = counter;
    if (mulib_sotp_next(p, MULIB_SCR_SHA1, otp, 6, &counter, mulib_sst_cas_, &committed,
                        code, sizeof code) || strcmp(code, "755224") ||
        counter.counter != 1 || committed.revision != 2)
        return 24;

    mulib_sp_object obj;
    mulib_sp_rule rule;
    memset(&obj, 0, sizeof obj);
    memset(&rule, 0, sizeof rule);
    const char *subject = "client";
    obj.kind = 4;
    obj.revision = 1;
    obj.capabilities = MULIB_SP_GENERATE;
    rule.subject = subject;
    rule.allow = rule.confirm = MULIB_SP_GENERATE;
    if (mulib_sp_check(&rule, 1, subject, &obj, MULIB_SP_GENERATE, 1, 0) != MULIB_SC_DENIED ||
        mulib_sp_check(&rule, 1, subject, &obj, MULIB_SP_GENERATE, 1, 1))
        return 25;

    mulib_spro_registry reg;
    memset(&reg, 0, sizeof reg);
    mulib_spro_entry pe = {1, 4, MULIB_SP_GENERATE, "test", NULL, mulib_sst_run_};
    mulib_spro_request request = {MULIB_SP_GENERATE, 1, abc};
    if (mulib_spro_register(&reg, &pe) || mulib_spro_freeze(&reg) ||
        mulib_spro_call(&reg, 4, &rule, 1, subject, &obj, &request, 1, out, &w) ||
        w != abc.len ||
        mulib_spro_call(&reg, 5, &rule, 1, subject, &obj, &request, 1, out, &w) != MULIB_SC_INVALID)
        return 26;

    mulib_sl_approval lease;
    memset(&lease, 0, sizeof lease);
    if (mulib_sl_issue(p, &lease, subject, &obj, MULIB_SP_GENERATE, abc, 1, 10))
        return 27;
    memcpy(digest, lease.token, 32);
    if (mulib_sl_consume(p, &lease, digest, subject, &obj, MULIB_SP_GENERATE, abc, 2) ||
        !mulib_sl_consume(p, &lease, digest, subject, &obj, MULIB_SP_GENERATE, abc, 2))
        return 28;

    mulib_scat_entry entry = {obj, 4, 0}, decoded;
    if (mulib_scat_encode(&entry, 1, encoded, &n))
        return 29;
    mulib_sc_view catalogue = {wire, n};
    if (mulib_scat_decode(catalogue, &decoded, 1, &w) ||
        w != 1 || decoded.kind != 4 || decoded.policy.revision != 1)
        return 30;

    mulib_saud_state audit, next;
    memset(&audit, 0, sizeof audit);
    mulib_saud_event event;
    memset(&event, 0, sizeof event);
    event.operation = 2;
    if (mulib_saud_append(p, kv, &audit, &event, wire, &next) ||
        mulib_saud_verify(p, kv, &audit, wire) || !mulib_sc_equal(audit.head, next.head, 32))
        return 31;

    mulib_srate_state rate;
    memset(&rate, 0, sizeof rate);
    uint64_t wait;
    if (mulib_srate_failure(&rate, 10, 2, 60) ||
        mulib_srate_check(&rate, 11, &wait) != MULIB_SC_DENIED || wait != 1 ||
        mulib_srate_success(&rate, 12))
        return 32;

    if (mulib_srand_password(p, 15, 24, text, sizeof text) || strlen(text) != 24)
        return 33;
    char verifier[44], challenge[44];
    if (mulib_soauth_pkce(p, verifier, challenge) || strlen(verifier) != 43 || strlen(challenge) != 43)
        return 34;
    if (mulib_swa_authdata(p, "example.com", 42, 1, 1, plain) || plain[32] != 5 || plain[36] != 42)
        return 35;
    mulib_sc_view aws = {(const unsigned char *)"wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", 40};
    if (mulib_saws_key(p, aws, "20120215", "us-east-1", "iam", digest) ||
        !mulib_sst_hex_(digest, 32, "f4780e2d9f65fa895f9c67b32ce1baf0b0d8a43505a000a1a9e090d414db404d"))
        return 36;
    if (mulib_ssh_secret_ed25519_blob(key, plain) || memcmp(plain + 4, "ssh-ed25519", 11))
        return 37;
    if (mulib_sphrase_recovery(p, text) || mulib_sphrase_verifier(p, kv, text, digest))
        return 38;
    if (!mulib_sspec_find("totp") || mulib_sspec_count() < 40)
        return 39;

    mulib_sc_wipe(digest, sizeof digest);
    mulib_sc_wipe(key, sizeof key);
    mulib_sc_wipe(root, sizeof root);
    mulib_sc_wipe(wire, sizeof wire);
    mulib_sc_wipe(plain, sizeof plain);
    mulib_sc_wipe(text, sizeof text);
    mulib_sc_wipe(subset, sizeof subset);
    return gcm ? mulib_sst_gcm_block_(p, alg) : 0;
}
#ifdef __cplusplus
}
#endif
#endif

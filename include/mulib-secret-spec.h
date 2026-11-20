/** @file mulib-secret-spec.h
 * @brief Stable credential kind catalogue and intended operation metadata.
 * Define MULIB_IMPL_SECRET_SPEC once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_SPEC_H
#define MULIB_SECRET_SPEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-policy.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    MULIB_SSPEC_STATIC = 1, MULIB_SSPEC_LOCAL = 2,
    MULIB_SSPEC_REMOTE = 3, MULIB_SSPEC_HARDWARE = 4
};
enum { MULIB_SSPEC_FORMAT = 1, MULIB_SSPEC_PRIMITIVE = 2, MULIB_SSPEC_HOST = 3 };
typedef struct {
    uint32_t kind;
    const char *name, *standard;
    unsigned source, stage, operations;
} mulib_sspec;

/* Static metadata only, not a registry of supported or allowed commands.
 * FORMAT describes storage/metadata, PRIMITIVE a partial local mechanism,
 * HOST a provider/backend/integration still needed. The broker discovers
 * capabilities from vetted registrations, never from this table. Results
 * point to immutable static data; absent entries return NULL. */
size_t mulib_sspec_count(void);
const mulib_sspec *mulib_sspec_at(size_t i);
const mulib_sspec *mulib_sspec_find(const char *name);
const mulib_sspec *mulib_sspec_find_kind(uint32_t kind);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_SPEC) && !defined(MULIB_SECRET_SPEC_IMPLEMENTED)
#define MULIB_SECRET_SPEC_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SSPEC_R MULIB_SP_READ
#define MULIB_SSPEC_G MULIB_SP_GENERATE
#define MULIB_SSPEC_S MULIB_SP_SIGN
#define MULIB_SSPEC_D MULIB_SP_DERIVE
#define MULIB_SSPEC_F MULIB_SP_REFRESH
#define MULIB_SSPEC_E MULIB_SP_ENCRYPT
#define MULIB_SSPEC_X MULIB_SP_DECRYPT
static const mulib_sspec mulib_sspec_table_[] = {
    { 1, "password", "local", MULIB_SSPEC_STATIC, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_R | MULIB_SSPEC_G },
    { 2, "note", "opaque", MULIB_SSPEC_STATIC, MULIB_SSPEC_FORMAT, MULIB_SSPEC_R },
    { 3, "api-key", "issuer-specific", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_R | MULIB_SSPEC_F },
    { 4, "totp", "RFC6238", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_G },
    { 5, "hotp", "RFC4226", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_G },
    { 6, "steam-guard", "Steam", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_G },
    { 7, "ocra", "RFC6287", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_G },
    { 8, "recovery-code", "local", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_G },
    { 9, "hs256", "RFC7515/7518", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_S },
    {10, "oauth-refresh", "RFC6749/9700", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_F },
    {11, "oauth-pkce", "RFC7636", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_G },
    {12, "dpop", "RFC9449", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {13, "aws-sigv4", "AWS SigV4", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_S },
    {14, "azure-sas", "Azure SAS", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {15, "gcp-service-account", "RFC7523", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_S | MULIB_SSPEC_F },
    {16, "ssh-ed25519", "RFC8709", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {17, "x25519", "RFC7748", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_D },
    {18, "webauthn", "WebAuthn/CTAP2", MULIB_SSPEC_HARDWARE, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {19, "x509", "RFC5280", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {20, "openpgp", "RFC9580", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S | MULIB_SSPEC_E | MULIB_SSPEC_X },
    {21, "wireguard", "WireGuard", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_D },
    {22, "kerberos", "RFC4120", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_F },
    {23, "pkcs11", "PKCS11", MULIB_SSPEC_HARDWARE, MULIB_SSPEC_HOST, MULIB_SSPEC_S | MULIB_SSPEC_D },
    {24, "tpm", "TPM2", MULIB_SSPEC_HARDWARE, MULIB_SSPEC_HOST, MULIB_SSPEC_S | MULIB_SSPEC_D },
    {25, "bip39", "BIP39", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_D },
    {26, "wallet", "chain-specific", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {27, "paseto", "PASETO v4", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S | MULIB_SSPEC_E },
    {28, "macaroon", "Macaroons", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {29, "biscuit", "Biscuit", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {30, "ml-dsa", "FIPS204", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {31, "ml-kem", "FIPS203", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_D },
    {32, "shamir", "GF256 raw", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_D },
    {33, "wifi", "WPA", MULIB_SSPEC_STATIC, MULIB_SSPEC_FORMAT, MULIB_SSPEC_R },
    {34, "db-password", "engine-specific", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_R | MULIB_SSPEC_F },
    {35, "aes-gcm", "SP800-38D", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_E | MULIB_SSPEC_X },
    {36, "xchacha20-poly1305", "XChaCha20-Poly1305", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_E | MULIB_SSPEC_X },
    {37, "webhook", "HMAC SHA256", MULIB_SSPEC_LOCAL, MULIB_SSPEC_PRIMITIVE, MULIB_SSPEC_S },
    {38, "ssh-ca", "OpenSSH certificates", MULIB_SSPEC_LOCAL, MULIB_SSPEC_HOST, MULIB_SSPEC_S },
    {39, "acme", "RFC8555", MULIB_SSPEC_REMOTE, MULIB_SSPEC_HOST, MULIB_SSPEC_F },
    {40, "mtls", "TLS1.3", MULIB_SSPEC_HARDWARE, MULIB_SSPEC_HOST, MULIB_SSPEC_S}
};
#undef MULIB_SSPEC_R
#undef MULIB_SSPEC_G
#undef MULIB_SSPEC_S
#undef MULIB_SSPEC_D
#undef MULIB_SSPEC_F
#undef MULIB_SSPEC_E
#undef MULIB_SSPEC_X
size_t mulib_sspec_count(void)
{
    return sizeof mulib_sspec_table_ / sizeof mulib_sspec_table_[0];
}

const mulib_sspec *mulib_sspec_at(size_t i)
{
    return i < mulib_sspec_count() ? mulib_sspec_table_ + i : NULL;
}

const mulib_sspec *mulib_sspec_find(const char *name)
{
    if (!name)
        return NULL;
    for (size_t i = 0; i < mulib_sspec_count(); ++i)
        if (!strcmp(name, mulib_sspec_table_[i].name))
            return mulib_sspec_table_ + i;
    return NULL;
}

const mulib_sspec *mulib_sspec_find_kind(uint32_t kind)
{
    for (size_t i = 0; i < mulib_sspec_count(); ++i)
        if (kind == mulib_sspec_table_[i].kind)
            return mulib_sspec_table_ + i;
    return NULL;
}
#ifdef __cplusplus
}
#endif
#endif

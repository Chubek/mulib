# HOTP/TOTP authentication

Define `MULIB_IMPL_OTPAUTH`, `MULIB_IMPL_AUTHCRYPTO` and `MULIB_IMPL_SECURITY`.
All secrets/configuration are borrowed; no allocation, clock reads, entropy,
provisioning URI/QR generation, persistence or network I/O is performed.

`mulib_otpauth_config_t` takes a binary secret of at least 16 bytes, SHA-1 or
SHA-256, and 6..8 decimal digits. Prefer a CSPRNG-generated secret with the digest
length (20/32 bytes); do not use a password as an OTP key. SHA-512 is unsupported.
TOTP additionally takes a nonzero period and epoch in Unix seconds (normally
30 and 0). Explicit times before epoch fail. HOTP ignores period and epoch.

`mulib_otpauth_base32` decodes strict RFC 4648 Base32, case-insensitive, with
canonical padding or no padding. Empty input, whitespace/hyphens, embedded NUL,
invalid alphabet/length/padding and nonzero unused tail bits fail. Output and
written length are unchanged on failure. Input and output must not overlap.

`mulib_otpauth_hotp` generates RFC 4226 HOTP. `mulib_otpauth_totp` generates RFC
6238 TOTP with the configured digest. Code output is NUL-terminated, needs
`digits+1` bytes, and is unchanged on failure. Codes preserve leading zeroes.
Verification accepts exactly the configured number of ASCII decimal characters.

`mulib_otpauth_hotp_verify` searches the next counter through at most 100 future
counters and advances the caller's counter past the first match only on success.
It never wraps. Counter `UINT64_MAX` can be generated but cannot be consumed
because the next counter could not be represented.

`mulib_otpauth_totp_verify` searches up to 10 past/future steps and requires
`mulib_otpauth_replay_t`, initially `{0,0}`. On success it stores the first
matching counter newer than any previously accepted counter. Old/equal counters
deny; failed checks leave replay state unchanged. Counter zero and `UINT64_MAX`
are representable without sentinel ambiguity. Windows saturate at the counter
bounds and never wrap. All candidate codes in the bounded window are compared
without a data-dependent comparison early exit. Narrow windows are recommended.

**Serialize and persist** counter/replay updates per credential across every
worker. Resetting replay state each request defeats replay protection. Apply
strict per-account and per-source rate limits; 6-digit codes are brute-forceable.
Require TLS, protect the shared secret at rest, provide secure recovery and
enrollment, and do not treat OTP as phishing-resistant authentication. Advancing
system time or accepting future steps can invalidate earlier codes intentionally.
Code/secret/output buffers must not alias the configuration or secret storage.

[Example](../examples/modules/otpauth.c), [tests](../tests/test_authentication.c).

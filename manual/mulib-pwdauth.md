# Password records

Define `MULIB_IMPL_PWDAUTH`, `MULIB_IMPL_AUTHCRYPTO` and `MULIB_IMPL_SECURITY`.
`mulib_pwdauth_create` derives a 32-byte PBKDF2-HMAC-SHA-256 key from an explicit
length binary password and a caller-supplied 16-byte salt. The record contains
salt, iteration count and derived key; it contains no plaintext password.
Use a fresh unique OS-CSPRNG salt for every password creation/change.

Work factors are bounded by `MULIB_PWDAUTH_MIN_ITERATIONS` (600000) and
`MULIB_PWDAUTH_MAX_ITERATIONS` (10000000). Both creation and verification reject
out-of-range factors before doing expensive work. Calibrate an adequate factor
for deployment hardware; even the minimum is deliberately expensive. The bound
is not a request rate limiter or a promise that maximum-cost requests are cheap.

Passwords may contain embedded NUL and may be empty; account policy should
enforce password quality/length before creation. NULL with nonzero length fails;
NULL with zero length is allowed. Output is unchanged on failure. No allocation,
password normalization, serialization, account lookup or entropy is provided.
Do not persist the native struct as a portable wire format; serialize its fields
with an explicit application format/version and validated iteration count.

`mulib_pwdauth_verify` recomputes and compares all 32 derived bytes without a
data-dependent comparison early exit, wipes temporary secrets and returns 1 only
on a match. Read records only from trusted storage and protect their integrity;
the derived key enables offline guessing if stolen. Callers own and should erase
password buffers; wiping local intermediates cannot erase all runtime copies.

Prefer a vetted memory-hard password-hashing provider (such as Argon2id) where
available. This dependency-free PBKDF2 implementation is unaudited, not a claim
of equivalent memory hardness. Add TLS, account lockout/rate limits, dummy work
for unknown users, audit logging and secure recovery externally.
[Example](../examples/modules/pwdauth.c), [tests](../tests/test_authentication.c).

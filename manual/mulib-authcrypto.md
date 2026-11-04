# Authentication cryptography primitives

Define `MULIB_IMPL_AUTHCRYPTO` once. No external libraries or MuLIB dependencies.
ISO C11 fixed-width unsigned arithmetic; no allocations, platform entropy or I/O.

`mulib_authcrypto_hash` implements SHA-1 (20 bytes) and SHA-256 (32 bytes).
`mulib_authcrypto_hmac` implements RFC 2104 HMAC with either digest, including
hashing long keys. `mulib_authcrypto_size` returns zero for unsupported algorithms.
SHA-1 exists solely for legacy HOTP/TOTP interoperability, not new signatures,
password hashing or collision-resistant identifiers. SHA-512 is unsupported.

Inputs use explicit byte lengths; NULL data/key/password/salt is allowed only
with zero length. Hash/HMAC reject lengths exceeding the SHA bit-length limit
(including HMAC framing). Output capacity must hold the complete digest. Errors
leave output unchanged. Output may overlap input, as it is written after input
has been consumed. Calls return 1 on success and 0 on invalid arguments.

`mulib_authcrypto_pbkdf2` derives exactly 32 bytes using PBKDF2-HMAC-SHA-256 and
a nonzero 32-bit iteration count. It supports binary passwords and salts. It is
a low-level derivation primitive: **one iteration is valid but unsafe for stored
passwords**. Do not accept attacker-controlled iteration counts without an upper
bound. [pwdauth](mulib-pwdauth.md) supplies a bounded password-record API.

`mulib_authcrypto_wipe` writes through a volatile byte pointer. Key-bearing
contexts and temporary digest buffers are wiped internally; this is best effort
and cannot erase register copies, swap, crash dumps or compiler-created copies.
NULL wipe pointers are ignored. Hash/HMAC have no intentional data-dependent
branches on key/message bytes; lengths and algorithm choices are not hidden.

This is new, unaudited cryptographic code, **not** a certification or security
audit. For high-assurance deployments use an independently audited provider.
Tests include SHA, RFC 2202/4231 HMAC, PBKDF2, RFC 4226/6238 OTP and RFC 7636
vectors, plus hostile input and boundary cases.
[Example](../examples/modules/authcrypto.c), [tests](../tests/test_authentication.c).

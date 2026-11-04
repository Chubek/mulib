# otpath alternate spelling

`mulib-otpath.h` provides the requested filename alongside the canonical
[`mulib-otpauth.h`](mulib-otpauth.md). Define `MULIB_IMPL_OTPATH`,
`MULIB_IMPL_OTPAUTH`, `MULIB_IMPL_AUTHCRYPTO` and `MULIB_IMPL_SECURITY` once.
No dependency implementation switches are enabled implicitly.

`mulib_otpath_totp` and `mulib_otpath_totp_verify` forward to the canonical TOTP
functions with identical configuration, buffer, time, failure and replay-state
contracts. Including this header also exposes all HOTP/Base32/OTP declarations.
There is no separate algorithm, storage or security policy in this facade.
See the canonical manual for supported digests, windows and deployment limits.

[Example](../examples/modules/otpath.c), [tests](../tests/test_authentication.c).

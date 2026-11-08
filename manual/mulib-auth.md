# HTTP credentials and API keys

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_AUTH`, `MULIB_IMPL_BASE64` and `MULIB_IMPL_SECURITY`.
All methods require an authenticated TLS transport supplied by the application.

Headers are limited to `MULIB_AUTH_MAX_HEADER` (8192 bytes, excluding NUL).
Schemes are ASCII case-insensitive and accept one or more spaces before the
credential; tabs, trailing spaces, controls and malformed encodings are rejected.
`mulib_auth_bearer` returns a borrowed RFC 6750 token, **not** proof of identity.
It supports the standard Bearer alphabet with optional trailing `=` padding.

`mulib_auth_basic` decodes strict padded Base64 `username:password`, NUL terminates
the output, and reports the decoded length excluding NUL. Missing colons, embedded
NUL and control bytes fail. The first colon separates the username; subsequent
colons belong to the password. Empty components are syntactically allowed; policy
can deny them. `mulib_auth_basic_n` accepts a length-delimited header, with no NUL
required in the input. Output and length remain unchanged on failure. Exact
in-place decoding works. Internal decoded buffers are wiped before freeing, but
callers must erase their output/transport buffers when finished.

`mulib_auth_basic_authorize` invokes an explicit credential verifier with borrowed
username/password slices, then wipes the temporary decoded credentials.
`mulib_auth_bearer_authorize` invokes an explicit token verifier after parsing.
Missing callbacks, malformed credentials, allocation failures and any callback
return other than **exactly 1** deny. Do not retain the Basic slices after return.
Callbacks must enforce password/token validity, expiry, account status and policy.
Use [pwdauth](mulib-pwdauth.md) for salted password records or
[oauth2](mulib-oauth2.md) for issuer/audience/scope-aware access tokens.

`mulib_auth_apikey` compares explicit-length binary keys with no data-dependent
early exit for equal lengths. Empty/NULL keys, mismatched lengths and keys over
8192 bytes deny. It does not extract a header, generate keys, hash them for storage,
or provide revocation. Use high-entropy CSPRNG-generated keys; never use short
human-chosen strings. Length is not hidden.

No account database, TLS stack, rate limiter or audit log is supplied. Applications
must also reject duplicate Authorization headers before calling these helpers.
[Example](../examples/modules/auth.c), [tests](../tests/test_authentication.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-auth.h`](../include/mulib-auth.h) |
| Implementation switch | `MULIB_IMPL_AUTH` |
| Repository-local includes | [mulib-base64](mulib-base64.md), [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/auth.c`](../examples/modules/auth.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/auth.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

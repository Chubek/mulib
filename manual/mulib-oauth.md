# OAuth convenience facade

`mulib-oauth.h` exposes the existing OAuth2 module under the requested filename.
It is **not OAuth 1.0/1.0a**. Define `MULIB_IMPL_OAUTH`, `MULIB_IMPL_OAUTH2`,
`MULIB_IMPL_AUTH`, `MULIB_IMPL_AUTHCRYPTO`, `MULIB_IMPL_BASE64` and
`MULIB_IMPL_SECURITY` in one translation unit.

`mulib_oauth_authorize`, `mulib_oauth_state` and `mulib_oauth_pkce_challenge`
delegate to their `mulib_oauth2_*` counterparts with identical return values,
ownership and failure behavior. The include also exposes the full OAuth2 API:
claim checks, scope matching, expiring one-use transactions and S256 PKCE.
See [the OAuth2 manual](mulib-oauth2.md) for mandatory verifier responsibilities
and the TLS, nonce, persistence and concurrency boundaries. The facade enables
no dependency implementation switches implicitly.

[Example](../examples/modules/oauth.c), [tests](../tests/test_authentication.c).

# OAuth2 policy, state and PKCE

Define `MULIB_IMPL_OAUTH2`, `MULIB_IMPL_AUTH`, `MULIB_IMPL_AUTHCRYPTO`,
`MULIB_IMPL_BASE64` and `MULIB_IMPL_SECURITY` in one translation unit.

This module implements local OAuth2 security helpers, **not** an authorization
server, JWT/JWK parser, OIDC login flow, HTTP client, or OAuth 1.0 signature stack.
It does not mint tokens, fetch keys, perform redirects, or exchange codes.

## Access tokens

The original `mulib_oauth2_authorize` requires an explicit verifier callback that
checks token authenticity/introspection, issuer, audience, expiry **and** scope.
Missing policy denies. A callback must return exactly 1; parse success alone must
never authorize. Required scopes must be syntactically valid, nonempty scope lists.

`mulib_oauth2_authorize_claims` separates cryptographic verification from local
policy. Its verifier authenticates the access token and supplies trusted borrowed
claims; the helper then enforces exact case-sensitive issuer/audience equality,
`not_before <= now < expires_at`, and every requested scope. Claims strings must
remain valid until authorization returns, not point to callback-local storage.
Only one audience string is modeled; a JWT backend must resolve array audiences
against its configured expected audience. Backend policy must reject unsupported
algorithms, invalid signatures, inactive/revoked tokens and incorrect token types
(including ID-token/access-token confusion). A missing expiry (zero) denies.

`mulib_oauth2_claims_check` is only for **already authenticated** claims; calling it
on an unverified decoded JWT does not authenticate anyone. `mulib_oauth2_scope`
does exact token-set containment, not substring matching. Scope strings are
nonempty single-space-separated RFC 6749 ASCII tokens, without quotes or
backslashes, and at most 8192 bytes. Empty lists and repeated/edge spaces deny.
No clock-skew allowance is applied. The application supplies trusted Unix seconds.

## Callback state

`mulib_oauth2_state` compares nonempty already-decoded strings, at most 8192 bytes.
It does not create/store/consume a nonce. `mulib_oauth2_state_consume` additionally
checks exclusive expiry and marks a transaction consumed on success. Failed
attempts leave the transaction unchanged. Consume before attempting code exchange;
failed exchanges must not make state reusable. Bind each transaction to the
browser session, provider/issuer, client and exact redirect URI. Reject duplicate
state/code parameters in the HTTP layer. Serialize and persist consumption across
all workers; this plain struct is not a concurrent or durable replay store.

## PKCE

`mulib_oauth2_pkce_challenge` implements RFC 7636 **S256 only**. The verifier must
be 43..128 ASCII unreserved characters; the challenge is 43 unpadded base64url
characters. Output requires 44 bytes including NUL and is unchanged on failure.
`mulib_oauth2_pkce_verify` checks the recomputed challenge. There is no `plain`
fallback. Generate each verifier/state with an OS CSPRNG, not `mulib-rng`;
keep the verifier server/session-bound and require S256 at the authorization
endpoint. The library supplies no entropy or secret storage.

Require authenticated TLS, exact redirect URI allowlists, replay protection and
rate limiting externally. PKCE does not replace state or token verification.
[Example](../examples/modules/oauth2.c), [tests](../tests/test_authentication.c).

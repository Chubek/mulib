# OAuth convenience facade

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-oauth.h`](../include/mulib-oauth.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-oauth.h
 * @brief OAuth2 convenience facade; not an OAuth 1.0 implementation.
 * Define MULIB_IMPL_OAUTH, OAUTH2, AUTH, AUTHCRYPTO, BASE64 and SECURITY. */
#ifndef MULIB_OAUTH_H
#define MULIB_OAUTH_H
#include "mulib-oauth2.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Delegate to OAuth2 bearer authorization. Missing verifier denies. */
int mulib_oauth_authorize (const char *authorization, const char *required_scope,
                           mulib_oauth2_verify_fn verify, void *context);
/** Compare already-decoded nonempty state; use oauth2_state_consume for replay
 * protection. */
int mulib_oauth_state (const char *expected, const char *received);
/** Compute an RFC 7636 S256 challenge. */
int mulib_oauth_pkce_challenge (const char *verifier, char *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-oauth.h`](../include/mulib-oauth.h) |
| Implementation switch | `MULIB_IMPL_OAUTH` |
| Repository-local includes | [mulib-oauth2](mulib-oauth2.md) |
| Complete example | [`examples/modules/oauth.c`](../examples/modules/oauth.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/oauth.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

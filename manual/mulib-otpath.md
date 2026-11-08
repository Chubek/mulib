# otpath alternate spelling

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-otpath.h`](../include/mulib-otpath.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-otpath.h
 * @brief Alternate-spelling facade for HOTP/TOTP authentication.
 * Define MULIB_IMPL_OTPATH, OTPAUTH, AUTHCRYPTO and SECURITY. */
#ifndef MULIB_OTPATH_H
#define MULIB_OTPATH_H
#include "mulib-otpauth.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Generate a TOTP using the canonical otpauth configuration. */
int mulib_otpath_totp (const mulib_otpauth_config_t *config, uint64_t now,
                       char *out, size_t capacity);
/** Verify and consume a TOTP with persistent replay protection. */
int mulib_otpath_totp_verify (const mulib_otpauth_config_t *config,
                              const char *code, uint64_t now,
                              unsigned past, unsigned future,
                              mulib_otpauth_replay_t *replay);
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
| Header | [`include/mulib-otpath.h`](../include/mulib-otpath.h) |
| Implementation switch | `MULIB_IMPL_OTPATH` |
| Repository-local includes | [mulib-otpauth](mulib-otpauth.md) |
| Complete example | [`examples/modules/otpath.c`](../examples/modules/otpath.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/otpath.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

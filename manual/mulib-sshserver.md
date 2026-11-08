# sshserver — SSH user-authentication payloads

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_SSHSERVER`.
See [`examples/modules/sshserver.c`](../examples/modules/sshserver.c).

`auth_parse` consumes an entire SSH_MSG_USERAUTH_REQUEST payload (message 50),
requiring service `ssh-connection`, and supports methods `none` and `password`
without password-change requests. SSH strings are uint32-length-prefixed and
binary-safe. Username must be 1..1024 bytes; password is at most 4096 bytes.
The result borrows both strings. Return 1 success, 0 malformed, -1 unsupported
method/service/password change. Outputs stay unchanged on failure.

Initialize attempt policy with a positive maximum. `authenticate` consumes an
attempt for every submitted request, including malformed or unsupported input.
`none` always fails. A supported password request invokes a caller verifier
which returns 1 to accept. Success becomes terminal; exhausted or already
authenticated policy returns -1, rejection returns 0, acceptance returns 1.
No credentials are retained. `auth_reply` encodes message 52 success or message
51 failure with `password` as the available method and partial-success=false.

These are payload and policy helpers, not an SSH daemon. The application must
obtain payloads from a verified encrypted SSH transport and supply account
authorization. Public-key authentication, key exchange, channels and command
execution require a separate backend. Tests: `tests/test_protocol_extensions.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sshserver.h`](../include/mulib-sshserver.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-sshserver.h
 * @brief SSH-2 password user-authentication payload parsing and attempt
 * policy. Define MULIB_IMPL_SSHSERVER and MULIB_IMPL_ENDIAN once. Inputs must
 * come from an authenticated/encrypted SSH transport supplied by the
 * application.
 */
#ifndef MULIB_SSHSERVER_H
#define MULIB_SSHSERVER_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_sshserver_auth
  {
    const unsigned char *user, *password;
    size_t user_length, password_length;
    int none;
  } mulib_sshserver_auth_t;
  typedef struct mulib_sshserver
  {
    size_t attempts, limit;
    int authenticated;
  } mulib_sshserver_t;
  /** @brief Parse an entire SSH_MSG_USERAUTH_REQUEST (50), service
   * ssh-connection, method none/password, password change unsupported. 1
   * success, 0 malformed, -1 unsupported. Borrowed binary-safe strings;
   * username 1..1024 and password
   * <=4096 bytes. Public-key methods require another backend. */
  int mulib_sshserver_auth_parse (const void *, size_t,
                                  mulib_sshserver_auth_t *);
  /** @brief Initialize attempt limit, 1 success/0 invalid. */
  int mulib_sshserver_init (mulib_sshserver_t *, size_t max_attempts);
  /** @brief Check one request with verifier (1 accepts). Every request
   * consumes an attempt. 1 authenticated, 0 rejected, -1
   * invalid/exhausted/already done. none is always rejected. No password
   * pointer is retained. */
  int mulib_sshserver_authenticate (
      mulib_sshserver_t *, const void *, size_t,
      int (*verify) (void *, const mulib_sshserver_auth_t *), void *);
  /** @brief Encode success (52) or failure (51, password, partial=false).
   * 1 success/0 insufficient output; used reset on failure. */
  int mulib_sshserver_auth_reply (int accepted, void *, size_t, size_t *used);
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
| Header | [`include/mulib-sshserver.h`](../include/mulib-sshserver.h) |
| Implementation switch | `MULIB_IMPL_SSHSERVER` |
| Repository-local includes | [mulib-endian](mulib-endian.md) |
| Complete example | [`examples/modules/sshserver.c`](../examples/modules/sshserver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sshserver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

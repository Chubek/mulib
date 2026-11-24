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

The two legacy switches above are enough for the password helpers.
`mulib_sshserver_start` starts the [transport](mulib-sshtransport.md) with the
server's private Ed25519 seed. Use one [user-authentication
policy](mulib-sshauth.md) per connection, supplying application account and key
authorization. Open [channels](mulib-sshchannel.md) only after authentication
succeeds. This is not an SSH daemon: the application owns sockets, key storage,
account policy and the handling of channel requests.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`kernels/mulib-sshserver.h`](../kernels/mulib-sshserver.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-sshserver.h
 * @brief SSH-2 server entry points and bounded user-authentication policy.
 * Define MULIB_IMPL_SSHSERVER and ENDIAN for the legacy password API.
 * The session helpers use SSHTRANSPORT and SSHAUTH.
 */
#ifndef MULIB_SSHSERVER_H
#define MULIB_SSHSERVER_H
#include "mulib-sshauth.h"
#include "mulib-sshtransport.h"
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
  /** @brief Attempt limit and optional callbacks for the connection driver.
   * The limit counts requests, including none, queries, and unsupported
   * methods. Returning 1 authorizes the exact request account and credential.
   * The key callback sees unsigned queries and separately sees verified
   * signed requests; approval is never cached. Callbacks borrow request data
   * for the call only and must not re-enter the driver. */
  typedef struct
  {
    size_t max_attempts;
    int (*verify_password) (void *, const mulib_ssh_auth_request_t *);
    int (*authorize_key) (void *, const mulib_ssh_auth_request_t *);
    void *context;
  } mulib_sshserver_policy_t;
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
  /** @brief Start a server transport using a caller-owned Ed25519 host seed.
   * Versions exclude CRLF. Drive the exchange with mulib_sshtransport_handshake. */
  static inline int
  mulib_sshserver_start (mulib_sshtransport_t *s, mulib_ssh_slice_t client_version,
                         mulib_ssh_slice_t server_version, const unsigned char host_seed[32],
                         void *out, size_t cap, size_t *used)
  {
    return mulib_sshtransport_start (s, MULIB_SSH_SERVER, client_version, server_version,
                                      host_seed, out, cap, used);
  }

  /** @brief Check a signed request from mulib_sshtransport_open after accepting
   * ssh-userauth. Use one initialized policy per connection. Each submitted
   * request consumes an attempt. A valid signature and authorizer return of 1
   * are both required. Return 1 accepted, 0 rejected, -1 invalid state. */
  static inline int
  mulib_sshserver_public_authenticate (
      mulib_sshserver_t *s, const mulib_sshtransport_t *transport, const void *p, size_t n,
      int (*authorize) (void *, const mulib_ssh_auth_request_t *), void *context)
  {
    unsigned char session_id[32];
    mulib_ssh_auth_request_t request;
    if (!s || !authorize || !transport || transport->role != MULIB_SSH_SERVER
        || s->authenticated || s->attempts >= s->limit
        || !mulib_sshtransport_session_id (transport, session_id))
      return -1;
    ++s->attempts;
    if (!mulib_sshauth_public_verify (p, n, session_id, &request)
        || authorize (context, &request) != 1)
      return 0;
    s->authenticated = 1;
    return 1;
  }
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
| Header | [`kernels/mulib-sshserver.h`](../kernels/mulib-sshserver.h) |
| Implementation switch | `MULIB_IMPL_SSHSERVER` |
| Repository-local includes | [mulib-sshauth](mulib-sshauth.md), [mulib-sshtransport](mulib-sshtransport.md) |
| Complete example | [`examples/modules/sshserver.c`](../examples/modules/sshserver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/sshserver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

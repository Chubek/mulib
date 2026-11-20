# sshclient — SSH-2 identification and initial packets

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_SSHCLIENT`.
See [`examples/modules/sshclient.c`](../examples/modules/sshclient.c).

`identification` writes `SSH-2.0-software\r\n` with a printable, nonspace ASCII
software identifier of 1..244 bytes. `identify` parses one SSH-2.0 line of at
most 255 bytes, requiring CRLF, and copies software plus optional comments to
a NUL-terminated buffer. It does not skip pre-identification server banners
or accept SSH-1.x. Return 1 complete, 0 incomplete, -1 malformed/capacity.

`packet_encode` and `packet_decode` implement the unencrypted binary packet
format used before key activation: uint32 packet length, padding length,
payload, random padding. Total length is divisible by eight, padding is at
least four bytes, and payload is 1..32768 bytes. Encoding requires an explicit
RNG callback that fills padding and returns 1. Production callers must use
appropriate randomness. Decode borrows payload; it validates frame lengths,
padding bounds and alignment, returning 1/0/-1. `used=0` on failure.

The two legacy switches above are enough for the cleartext codecs. Do not use
them for encrypted traffic. For an SSH connection, `mulib_sshclient_start`
starts the [transport](mulib-sshtransport.md) with a trusted raw Ed25519 host
public key. Do not use a key learned from the same untrusted connection as its
own pin. Once the server accepts `ssh-userauth`,
`mulib_sshclient_public_auth` creates a signed Ed25519 request for that session;
seal it before sending. After authentication, use the
[channel helpers](mulib-sshchannel.md). The application owns sockets and
trust decisions.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sshclient.h`](../include/mulib-sshclient.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-sshclient.h
 * @brief SSH-2 client entry points and legacy cleartext codecs.
 * Define MULIB_IMPL_SSHCLIENT and ENDIAN for the legacy codecs.
 * The session helpers use SSHTRANSPORT and SSHAUTH.
 */
#ifndef MULIB_SSHCLIENT_H
#define MULIB_SSHCLIENT_H
#include "mulib-sshauth.h"
#include "mulib-sshtransport.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode SSH-2.0-software CRLF, printable nonspace ASCII software
   * identifier <=244 bytes. 1 success/0 invalid or insufficient capacity. */
  int mulib_sshclient_identification (const char *, void *, size_t,
                                      size_t *used);
  /** @brief Parse one identification line (no banner skipping). 1 success,
   * 0 incomplete, -1 malformed/unsupported. Copies software+comments to a
   * NUL-terminated output. Only SSH-2.0 accepted, maximum 255 wire bytes. */
  int mulib_sshclient_identify (const void *, size_t, char *, size_t,
                                size_t *used);
  /** @brief Encode an unencrypted SSH packet, block alignment 8 bytes, at
   * least 4 padding bytes. rng must fill padding with random bytes and
   * return 1. Payload length 1..32768. 1 success/0 failure; output may change
   * on RNG failure. */
  int mulib_sshclient_packet_encode (const void *, size_t, void *, size_t,
                                     size_t *used,
                                     int (*rng) (void *, void *, size_t),
                                     void *);
  /** @brief Decode unencrypted packet; borrowed payload. 1 complete, 0 need
   * more, -1 invalid. Strict 8-byte alignment and bounded payload. */
  int mulib_sshclient_packet_decode (const void *, size_t,
                                     const unsigned char **, size_t *length,
                                     size_t *used);
  /** @brief Start the client transport with a trusted raw host key pin.
   * Versions exclude CRLF. Drive the exchange with mulib_sshtransport_handshake. */
  static inline int
  mulib_sshclient_start (mulib_sshtransport_t *s, mulib_ssh_slice_t client_version,
                         mulib_ssh_slice_t server_version, const unsigned char host_key[32],
                         void *out, size_t cap, size_t *used)
  {
    return mulib_sshtransport_start (s, MULIB_SSH_CLIENT, client_version, server_version,
                                      host_key, out, cap, used);
  }

  /** @brief Encode a signed request for a ready client transport after the
   * peer accepts ssh-userauth. Seal the result before sending. */
  static inline int
  mulib_sshclient_public_auth (const mulib_sshtransport_t *s, mulib_ssh_slice_t user,
                               const unsigned char seed[32], void *out, size_t cap, size_t *used)
  {
    unsigned char session_id[32];
    if (used) *used = 0;
    return s && s->role == MULIB_SSH_CLIENT
           && mulib_sshtransport_session_id (s, session_id)
           && mulib_sshauth_public_write (user, seed, session_id, out, cap, used);
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
| Header | [`include/mulib-sshclient.h`](../include/mulib-sshclient.h) |
| Implementation switch | `MULIB_IMPL_SSHCLIENT` |
| Repository-local includes | [mulib-sshauth](mulib-sshauth.md), [mulib-sshtransport](mulib-sshtransport.md) |
| Complete example | [`examples/modules/sshclient.c`](../examples/modules/sshclient.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sshclient.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

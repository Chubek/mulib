# mulib-mumq

[Manuals](README.md) · [Library index](index.md)

MuMQ v1 is MuLIB's bounded topic message protocol, **not MQTT, AMQP or a claim
of interoperability with an unspecified existing MQ standard**. It uses only
internal socket/AES/authentication libraries and the OS socket interface.

Define `MULIB_IMPL_MUMQ`, `MULIB_IMPL_SOCKET`, `MULIB_IMPL_AES`, and
`MULIB_IMPL_AUTHCRYPTO` once. Add publisher/subscriber/server switches when
using those modules. `examples/mumq-common.h` demonstrates placement;
`examples/modules/mumq.c` runs a complete local broker/client exchange.

## Connections and events

Create with an initialized owning `mulib_socket_t`; success moves the descriptor
and enables nonblocking I/O. Failure retains ownership (the nonblocking flag
may already have changed only on successful creation). Destroy closes it and
frees queues. Objects require external synchronization. `mulib_mumq_fd` exposes
the descriptor for `mulib-select` or another caller-owned event loop.

`send` copies a frame, `flush` advances short writes, and `receive` assembles
one frame across short reads. Initialize received messages with `{0}` and call
`mulib_mumq_message_free` before reuse. AGAIN (1) retains state; EOF (2), negative
I/O/protocol/authentication errors on receive poison the channel: reconnect,
do not keep reading. Socket setup supports numeric IPv4 TCP with
`mulib_socket_listen`, `mulib_socket_accept`, `mulib_socket_connect` and local
stream pairs. Connect is blocking; listen/accept are nonblocking. No DNS or
implicit listener binding. The transport is POSIX; unsupported hosts fail
explicitly rather than emulate network delivery.

Per connection: <=16 queued frames and <=4 MiB queued wire bytes, <=1 MiB payload,
255-byte topic, one incoming frame. A full output queue returns AGAIN without
consuming a sequence number. Topics are exact, case-sensitive, nonempty ASCII
letters/digits/underscore/hyphen/dot/slash; no wildcards. SUBSCRIBE and SUBACK
have empty payloads. A successful enqueue is **not** a broker/delivery ACK.

## Wire format

All integers are unsigned big-endian; there is no native struct serialization.

| Offset | Bytes | Meaning |
|---|---:|---|
| 0 | 4 | ASCII `MMQ1` |
| 4 | 1 | 1 PUBLISH, 2 SUBSCRIBE, 3 SUBACK |
| 5 | 1 | 0 plaintext, 1 authenticated encryption |
| 6 | 2 | topic length (1..255) |
| 8 | 4 | payload length (0..1048576) |
| 12 | 8 | connection sequence, starting at 1 |
| 20 | 12 | nonce, or zeroes in plaintext mode |
| 32 | topic+payload | body, encrypted together in secure mode |
| after body | 32 | secure-mode authentication tag only |

Secure nonce = configured direction prefix[4] || sequence_be64. The entire
32-byte header is AAD. Receivers require the configured peer prefix, matching
nonce/sequence, strict consecutive sequences, matching security mode and valid
tag before returning plaintext. Plaintext cannot downgrade a secure channel.
Counter exhaustion fails; reconnect with a **new namespace**, not reused keys
and prefixes. Closed-session replay is not prevented if the application reuses
its old key/prefix pair.

NULL options explicitly select insecure plaintext, suitable for local testing
or a separately protected transport. Non-NULL key copies 64 secret bytes for
AES+HMAC. Supply a distinct **durably unique per-key/session/direction** four-byte
prefix for both ends: sender.tx = receiver.rx, sender.rx = receiver.tx. Do not
use random four-byte prefixes and assume collision freedom; provision a durable
namespace or fresh independent session keys. Configuration/task files must not
contain secret keys. There is no entropy generation, key exchange, identity
handshake or authorization implied by encryption. A broker terminates link
encryption and sees messages; this is not end-to-end encryption through it.

Optional paired `seal/open` callbacks replace AES with another **authenticated**
cipher implementing the documented nonce/AAD/in-place and fixed 32-byte-tag
contract. Context remains borrowed. A custom provider must preserve nonce
uniqueness and verify before decrypting. No external crypto dependency is
required or automatically loaded. See AES manual for audit limitations.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mumq.h`](../include/mulib-mumq.h) |
| Implementation switch | `MULIB_IMPL_MUMQ` |
| Repository-local includes | [mulib-aes](mulib-aes.md), [mulib-socket](mulib-socket.md) |
| Complete example | [`examples/modules/mumq.c`](../examples/modules/mumq.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mumq.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

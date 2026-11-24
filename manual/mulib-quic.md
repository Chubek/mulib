# QUIC wire helpers

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_QUIC` and `MULIB_IMPL_BUFFER`. The codecs accept RFC 9000
variable integers up to 2^62-1 and plaintext STREAM and ACK/ACK_ECN frames.
Read returns 1 OK, 0 truncated, or -1 invalid. STREAM views borrow input.
Writes append atomically to an initialized buffer.

`mulib_quic_stream_read` requires an explicit length, as before.
`mulib_quic_packet_stream_read` also accepts STREAM frames whose length
is implicit in the remaining packet payload. Give it only the bytes left
in the authenticated plaintext packet.

ACK decoding writes inclusive, descending ranges into caller-provided storage.
The capacity includes the first range. A declared range count above capacity
returns -2 before parsing ranges. ECN counters and the encoded delay are
returned unchanged in wire units. All outputs stay unchanged on failure.
The encoder rejects reversed, overlapping, and adjacent ranges.

These codecs do not depend on networking or TLS. [Codec example](../examples/modules/quic.c).

## Native QUIC v1 client

Define `MULIB_IMPL_QUIC_CLIENT` in the same C11 implementation translation unit
as the QUIC codec and the [native TLS implementation and its dependencies](mulib-tls.md#build-and-use).
The [live example](../examples/http3-get.c) shows the full switch setup. Other
translation units, including C++17 consumers, use the declarations without
switches. The live client uses POSIX UDP sockets and time; the codec is still
independently usable.

`mulib_quic_client_connect` takes a NUL-terminated host (no IPv6 brackets),
numeric port, optional options, and a pointer initialized to `NULL`. It returns
only after authenticating the TLS chain/SAN, verifying Finished, negotiating the
requested ALPN (defaults to `h3`), and receiving HANDSHAKE_DONE. A supplied
trust store overrides an explicit CA file; otherwise the system store is used.
There is no verification bypass or TCP fallback. The monotonic I/O budget defaults
to 10 seconds across the **whole connection**, not each I/O call; system DNS and
trust loading are outside that budget. The client connects to the first address
for which UDP socket setup succeeds; it does not race addresses.

The client supports QUIC v1 Initial/Handshake/1-RTT packet protection with
AES-128-GCM and X25519 TLS 1.3, Retry integrity validation, independent packet
number spaces, CRYPTO and STREAM reassembly, ACK ranges, bounded retransmission,
flow-control limits, and explicit FIN. Use stream 0 for bidirectional bytes;
client unidirectional streams are 2/6/10, and peer unidirectional streams are
3/7/11. The peer must grant any streams used for sending. Receive credit is
fixed per stream (1 MiB default on stream 0, configurable up to 16 MiB; 64 KiB
per unidirectional stream) and not recycled. Views borrow client memory and
expire on send, poll or close. Always call `mulib_quic_client_close` on an owned
client. The API is synchronous and not safe for concurrent access.

This is a **bounded client profile**, not a general-purpose QUIC stack. It has
no server, connection migration, version negotiation or QUIC v2, 0-RTT or
resumption, key updates, datagrams, or independent stream reset recovery. It
keeps one outstanding stream chunk and uses capped probe retries rather than
implementing RFC 9002 loss detection and congestion control. Packets far outside
the receive ACK window are ignored. TLS HelloRetryRequest and client
authentication are unsupported. Use the live profile only where those limits
are acceptable.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`kernels/mulib-quic.h`](../kernels/mulib-quic.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-quic.h
 * @brief RFC 9000 variable integers, STREAM and ACK frame codecs.
 * Define MULIB_IMPL_QUIC and BUFFER. These codecs operate on plaintext frame
 * payloads. Define MULIB_IMPL_QUIC_CLIENT separately for the native live client.
 * Parse: 1 OK, 0 truncated, -1 invalid. Views borrow input; failure unchanged.
 */
#ifndef MULIB_QUIC_H
#define MULIB_QUIC_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_QUIC_MAX_INTEGER UINT64_C (0x3fffffffffffffff)
  int mulib_quic_varint_read (const void *data, size_t length, uint64_t *value,
                              size_t *consumed);
  int mulib_quic_varint_write (mulib_buffer_t *out, uint64_t value);
  typedef struct mulib_quic_ack_range
  {
    uint64_t smallest, largest;
  } mulib_quic_ack_range_t;
  typedef struct mulib_quic_ack
  {
    uint64_t delay, ect0, ect1, ce;
    size_t range_count;
    int ecn;
  } mulib_quic_ack_t;
  /** Parse ACK/ACK_ECN into descending inclusive ranges. Returns -2 when
   * range_capacity is too small. All outputs are unchanged on failure. */
  int mulib_quic_ack_read (const void *data, size_t length,
                           mulib_quic_ack_t *out, mulib_quic_ack_range_t *ranges,
                           size_t range_capacity, size_t *consumed);
  /** Append ACK/ACK_ECN with descending, disjoint and nonadjacent ranges. */
  int mulib_quic_ack_write (mulib_buffer_t *out, const mulib_quic_ack_t *ack,
                            const mulib_quic_ack_range_t *ranges);
  typedef struct mulib_quic_stream
  {
    uint64_t stream_id, offset;
    const unsigned char *data;
    size_t length;
    int fin;
  } mulib_quic_stream_t;
  int mulib_quic_stream_read (const void *data, size_t length,
                              mulib_quic_stream_t *out, size_t *consumed);
  /** Like stream_read, but also accepts implicit-length STREAM frames.
   * Supply only the remaining plaintext packet payload as input. */
  int mulib_quic_packet_stream_read (const void *data, size_t length,
                                     mulib_quic_stream_t *out, size_t *consumed);
  int mulib_quic_stream_write (mulib_buffer_t *out,
                                const mulib_quic_stream_t *stream);

  struct mulib_x509_store;
  typedef struct mulib_quic_client mulib_quic_client_t;
  typedef struct mulib_quic_client_options
  {
    const struct mulib_x509_store *trust; /* Overrides ca_file. */
    const char *ca_file; /* NULL selects system trust. */
    const char *alpn; /* NULL selects "h3". */
    unsigned timeout_ms; /* Absolute I/O budget; 0 selects 10000. */
    size_t max_stream_bytes; /* Per direction on stream 0; 0 selects 1 MiB. */
  } mulib_quic_client_options_t;
  enum
  {
    MULIB_QUIC_OK = 1, MULIB_QUIC_TIMEOUT = 0, MULIB_QUIC_INVALID = -1,
    MULIB_QUIC_LIMIT = -2, MULIB_QUIC_NOMEM = -3, MULIB_QUIC_IO = -4,
    MULIB_QUIC_AUTH = -5, MULIB_QUIC_PROTOCOL = -6, MULIB_QUIC_CLOSED = -7
  };
  /** Open a QUIC v1 connection over UDP and confirm an authenticated TLS 1.3
   * handshake. *out must be NULL; unchanged on failure. DNS uses system policy.
   * This bounded client offers one bidirectional stream and three unidirectional
   * streams per endpoint. See the manual for the supported transport profile. */
  int mulib_quic_client_connect (const char *host, unsigned port,
                                 const mulib_quic_client_options_t *options,
                                 mulib_quic_client_t **out);
  /** Send ordered bytes on stream 0, 2, 6 or 10; waits for acknowledgment.
   * Acknowledged prefixes cannot be undone if the operation fails. */
  int mulib_quic_client_send (mulib_quic_client_t *client, uint64_t stream_id,
                              const void *data, size_t length, int fin);
  /** Receive and process one UDP datagram, waiting up to the remaining budget. */
  int mulib_quic_client_poll (mulib_quic_client_t *client);
  /** View all contiguous received bytes on stream 0, 3, 7 or 11.
   * View expires at the next send, poll or close. FIN means the final size has
   * been reached. Absent streams are represented by NULL data, zero length/FIN. */
  int mulib_quic_client_stream (const mulib_quic_client_t *client,
                                uint64_t stream_id, mulib_quic_stream_t *out);
  /** Best-effort CONNECTION_CLOSE followed by releasing all owned state. */
  void mulib_quic_client_close (mulib_quic_client_t *client);
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
| Header | [`kernels/mulib-quic.h`](../kernels/mulib-quic.h) |
| Implementation switch | `MULIB_IMPL_QUIC` |
| Repository-local includes | [`kernels/detail/mulib-quic-client.h`](../kernels/detail/mulib-quic-client.h), [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/quic.c`](../examples/modules/quic.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/quic.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

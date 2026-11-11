# socket

[Manuals](README.md) · [Library index](index.md)

POSIX connected stream ownership. Initialize handles with `MULIB_SOCKET_INIT`.
`pair` creates a close-on-exec AF_UNIX pair; outputs unchanged on failure.
`nonblocking` controls O_NONBLOCK; `send`/`receive` perform one possibly short
operation (maximum 1 MiB), retry EINTR and report AGAIN distinctly from EOF.
Counts are zero on error; zero-capacity receive succeeds without implying EOF.

Results: OK 0, AGAIN 1, EOF 2, negative invalid/I/O/unsupported. Send suppresses
SIGPIPE using MSG_NOSIGNAL/SO_NOSIGPIPE, or returns unsupported rather than
silently changing global signal policy. `close` resets, is idempotent, and never
retries close after EINTR. No TLS, authentication or threads.
Switch: `MULIB_IMPL_SOCKET`. Example: `examples/modules/socket.c`.

`connect(out, numeric_ipv4, port)` creates a close-on-exec TCP connection with
blocking connection establishment. Port is 1..65535. `listen(out, numeric_ipv4,
port, &bound_port)` creates a close-on-exec nonblocking listener with backlog 32
and SO_REUSEADDR; port zero selects an ephemeral port. No implicit wildcard
address is chosen. `accept(listener, out)` returns AGAIN when no client is ready,
and otherwise a new close-on-exec stream (call nonblocking if desired). These
operations require initialized invalid output handles and transfer ownership
only on success; `bound_port` remains unchanged on error. They use the platform
IPv4 socket interface, not a third-party networking library. There is no timeout
for blocking connect or name resolution. TCP roundtrip setup is tested in
`tests/test_mumq.c`.

`connect_host(out,host,port,timeout_ms)` resolves IPv4/IPv6 using the system resolver
and tries nonblocking connections with a per-address timeout. System DNS uses its
own timeout policy. Enable POSIX.1-2001 declarations (`_POSIX_C_SOURCE=200809L`)
before all system headers; otherwise this entry point returns ENOTSUP. Hostnames
and numeric IPs are accepted; IPv6 addresses have no URI brackets at this layer.

`local_listen(out,path)` and `local_connect(out,path,timeout_ms)` create filesystem
Unix streams. Both produce nonblocking close-on-exec descriptors. Listen never
removes an existing path; the caller owns unlink after closing its listener. Paths
must fit the platform's `sockaddr_un.sun_path` and cannot be empty. Put local
endpoints in an application-owned private directory; socket mode policy belongs
to the caller (MicroChan applies 0600).

`wait(socket,writing,timeout_ms)` polls read or write readiness, returning AGAIN on
timeout. Zero polls immediately; hangup is readable so receive reports EOF. A
readiness notification can race another operation, so callers must still handle
AGAIN on send/receive. These extensions are tested by MuW3's local integration and
MicroChan's multiclient/fragmentation tests.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-socket.h`](../include/mulib-socket.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-socket.h
 * @brief POSIX connected stream descriptor ownership and bounded I/O
 * primitives, TCP/DNS connection setup and filesystem Unix sockets.
 * Define MULIB_IMPL_SOCKET once. No implicit transport security. */
#ifndef MULIB_SOCKET_H
#define MULIB_SOCKET_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_socket
  {
    int fd;
  } mulib_socket_t;
#define MULIB_SOCKET_INIT { -1 }
  enum
  {
    MULIB_SOCKET_OK = 0,
    MULIB_SOCKET_AGAIN = 1,
    MULIB_SOCKET_EOF = 2,
    MULIB_SOCKET_EINVAL = -1,
    MULIB_SOCKET_EIO = -2,
    MULIB_SOCKET_ENOTSUP = -3
  };
  /** @brief Create owning close-on-exec AF_UNIX stream pair into two
   * initialized invalid descriptors; 0 success. Outputs unchanged on failure.
   */
  int mulib_socket_pair (mulib_socket_t *a, mulib_socket_t *b);
  /** @brief Blocking TCP connect to a numeric IPv4 address; output must be
   * invalid. Close-on-exec, no DNS. Returns 0, EINVAL/EIO/ENOTSUP below. */
  int mulib_socket_connect (mulib_socket_t *out, const char *address, unsigned port);
  /** Resolve a host (IPv4/IPv6) and connect, with a per-address millisecond
   * timeout. DNS uses the system resolver and its timeout policy. Requires
   * POSIX.1-2001 declarations enabled before system headers. Output nonblocking. */
  int mulib_socket_connect_host (mulib_socket_t *out, const char *host,
                                 unsigned port, unsigned timeout_ms);
  /** Filesystem Unix stream endpoints. Listen never removes an existing path;
   * caller owns unlink after close. Both outputs are nonblocking, close-on-exec. */
  int mulib_socket_local_listen (mulib_socket_t *out, const char *path);
  int mulib_socket_local_connect (mulib_socket_t *out, const char *path,
                                  unsigned timeout_ms);
  /** Wait for read (writing=0) or write (1) readiness; AGAIN means timeout.
   * Hangup is readable so receive can distinguish EOF. No signal policy changes. */
  int mulib_socket_wait (const mulib_socket_t *s, int writing, unsigned timeout_ms);
  /** @brief Nonblocking numeric IPv4 listener. Port zero requests an ephemeral
   * port, returned through bound_port. No implicit wildcard binding. */
  int mulib_socket_listen (mulib_socket_t *out, const char *address, unsigned port,
                          unsigned *bound_port);
  /** @brief Accept a close-on-exec connected stream; AGAIN when no client. */
  int mulib_socket_accept (mulib_socket_t *listener, mulib_socket_t *out);
  /** @brief Enable/disable O_NONBLOCK; 0 success. */
  int mulib_socket_nonblocking (mulib_socket_t *s, int enabled);
  /** @brief Send once (short writes allowed). EINTR retried, EAGAIN explicit.
   * SIGPIPE suppressed when platform supports MSG_NOSIGNAL/SO_NOSIGPIPE;
   * otherwise ENOTSUP (never silently change process signal policy). Count set
   * zero on errors. */
  int mulib_socket_send (mulib_socket_t *s, const void *data, size_t size,
                         size_t *sent);
  /** @brief Receive once; EOF distinct from AGAIN, zero capacity succeeds with
   * zero. */
  int mulib_socket_receive (mulib_socket_t *s, void *data, size_t capacity,
                            size_t *received);
  /** @brief Close/reset idempotently, 0 success. Never retries close after
   * EINTR. */
  int mulib_socket_close (mulib_socket_t *s);
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
| Header | [`include/mulib-socket.h`](../include/mulib-socket.h) |
| Implementation switch | `MULIB_IMPL_SOCKET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/socket.c`](../examples/modules/socket.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/socket.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

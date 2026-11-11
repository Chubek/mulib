# tsl

[Manuals](README.md) · [Library index](index.md)

Historical TSL spelling for an explicit **TLS backend adapter**, not encryption.
Zero initialize; `init` requires borrowed handshake/read/write/close callbacks
and context. Handshake returns OK only after actual hostname/chain verification
with verified=1. AGAIN permits retry; failed/unverified handshake is sticky and
prevents I/O. This interface has no plaintext fallback. The separate
[mulib-tls](mulib-tls.md) module supplies MuLIB's native TLS 1.3 backend and can
initialize an adapter with `mulib_tls_connect`.

Read/write require an authenticated state. Backend counts must fit capacity;
error/AGAIN/EOF must have zero count, and invalid results fail closed. Read may
report EOF, write only OK/AGAIN. Nonempty successful I/O must make positive
progress; empty I/O succeeds without invoking the backend after authentication.
`close` invokes backend close once and resets, even after failed handshake.
Recursive handshake/read/write calls return EINVAL. Close requested during a
backend callback is deferred until that callback returns; the outer operation
then returns EINVAL with zero count and leaves the adapter closed. Backend close
may itself call close without recursively releasing the session. Backend/context
must remain live until close; callbacks may not mutate/reinitialize the adapter
or free its storage. This is not peer verification by itself: security depends
entirely on a correctly configured real TLS backend. Test doubles only validate
lifecycle/result handling.

Switch: `MULIB_IMPL_TSL`. Example: `examples/modules/tsl.c` deliberately uses an
unverified fake backend to demonstrate refusal of unauthenticated sessions.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tsl.h`](../include/mulib-tsl.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-tsl.h
 * @brief Transport-security lifecycle adapter for an application-supplied TLS
 * backend. Define MULIB_IMPL_TSL once. Historical spelling TSL; this header
 * supplies the lifecycle interface. mulib-tls.h supplies MuLIB's native TLS 1.3
 * backend. Backends must implement authenticated TLS. */
#ifndef MULIB_TSL_H
#define MULIB_TSL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TSL_OK = 0,
    MULIB_TSL_AGAIN = 1,
    MULIB_TSL_EOF = 2,
    MULIB_TSL_EINVAL = -1,
    MULIB_TSL_EAUTH = -2,
    MULIB_TSL_EBACKEND = -3
  };
  /** @brief Borrowed callbacks/context, must outlive adapter. Handshake
   * returns OK only after hostname/chain verification, with verified=1. AGAIN
   * is resumable. I/O returns OK/AGAIN/EOF or negative; count may never exceed
   * capacity. Nonempty OK I/O must make progress; other statuses require zero
   * count. Zero-length I/O does not invoke the backend. Recursive operations
   * are rejected; close during a callback is deferred until it returns.
   * Callbacks must not mutate/reinitialize the adapter or free its storage.
   * close releases the backend session exactly once. */
  typedef struct mulib_tsl_ops
  {
    int (*handshake) (void *, int *verified);
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
    void (*close) (void *);
  } mulib_tsl_ops_t;
  typedef struct mulib_tsl
  {
    mulib_tsl_ops_t ops;
    void *context;
    int state;
  } mulib_tsl_t;
  /** @brief Initialize inactive adapter; all callbacks mandatory; 1 success.
   */
  int mulib_tsl_init (mulib_tsl_t *s, const mulib_tsl_ops_t *ops,
                      void *context);
  /** @brief Establish verified session before permitting I/O; failure becomes
   * sticky. */
  int mulib_tsl_handshake (mulib_tsl_t *s);
  /** @brief Read authenticated session only; backend cannot be bypassed. */
  int mulib_tsl_read (mulib_tsl_t *s, void *data, size_t capacity,
                      size_t *count);
  /** @brief Write authenticated session only; short writes/AGAIN allowed. */
  int mulib_tsl_write (mulib_tsl_t *s, const void *data, size_t size,
                       size_t *count);
  /** @brief Close/reset once; zero-initialized/closed objects accepted. */
  void mulib_tsl_close (mulib_tsl_t *s);
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
| Header | [`include/mulib-tsl.h`](../include/mulib-tsl.h) |
| Implementation switch | `MULIB_IMPL_TSL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/tsl.c`](../examples/modules/tsl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tsl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

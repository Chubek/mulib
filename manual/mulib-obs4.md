# obs4 — obfs4 backend adapter

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_XTKS` and `MULIB_IMPL_OBS4`. The filename preserves `obs4`;
the intended protocol is **obfs4**. See
[`examples/modules/obs4.c`](../examples/modules/obs4.c).

Initialize a zeroed adapter with a copied 52-byte decoded certificate
(20-byte node ID plus 32-byte public key), a borrowed backend context and all
four operations. The backend is responsible for actual obfs4 handshake,
cryptography, certificate pinning, replay defenses, packet framing and padding.
No cryptographic implementation or alternative plaintext transport is supplied.

`handshake` returns 1 for resumable AGAIN, 0 only when the backend returns
success **and** authenticated=1, or a negative sticky failure. The adapter does
not permit application I/O before that transition. `stream` produces a borrowed
XTK stream. It checks callback progress/count/status contracts and makes I/O
errors sticky. The adapter must remain live and unmoved while that stream is
used. EOF is valid for reads; AGAIN transfers no bytes.

`close` invokes backend close once, clears adapter state and is idempotent.
Recursive handshake/read/write calls return EINVAL. Close requested during a
backend callback is deferred until it returns; the outer operation reports
EINVAL with zero count and leaves the adapter closed. Repeated close, including
from backend close itself, does not release the backend twice. Callbacks must
not mutate/reinitialize the adapter or free its storage.
Reinitialization requires closing the old session. Context ownership remains
with the application, subject to its backend close contract. The example uses
a rejecting fixture, not fake obfs4 encryption; tests in
`tests/test_transport_extensions.c` exercise state transitions and pinning failure.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-obs4.h`](../include/mulib-obs4.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-obs4.h
 * @brief Authenticated obfs4 backend lifecycle and X transport adaptation.
 * Define MULIB_IMPL_OBS4 and MULIB_IMPL_XTKS once. Historical obs4 filename.
 * The backend supplies obfs4 cryptography, handshake, framing and padding.
 */
#ifndef MULIB_OBS4_H
#define MULIB_OBS4_H
#include "mulib-xtks.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief I/O follows the X transport callback contract. Recursive
   * operations are rejected; close during a callback is deferred until it
   * returns. Callbacks must not mutate/reinitialize the adapter or free its
   * storage. Backend close is invoked exactly once. */
  typedef struct mulib_obs4_ops
  {
    int (*handshake) (void *, const unsigned char certificate[52],
                      int *authenticated);
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
    void (*close) (void *);
  } mulib_obs4_ops_t;
  typedef struct mulib_obs4
  {
    mulib_obs4_ops_t ops;
    void *context;
    unsigned char certificate[52];
    int state;
  } mulib_obs4_t;
  /** @brief Initialize zero-state client with decoded node-id/public-key cert.
   * All callbacks mandatory; copied cert, borrowed backend. 1 success/0
   * invalid. Backend handshake returns OK only after checking the pinned
   * certificate. */
  int mulib_obs4_init (mulib_obs4_t *, const mulib_obs4_ops_t *, void *,
                       const unsigned char certificate[52]);
  /** @brief 0 ready, 1 AGAIN, negative sticky failure. No I/O before ready. */
  int mulib_obs4_handshake (mulib_obs4_t *);
  /** @brief Borrow a checked stream; invalidated when adapter closes. */
  mulib_xtks_t mulib_obs4_stream (mulib_obs4_t *);
  /** @brief Close backend exactly once and clear state; NULL-safe. */
  void mulib_obs4_close (mulib_obs4_t *);
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
| Header | [`include/mulib-obs4.h`](../include/mulib-obs4.h) |
| Implementation switch | `MULIB_IMPL_OBS4` |
| Repository-local includes | [mulib-xtks](mulib-xtks.md) |
| Complete example | [`examples/modules/obs4.c`](../examples/modules/obs4.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/obs4.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

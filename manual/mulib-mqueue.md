# mqueue — System V interprocess queues

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MQUEUE`; no MuLIB dependencies.
See [`examples/modules/mqueue.c`](../examples/modules/mqueue.c).

`create` allocates a private-key mode-0600 queue and returns its native integer
ID. `send` copies one message with a positive long type; `receive` copies one
message to caller storage. Operations are nonblocking and retry EINTR. Payload
and receive capacity are capped at 1 MiB; native queue limits can be smaller.
Each operation allocates a temporary native message buffer and frees it before
returning. Zero-length messages are valid.

Receive selectors use `msgrcv` semantics: zero means FIFO; positive matches an
exact type; negative selects the lowest type <= its absolute value. LONG_MIN
is rejected. If the caller buffer is too small, TOOSMALL (-5) is returned and
the message remains queued. No silent truncation occurs. Counts reset on
failure; returned type changes only on success.

Results: 0 success, 1 AGAIN (empty receive/full send), -1 invalid, -2 OS error,
-3 unsupported platform, -4 allocation failure, -5 TOOSMALL. `remove` deletes
the queue and all pending messages. There is no implicit deletion or per-handle
close. This is System V IPC, not POSIX `mq_open`. Tests:
`tests/test_ipc_extensions.c`, including child-process communication.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mqueue.h`](../include/mulib-mqueue.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mqueue.h
 * @brief System V typed, nonblocking interprocess message queues.
 * Define MULIB_IMPL_MQUEUE once. Queue deletion is explicit, not automatic.
 */
#ifndef MULIB_MQUEUE_H
#define MULIB_MQUEUE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_MQUEUE_OK = 0,
    MULIB_MQUEUE_AGAIN = 1,
    MULIB_MQUEUE_EINVAL = -1,
    MULIB_MQUEUE_EIO = -2,
    MULIB_MQUEUE_ENOTSUP = -3,
    MULIB_MQUEUE_NOMEM = -4,
    MULIB_MQUEUE_TOOSMALL = -5
  };
  /** @brief Create a private mode-0600 queue; id unchanged on failure. */
  int mulib_mqueue_create (int *id);
  /** @brief Nonblocking atomic send, positive type, length <= 1 MiB (OS limits
   * may be smaller). Copies data during call. Zero-length messages supported.
   */
  int mulib_mqueue_send (int id, long type, const void *, size_t);
  /** @brief selector uses msgrcv semantics (zero FIFO, positive exact,
   * negative lowest type <= abs(selector)). Too-small buffers leave the
   * message queued. Count reset on errors; type changed only on success. */
  int mulib_mqueue_receive (int id, long selector, long *type, void *, size_t,
                            size_t *);
  /** @brief Delete a queue and all pending messages. */
  int mulib_mqueue_remove (int id);
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
| Header | [`include/mulib-mqueue.h`](../include/mulib-mqueue.h) |
| Implementation switch | `MULIB_IMPL_MQUEUE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/mqueue.c`](../examples/modules/mqueue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mqueue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

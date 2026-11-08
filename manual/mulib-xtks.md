# xtks — X transport toolkit

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_XTKS`; no dependencies. Here **xtks** means X transport
toolkit: a generic byte-stream abstraction, not the X11 protocol.
See [`examples/modules/xtks.c`](../examples/modules/xtks.c).

`mulib_xtks_t` borrows a context and read/write callbacks. There is no close
ownership, allocation, scheduling or implicit buffering. Read-only/write-only
streams are allowed; invoking a missing operation is invalid. Callbacks return
0 with positive progress, 1 AGAIN with zero count, 2 EOF with zero count for
reads only, or a negative error with zero count. Zero-length operations are
handled without invoking the backend. The checked wrappers reject oversized
counts, invalid statuses and success without progress, returning EIO (-2).
They reset public counts on errors.

`read_exact` and `write_all` loop over short operations and update a caller-owned
offset, initially zero. On AGAIN retain the same buffers, length, stream and
offset, then resume when ready. EOF may leave a partial read. The offset must
never exceed length. These helpers make progress on each iteration; a blocking
backend can still block, and timeout/cancellation policy belongs to that backend.
State and callbacks must be externally synchronized.

Tests: `tests/test_transport_extensions.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-xtks.h`](../include/mulib-xtks.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-xtks.h
 * @brief X transport toolkit: checked borrowed streams and resumable exact
 * I/O. Define MULIB_IMPL_XTKS once. No allocation or implicit descriptor
 * ownership.
 */
#ifndef MULIB_XTKS_H
#define MULIB_XTKS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_XTKS_OK = 0,
    MULIB_XTKS_AGAIN = 1,
    MULIB_XTKS_EOF = 2,
    MULIB_XTKS_EINVAL = -1,
    MULIB_XTKS_EIO = -2
  };
  /** @brief Callbacks return OK with positive progress, AGAIN/EOF with zero
   * count, or a negative error. Zero-length calls are handled by the toolkit.
   * EOF is only valid for reads. The context and operations are borrowed. */
  typedef struct mulib_xtks
  {
    void *context;
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
  } mulib_xtks_t;
  /** @brief Checked single read/write. Count is reset on failure. */
  int mulib_xtks_read (const mulib_xtks_t *, void *, size_t, size_t *);
  int mulib_xtks_write (const mulib_xtks_t *, const void *, size_t, size_t *);
  /** @brief Transfer exactly length bytes, resuming at *offset (initially
   * zero). Preserve offset and buffer across AGAIN. EOF may leave a partial
   * read. */
  int mulib_xtks_read_exact (const mulib_xtks_t *, void *, size_t,
                             size_t *offset);
  int mulib_xtks_write_all (const mulib_xtks_t *, const void *, size_t,
                            size_t *offset);
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
| Header | [`include/mulib-xtks.h`](../include/mulib-xtks.h) |
| Implementation switch | `MULIB_IMPL_XTKS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/xtks.c`](../examples/modules/xtks.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xtks.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

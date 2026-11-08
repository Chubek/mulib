# mulib-termutils

[Manuals](README.md) · [Library index](index.md)

POSIX terminal mode, window-size and complete-write helpers. Complements
`mulib-termkey`/`mulib-terminfo`; no curses dependency, escape-sequence renderer
or process-global cleanup hooks.

## Integration and ownership

Define `MULIB_IMPL_TERMUTILS` in one C11 translation unit. Native operations work
on Unix/macOS and return ENOTSUP elsewhere. Initialize raw state with
`MULIB_TERMUTILS_RAW_INIT`; do not copy it. `raw_begin(&state,fd)` saves native
attributes in owned heap storage, then sets raw 8-bit mode with VMIN=1, VTIME=0.
The fd is borrowed: keep it open and referring to the same terminal until restore.

`raw_end(&state)` restores attributes, releases saved storage and resets state.
It is idempotent and retains saved state after native failure for retry. Restore
on every normal/error path; abrupt termination/signals require application-level
cleanup. Do not stack independent raw states on the same fd or change terminal
attributes concurrently. No `atexit`, signal-handler installation or fd ownership.

## Results and writes

Success zero; EINVAL=-1, EIO=-2, ENOTSUP=-3, ESTATE=-4, NOMEM=-5. EIO preserves
errno. `size(fd,&columns,&rows)` queries TIOCGWINSZ; native zero dimensions remain
zero (no guessed defaults). Outputs unchanged on error.

`write(fd,bytes,length,&written)` retries EINTR and short writes, using bounded
chunks, until complete. On EIO it reports partial progress; EAGAIN remains in
errno and callers may resume from `written`. Zero-length data may be NULL. No
SIGPIPE suppression: callers writing to pipes must select their own signal policy.

`cc -std=c11 -Iinclude examples/modules/termutils.c -o termutils` does not alter
the user's console. Tests use an isolated pseudo-terminal for raw/restore/size
and a pipe for complete writes.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-termutils.h`](../include/mulib-termutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-termutils.h
 * @brief POSIX raw-terminal lifetime, window size and complete-write helpers.
 * Define MULIB_IMPL_TERMUTILS once. No process-global terminal state.
 */
#ifndef MULIB_TERMUTILS_H
#define MULIB_TERMUTILS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TERMUTILS_OK = 0,
    MULIB_TERMUTILS_EINVAL = -1,
    MULIB_TERMUTILS_EIO = -2,
    MULIB_TERMUTILS_ENOTSUP = -3,
    MULIB_TERMUTILS_ESTATE = -4,
    MULIB_TERMUTILS_NOMEM = -5
  };
  /** @brief Owned saved attributes, borrowed fd; initialize and never copy. */
  typedef struct mulib_termutils_raw
  {
    int fd;
    void *saved;
  } mulib_termutils_raw_t;
#define MULIB_TERMUTILS_RAW_INIT { -1, NULL }
  /** @brief Enter raw 8-bit mode, VMIN=1/VTIME=0; unchanged on error. fd must
   * stay open. */
  int mulib_termutils_raw_begin (mulib_termutils_raw_t *state, int fd);
  /** @brief Restore/free saved attributes, idempotently; retain state on
   * failure for retry. */
  int mulib_termutils_raw_end (mulib_termutils_raw_t *state);
  /** @brief Query columns/rows with TIOCGWINSZ; outputs unchanged on failure.
   */
  int mulib_termutils_size (int fd, unsigned *columns, unsigned *rows);
  /** @brief Write all bytes, retrying EINTR. On error written reports partial
   * progress. Nonblocking EAGAIN is EIO (errno retained); caller may resume at
   * written.
   */
  int mulib_termutils_write (int fd, const void *bytes, size_t length,
                             size_t *written);
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
| Header | [`include/mulib-termutils.h`](../include/mulib-termutils.h) |
| Implementation switch | `MULIB_IMPL_TERMUTILS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/termutils.c`](../examples/modules/termutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/termutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

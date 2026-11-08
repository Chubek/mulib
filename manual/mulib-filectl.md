# filectl — descriptor flags and locks

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_FILECTL`; no dependencies.
See [`examples/modules/filectl.c`](../examples/modules/filectl.c).

Descriptors are borrowed. `nonblocking(fd, enabled)` changes only O_NONBLOCK;
`cloexec(fd, enabled)` changes only FD_CLOEXEC, preserving other flags. Enabled
must be 0 or 1. These are read/modify/write operations, so callers must avoid
concurrent flag modifications. Setting close-on-exec after opening is not an
atomic open-with-CLOEXEC operation.

`lock(fd, mode)` requests a nonblocking whole-file POSIX record lock with
`fcntl(F_SETLK)`: 0 unlock, 1 shared/read, 2 exclusive/write. Conflicts return
BUSY (1), not an exception. Locks are process-associated, are not inherited by
forked children, and closing any descriptor for that file can release the
process's locks. They are not thread mutexes or open-file-description locks.

All functions return 0 success, -1 invalid, -2 OS error, -3 unsupported. Only
lock returns BUSY. No ownership transfer, allocation, or blocking lock waits.
POSIX backends preserve errno on OS failures. Tests: `tests/test_ipc_extensions.c`
uses a child process to verify lock exclusion and checks native descriptor flags.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-filectl.h`](../include/mulib-filectl.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-filectl.h
 * @brief POSIX descriptor flags and advisory whole-file record locks.
 * Define MULIB_IMPL_FILECTL once. Descriptors are always borrowed.
 */
#ifndef MULIB_FILECTL_H
#define MULIB_FILECTL_H
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_FILECTL_OK = 0,
    MULIB_FILECTL_BUSY = 1,
    MULIB_FILECTL_EINVAL = -1,
    MULIB_FILECTL_EIO = -2,
    MULIB_FILECTL_ENOTSUP = -3
  };
  /** @brief Toggle only O_NONBLOCK or FD_CLOEXEC, preserving other flags. */
  int mulib_filectl_nonblocking (int fd, int enabled);
  int mulib_filectl_cloexec (int fd, int enabled);
  /** @brief Nonblocking process-associated fcntl lock: 0 unlock, 1 shared,
   * 2 exclusive. Closing any descriptor for the file can release these locks.
   */
  int mulib_filectl_lock (int fd, int mode);
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
| Header | [`include/mulib-filectl.h`](../include/mulib-filectl.h) |
| Implementation switch | `MULIB_IMPL_FILECTL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/filectl.c`](../examples/modules/filectl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/filectl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

# shm — System V shared memory

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_SHM`; no MuLIB dependencies.
See [`examples/modules/shm.c`](../examples/modules/shm.c).

Initialize each handle with `MULIB_SHM_INIT`. `create` allocates a nonempty,
private-key, mode-0600 System V segment and attaches it read/write. Share its
integer ID explicitly with cooperating processes. `attach` discovers the
segment size and attaches read/write or read-only. Outputs remain unchanged on
failure; failed creation removes the new segment. Live handles must not be
copied or reinitialized. Read-only mappings must never be written.

`remove(id)` explicitly marks the segment for deletion after the final detach.
Existing mappings remain valid. `close` detaches and resets the local handle
without deleting the segment; it is idempotent and retains state if detach
fails. Applications must arrange removal, including failure paths, because
System V segments can outlive the creating process.

Returns 0 success, -1 invalid/state, -2 OS failure, -3 unsupported platform.
The backend uses native `shmget/shmat/shmctl/shmdt` on Unix systems providing
them. No named POSIX shm API, resize, allocator, semaphore, atomicity or
synchronization is implicit. Tests: `tests/test_ipc_extensions.c` verifies
cross-process visibility, a read-only second attachment, and removal lifetime.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-shm.h`](../include/mulib-shm.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-shm.h
 * @brief System V shared-memory creation, attachment and explicit removal.
 * Define MULIB_IMPL_SHM once. No automatic synchronization is provided.
 */
#ifndef MULIB_SHM_H
#define MULIB_SHM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_shm
  {
    void *data;
    size_t size;
    int id, readonly;
  } mulib_shm_t;
#define MULIB_SHM_INIT { NULL, 0, -1, 0 }
  /** @brief Create a private, mode-0600 segment and attach read/write. Returns
   * 0 success, -1 invalid, -2 OS failure, -3 unsupported. Initialized output
   * is unchanged on failure. Share id explicitly with cooperating processes.
   */
  int mulib_shm_create (mulib_shm_t *, size_t);
  /** @brief Attach an existing id and discover its size; readonly is 0 or 1.
   */
  int mulib_shm_attach (mulib_shm_t *, int id, int readonly);
  /** @brief Mark id for deletion after the last detach; mappings stay valid.
   */
  int mulib_shm_remove (int id);
  /** @brief Detach/reset without deleting the segment; idempotent. */
  int mulib_shm_close (mulib_shm_t *);
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
| Header | [`include/mulib-shm.h`](../include/mulib-shm.h) |
| Implementation switch | `MULIB_IMPL_SHM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/shm.c`](../examples/modules/shm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/shm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

# blockdev — callback-backed blocks

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_BLOCKDEV`.

Configure a positive `block_size`, context, and optional read/write/flush
callbacks. Read/write pass exactly one block's bytes and return the callback's
status unchanged; absent callbacks, NULL buffers or zero block size return 0.
Flush without a callback succeeds (1); NULL device fails (0).
The application owns storage, buffer bounds, block-number validation and
durability. Write callbacks receive a legacy `void *` but must not modify the
input buffer. The wrapper allocates nothing and does not perform offset arithmetic.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-blockdev.h`](../include/mulib-blockdev.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Application-supplied fixed-size block device with checked I/O callbacks. C11
 * single-header; define MULIB_IMPL_BLOCKDEV once. */
#ifndef MULIB_BLOCKDEV_H
#define MULIB_BLOCKDEV_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_blockdev_io_fn) (void *context, uint64_t block, void *buffer,
                                       size_t size);
  typedef int (*mulib_blockdev_flush_fn) (void *context);
  typedef struct
  {
    void *context;
    size_t block_size;
    mulib_blockdev_io_fn read, write;
    mulib_blockdev_flush_fn flush;
  } mulib_blockdev_t;
  int mulib_blockdev_read (const mulib_blockdev_t *d, uint64_t block, void *out);
  int mulib_blockdev_write (const mulib_blockdev_t *d, uint64_t block, const void *in);
  int mulib_blockdev_flush (const mulib_blockdev_t *d);
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
| Header | [`include/mulib-blockdev.h`](../include/mulib-blockdev.h) |
| Implementation switch | `MULIB_IMPL_BLOCKDEV` |
| Repository-local includes | None |
| Complete example | [`examples/modules/blockdev.c`](../examples/modules/blockdev.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/blockdev.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

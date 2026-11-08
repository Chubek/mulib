# MuLIB fs

[Manuals](README.md) · [Library index](index.md)

Whole-file read and write helpers with explicit byte length. Reading allocates a NUL padding byte after the actual file contents for convenience, but binary data may itself contain NULs. Writing truncates existing destination files; writes are not atomic.

## Contract and behavior

`read()` returns a malloc-owned byte array, including for an empty file;
free it with `free()`. The optional length excludes a convenience trailing NUL;
embedded NUL bytes are preserved. NULL means argument, allocation, size, or I/O
failure and leaves the length output unchanged. The entire file is buffered.

`write()` opens with `wb`, replacing/truncating an existing file. It returns 1
only if writing, flushing, and closing succeed. Failure can leave a truncated
or partial file; this is not atomic replacement or a durability/fsync guarantee.
NULL data is accepted for a zero-byte write.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_FS
#include "mulib-fs.h"
int main(void) {
    assert(mulib_fs_write("mulib-example-tmp.txt", "ok", 2));
    size_t n = 0;
    unsigned char *p = mulib_fs_read("mulib-example-tmp.txt", &n);
    assert(p && n == 2);
    free(p);
    remove("mulib-example-tmp.txt");
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fs.h`](../include/mulib-fs.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-fs.h
 * @brief Whole-file read and write helpers with explicit byte length.
 * @defgroup mulib_fs MuLIB fs
 * @{
 * @details Reading allocates a NUL padding byte after the actual file contents
 * for convenience, but binary data may itself contain NULs. Writing truncates
 * existing destination files; writes are not atomic.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_FS before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_FS_H
#define MULIB_FS_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief mulib fs read; see manual/mulib-fs.md for contracts and examples.
   */
  unsigned char *mulib_fs_read (const char *path, size_t *length);
  /** @brief mulib fs write; see manual/mulib-fs.md for contracts and examples.
   */
  int mulib_fs_write (const char *path, const void *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fs.h`](../include/mulib-fs.h) |
| Implementation switch | `MULIB_IMPL_FS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/fs.c`](../examples/modules/fs.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fs.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

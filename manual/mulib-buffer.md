# MuLIB buffer

[Manuals](README.md) · [Library index](index.md)

Dynamically growing binary byte buffer without an automatic string terminator.
For a maintained trailing NUL, use [strbuf](mulib-strbuf.md); both can store
embedded zero bytes through their explicit-length APIs.

## Contract and behavior

Initialize with `{0}` or `mulib_buffer_init`; free before reinitializing
live storage. `append()` copies exactly the supplied number of bytes and does
not append a NUL terminator. A source range within the buffer's live bytes is
supported, including across growth. NULL input is valid only for zero bytes.

`append()` and `remove()` return 1 on success and 0 on invalid ranges, overflow,
or allocation failure. Failed append preserves the existing bytes and length.
Remove shifts following bytes left and retains capacity. Growth invalidates
borrowed data pointers; removal changes their logical contents. `free()` releases
the allocation and zeros state. No pointed-to objects are destroyed.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BUFFER
#include "mulib-buffer.h"
int main(void) {
    mulib_buffer_t b;
    mulib_buffer_init(&b);
    assert(mulib_buffer_append(&b, "ABC", 3));
    assert(b.length == 3);
    mulib_buffer_free(&b);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-buffer.h`](../include/mulib-buffer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-buffer.h
 * @brief Dynamically growing binary byte buffer.
 * @defgroup mulib_buffer MuLIB buffer
 * @{
 * @details Stores arbitrary bytes without appending a string terminator.
 * Both buffer and strbuf accept embedded NULs through explicit-length APIs.
 * Appending a range from the same buffer is supported,
 * including when growth reallocates it.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BUFFER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BUFFER_H
#define MULIB_BUFFER_H
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

  /** @brief Dynamically growing binary byte buffer. */
  typedef struct mulib_buffer
  {
    unsigned char *data;
    size_t length, capacity;
  } mulib_buffer_t;
  /** @brief mulib buffer init; see manual/mulib-buffer.md for contracts and
   * examples. */
  void mulib_buffer_init (mulib_buffer_t *b);
  /** @brief mulib buffer append; see manual/mulib-buffer.md for contracts and
   * examples. */
  int mulib_buffer_append (mulib_buffer_t *b, const void *data, size_t n);
  /** @brief mulib buffer remove; see manual/mulib-buffer.md for contracts and
   * examples. */
  int mulib_buffer_remove (mulib_buffer_t *b, size_t offset, size_t n);
  /** @brief mulib buffer free; see manual/mulib-buffer.md for contracts and
   * examples. */
  void mulib_buffer_free (mulib_buffer_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BUFFER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-buffer.h`](../include/mulib-buffer.h) |
| Implementation switch | `MULIB_IMPL_BUFFER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/buffer.c`](../examples/modules/buffer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/buffer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

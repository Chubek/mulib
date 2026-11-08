# MuLIB strbuf

[Manuals](README.md) · [Library index](index.md)

Dynamically growing NUL-terminated text builder. Successful append always maintains a trailing NUL. For empty newly initialized builders data is NULL until the first append. Appending a range from the same builder is supported even when growth reallocates it; the source range must fit within the existing bytes, including the trailing NUL.

## Contract and behavior

Initialize with `{0}` or `init()`. Append copies bytes and maintains
a trailing NUL; `length` excludes that terminator. `_append_n()` supports embedded
NUL bytes and NULL input at length zero; plain `append()` requires a valid
NUL-terminated string. Self-appends from valid existing storage are supported
across growth. Append/putc return 1 on success, 0 on invalid arguments, overflow,
or allocation failure, preserving existing content on failure.

An untouched empty buffer can have `data == NULL`; do not assume it already
points to an empty C string. Growth invalidates borrowed data pointers. `free()`
releases and zeros state. Free before reinitializing live storage.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#include "mulib-strbuf.h"
int main(void) {
    mulib_strbuf_t s;
    mulib_strbuf_init(&s);
    assert(mulib_strbuf_append(&s, "Hello"));
    assert(!strcmp(s.data, "Hello"));
    mulib_strbuf_free(&s);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-strbuf.h`](../include/mulib-strbuf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-strbuf.h
 * @brief Dynamically growing NUL-terminated text builder.
 * @defgroup mulib_strbuf MuLIB strbuf
 * @{
 * @details Successful append always maintains a trailing NUL. For empty newly
 * initialized builders data is NULL until the first append. Appending a range
 * from the same builder is supported, including when growth reallocates it.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STRBUF before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_STRBUF_H
#define MULIB_STRBUF_H
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

  /** @brief Dynamically growing NUL-terminated text builder. */
  typedef struct mulib_strbuf
  {
    char *data;
    size_t length, capacity;
  } mulib_strbuf_t;
  /** @brief mulib strbuf init; see manual/mulib-strbuf.md for contracts and
   * examples. */
  void mulib_strbuf_init (mulib_strbuf_t *s);
  /** @brief mulib strbuf append n; see manual/mulib-strbuf.md for contracts
   * and examples. */
  int mulib_strbuf_append_n (mulib_strbuf_t *s, const char *text, size_t n);
  /** @brief mulib strbuf append; see manual/mulib-strbuf.md for contracts and
   * examples. */
  int mulib_strbuf_append (mulib_strbuf_t *s, const char *text);
  /** @brief mulib strbuf putc; see manual/mulib-strbuf.md for contracts and
   * examples. */
  int mulib_strbuf_putc (mulib_strbuf_t *s, char c);
  /** @brief mulib strbuf free; see manual/mulib-strbuf.md for contracts and
   * examples. */
  void mulib_strbuf_free (mulib_strbuf_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STRBUF_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-strbuf.h`](../include/mulib-strbuf.h) |
| Implementation switch | `MULIB_IMPL_STRBUF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/strbuf.c`](../examples/modules/strbuf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/strbuf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

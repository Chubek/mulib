# MuLIB glob

[Manuals](README.md) · [Library index](index.md)

Portable glob pattern matching with star and question-mark wildcards. Matches the entire input string with `?` for exactly one byte and `*` for zero or more bytes. Matching is byte-oriented and not POSIX filename globbing; no character classes or path separator rules.

## Contract and behavior

`match(pattern, text)` matches the entire NUL-terminated byte string.
`?` consumes one byte and `*` consumes zero or more bytes; matching is case-
sensitive. Slashes and leading dots have no special treatment. Bracket classes,
escaping, and filesystem traversal are not implemented. NULL arguments return
0; a match returns 1. The matcher allocates nothing, retains no pointers, and
may retry suffixes after a star.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_GLOB
#include "mulib-glob.h"
int main(void) {
    assert(mulib_glob_match("file*.c", "file1.c"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-glob.h`](../include/mulib-glob.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-glob.h
 * @brief Portable glob pattern matching with star and question-mark wildcards.
 * @defgroup mulib_glob MuLIB glob
 * @{
 * @details Matches the entire input string with `?` for exactly one byte and
 * `*` for zero or more bytes. Matching is byte-oriented and not POSIX filename
 * globbing; no character classes or path separator rules.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GLOB before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_GLOB_H
#define MULIB_GLOB_H
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

  /** @brief mulib glob match; see manual/mulib-glob.md for contracts and
   * examples. */
  int mulib_glob_match (const char *pattern, const char *text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GLOB_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-glob.h`](../include/mulib-glob.h) |
| Implementation switch | `MULIB_IMPL_GLOB` |
| Repository-local includes | None |
| Complete example | [`examples/modules/glob.c`](../examples/modules/glob.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/glob.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

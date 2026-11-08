# MuLIB path

[Manuals](README.md) · [Library index](index.md)

Portable lexical path manipulations for slash-separated paths. Performs string-level operations only: does not resolve symlinks, canonicalize dot segments, or check the existence of a path.

## Contract and behavior

Basename recognizes both `/` and `\` separators and returns a pointer
inside the original string; a trailing separator yields the empty suffix.
Extension returns text after the last basename dot, excluding a leading dot
alone; absence yields a pointer to the terminating NUL. NULL input returns NULL.

Join copies directory and name, adding `/` only when needed. It returns 1 on
success, 0 on invalid input/insufficient capacity, and requires capacity including
NUL. Use separate output storage. This is lexical concatenation: it does not
normalize `..`, resolve symlinks, or treat an absolute name as replacing the directory.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_PATH
#include "mulib-path.h"
int main(void) {
    assert(!strcmp(mulib_path_basename("/foo/bar"), "bar"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-path.h`](../include/mulib-path.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-path.h
 * @brief Portable lexical path manipulations for slash-separated paths.
 * @defgroup mulib_path MuLIB path
 * @{
 * @details Performs string-level operations only: does not resolve symlinks,
 * canonicalize dot segments, or check the existence of a path.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_PATH before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_PATH_H
#define MULIB_PATH_H
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

  /** @brief mulib path basename; see manual/mulib-path.md for contracts and
   * examples. */
  const char *mulib_path_basename (const char *path);
  /** @brief mulib path extension; see manual/mulib-path.md for contracts and
   * examples. */
  const char *mulib_path_extension (const char *path);
  /** @brief mulib path join; see manual/mulib-path.md for contracts and
   * examples. */
  int mulib_path_join (const char *directory, const char *name, char *out,
                       size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_PATH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-path.h`](../include/mulib-path.h) |
| Implementation switch | `MULIB_IMPL_PATH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/path.c`](../examples/modules/path.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/path.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

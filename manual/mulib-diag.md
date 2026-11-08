# MuLIB diag

[Manuals](README.md) · [Library index](index.md)

Small status-code-to-text mapping for library diagnostics. Defines general error categories without imposing them on other modules or promising a global errno convention.

## Contract and behavior

`mulib_diag_message(code)` returns a static, immutable string. Do not
free or modify it. Known enum values map to success, argument, memory, parse,
I/O, and overflow descriptions; any other value returns `"unknown error"`.
This lookup does not inspect `errno`, allocate storage, or record diagnostics
from other modules automatically.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_DIAG
#include "mulib-diag.h"
int main(void) {
    assert(!strcmp(mulib_diag_message(MULIB_DIAG_OK), "success"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-diag.h`](../include/mulib-diag.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-diag.h
 * @brief Small status-code-to-text mapping for library diagnostics.
 * @defgroup mulib_diag MuLIB diag
 * @{
 * @details Defines general error categories without imposing them on other
 * modules or promising a global errno convention.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DIAG before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DIAG_H
#define MULIB_DIAG_H
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

  /** @brief Small status-code-to-text mapping for library diagnostics. */
  typedef enum mulib_diag_code
  {
    MULIB_DIAG_OK = 0,
    MULIB_DIAG_ARGUMENT = 1,
    MULIB_DIAG_MEMORY = 2,
    MULIB_DIAG_PARSE = 3,
    MULIB_DIAG_IO = 4,
    MULIB_DIAG_OVERFLOW = 5
  } mulib_diag_code_t;
  /** @brief mulib diag message; see manual/mulib-diag.md for contracts and
   * examples. */
  const char *mulib_diag_message (mulib_diag_code_t code);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DIAG_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-diag.h`](../include/mulib-diag.h) |
| Implementation switch | `MULIB_IMPL_DIAG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/diag.c`](../examples/modules/diag.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/diag.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

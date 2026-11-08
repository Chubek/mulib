# MuLIB env

[Manuals](README.md) · [Library index](index.md)

Environment variable inspection and bounded fallback copy. Accesses the process environment through the C runtime. Concurrent mutation of environment variables must be synchronized by the application.

## Contract and behavior

`present()` distinguishes an absent environment variable from a present
empty one. `copy()` copies its value, or the fallback when absent; a NULL
fallback means the empty string. Capacity includes the NUL terminator. It
returns 1 on success and 0 for invalid arguments or insufficient capacity,
leaving output unchanged on failure. No pointer into the environment escapes.
The process environment is external shared state; coordinate concurrent changes
according to your platform's environment API rules.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ENV
#include "mulib-env.h"
int main(void) {
    char out[32];
    assert(mulib_env_copy("MULIB_NONEXISTENT_ENV_NAME", "fallback", out,
                          sizeof out));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-env.h`](../include/mulib-env.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-env.h
 * @brief Environment variable inspection and bounded fallback copy.
 * @defgroup mulib_env MuLIB env
 * @{
 * @details Accesses the process environment through the C runtime. Concurrent
 * mutation of environment variables must be synchronized by the application.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ENV before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ENV_H
#define MULIB_ENV_H
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

  /** @brief mulib env present; see manual/mulib-env.md for contracts and
   * examples. */
  int mulib_env_present (const char *name);
  /** @brief mulib env copy; see manual/mulib-env.md for contracts and
   * examples. */
  int mulib_env_copy (const char *name, const char *fallback, char *out,
                      size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ENV_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-env.h`](../include/mulib-env.h) |
| Implementation switch | `MULIB_IMPL_ENV` |
| Repository-local includes | None |
| Complete example | [`examples/modules/env.c`](../examples/modules/env.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/env.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

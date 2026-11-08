# MuLIB uuid

[Manuals](README.md) · [Library index](index.md)

Formatting and parsing RFC 4122-style 128-bit UUID text. Parses and formats standard UUID hyphen groups. Does not generate UUIDs or validate version/variant fields; use OS random APIs for cryptographically secure generation.

## Contract and behavior

Parse requires a NUL-terminated 36-character `8-4-4-4-12` hexadecimal
spelling with hyphens at fixed positions. Either hex case is accepted; braces,
URN prefixes, and whitespace are rejected. It returns 1 on success, 0 on failure,
leaving output unchanged on failure. Format emits lowercase and requires an
output array of **37 bytes**, including NUL. No UUID version/variant validation,
random generation, allocation, or ownership transfer is performed.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_UUID
#include "mulib-uuid.h"
int main(void) {
    mulib_uuid_t u;
    assert(mulib_uuid_parse("550e8400-e29b-41d4-a716-446655440000", &u));
    char s[37];
    mulib_uuid_format(&u, s);
    assert(strlen(s) == 36);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-uuid.h`](../include/mulib-uuid.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-uuid.h
 * @brief Formatting and parsing RFC 4122-style 128-bit UUID text.
 * @defgroup mulib_uuid MuLIB uuid
 * @{
 * @details Parses and formats standard UUID hyphen groups. Does not generate
 * UUIDs or validate version/variant fields; use OS random APIs for
 * cryptographically secure generation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_UUID before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_UUID_H
#define MULIB_UUID_H
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

  /** @brief Formatting and parsing RFC 4122-style 128-bit UUID text. */
  typedef struct mulib_uuid
  {
    unsigned char bytes[16];
  } mulib_uuid_t;
  /** @brief mulib uuid parse; see manual/mulib-uuid.md for contracts and
   * examples. */
  int mulib_uuid_parse (const char *text, mulib_uuid_t *out);
  /** @brief mulib uuid format; see manual/mulib-uuid.md for contracts and
   * examples. */
  void mulib_uuid_format (const mulib_uuid_t *id, char out[37]);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_UUID_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uuid.h`](../include/mulib-uuid.h) |
| Implementation switch | `MULIB_IMPL_UUID` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uuid.c`](../examples/modules/uuid.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uuid.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

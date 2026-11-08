# MuLIB set

[Manuals](README.md) · [Library index](index.md)

Unique set of signed 64-bit integers. Insert duplicates succeed without increasing the set size. Remove does not preserve enumeration order.

## Contract and behavior

Initialize with `{0}` or `init()`. The set stores unique `int64_t` values
in an array; contains/add/remove use linear scans. Adding an existing value is
a successful no-op (returns 1). Add returns 0 on argument/allocation failure;
remove returns 0 when absent. Removal replaces the removed slot with the last
element, so iteration order is unstable. `free()` releases the array and zeros
state; free before reinitializing live storage. No key callbacks or hashing
are involved.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SET
#include "mulib-set.h"
int main(void) {
    mulib_set_t s;
    mulib_set_init(&s);
    assert(mulib_set_add(&s, 20) && mulib_set_contains(&s, 20));
    mulib_set_free(&s);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-set.h`](../include/mulib-set.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-set.h
 * @brief Unique set of signed 64-bit integers.
 * @defgroup mulib_set MuLIB set
 * @{
 * @details Insert duplicates succeed without increasing the set size. Remove
 * does not preserve enumeration order.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SET before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SET_H
#define MULIB_SET_H
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

  /** @brief Unique set of signed 64-bit integers. */
  typedef struct mulib_set
  {
    int64_t *items;
    size_t length, capacity;
  } mulib_set_t;
  /** @brief mulib set init; see manual/mulib-set.md for contracts and
   * examples. */
  void mulib_set_init (mulib_set_t *s);
  /** @brief mulib set add; see manual/mulib-set.md for contracts and examples.
   */
  int mulib_set_add (mulib_set_t *s, int64_t x);
  /** @brief mulib set contains; see manual/mulib-set.md for contracts and
   * examples. */
  int mulib_set_contains (const mulib_set_t *s, int64_t x);
  /** @brief mulib set remove; see manual/mulib-set.md for contracts and
   * examples. */
  int mulib_set_remove (mulib_set_t *s, int64_t x);
  /** @brief mulib set free; see manual/mulib-set.md for contracts and
   * examples. */
  void mulib_set_free (mulib_set_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SET_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-set.h`](../include/mulib-set.h) |
| Implementation switch | `MULIB_IMPL_SET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/set.c`](../examples/modules/set.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/set.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

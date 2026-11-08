# MuLIB bloom

[Manuals](README.md) · [Library index](index.md)

Bit-packed Bloom filter with configurable hash rounds. Bloom filters never return false negatives for inserted items, assuming the storage remains unchanged, but can return false positives. Hash function is non-cryptographic.

## Contract and behavior

`init()` requires a positive bit count and positive number of hash
rounds; it returns 1 on success, 0 on invalid sizing or allocation failure.
`add()` retains only hash-derived bits, not the input bytes. `test()` returns
1 for possible membership and 0 for definite absence (or invalid arguments).
Inserted values remain possible matches until the filter is freed; false
positives are expected and entries cannot be individually removed. Empty byte
strings are supported. `free()` releases and zeros state; free before reinit.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BLOOM
#include "mulib-bloom.h"
int main(void) {
    mulib_bloom_t b;
    assert(mulib_bloom_init(&b, 1024, 5));
    mulib_bloom_add(&b, "hello", 5);
    assert(mulib_bloom_test(&b, "hello", 5));
    mulib_bloom_free(&b);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bloom.h`](../include/mulib-bloom.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-bloom.h
 * @brief Bit-packed Bloom filter with configurable hash rounds.
 * @defgroup mulib_bloom MuLIB bloom
 * @{
 * @details Bloom filters never return false negatives for inserted items,
 * assuming the storage remains unchanged, but can return false positives. Hash
 * function is non-cryptographic.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BLOOM before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BLOOM_H
#define MULIB_BLOOM_H
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

  /** @brief Bit-packed Bloom filter with configurable hash rounds. */
  typedef struct mulib_bloom
  {
    unsigned char *bits;
    size_t bit_count;
    unsigned rounds;
  } mulib_bloom_t;
  /** @brief mulib bloom init; see manual/mulib-bloom.md for contracts and
   * examples. */
  int mulib_bloom_init (mulib_bloom_t *b, size_t bits, unsigned rounds);
  /** @brief mulib bloom add; see manual/mulib-bloom.md for contracts and
   * examples. */
  void mulib_bloom_add (mulib_bloom_t *b, const void *data, size_t n);
  /** @brief mulib bloom test; see manual/mulib-bloom.md for contracts and
   * examples. */
  int mulib_bloom_test (const mulib_bloom_t *b, const void *data, size_t n);
  /** @brief mulib bloom free; see manual/mulib-bloom.md for contracts and
   * examples. */
  void mulib_bloom_free (mulib_bloom_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BLOOM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bloom.h`](../include/mulib-bloom.h) |
| Implementation switch | `MULIB_IMPL_BLOOM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bloom.c`](../examples/modules/bloom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bloom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

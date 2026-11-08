# Hamming distances

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HAMMING_DISTANCE`.
[Example](../examples/modules/hamming_distance.c).

Both functions borrow two buffers of the same supplied byte length.
`mulib_hamming_distance` counts differing bytes; `mulib_hamming_bits` counts
set bits in their XOR (all bits in each unsigned char). Embedded NUL is ordinary
data. Empty inputs return zero and may be NULL. Missing nonempty input or bit
count overflow returns SIZE_MAX. There is no implicit strlen or unequal-length
padding. O(length) time, no allocation or ownership transfer.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hamming_distance.h`](../include/mulib-hamming_distance.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hamming_distance.h
 * @brief Equal-length byte and bit Hamming distances.
 * Define MULIB_IMPL_HAMMING_DISTANCE.
 */
#ifndef MULIB_HAMMING_DISTANCE_H
#define MULIB_HAMMING_DISTANCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Count differing bytes; SIZE_MAX for NULL with nonzero length. */
size_t mulib_hamming_distance(const void *a, const void *b, size_t length);
/** Count differing bits; SIZE_MAX on invalid input or count overflow. */
size_t mulib_hamming_bits(const void *a, const void *b, size_t length);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hamming_distance.h`](../include/mulib-hamming_distance.h) |
| Implementation switch | `MULIB_IMPL_HAMMING_DISTANCE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hamming_distance.c`](../examples/modules/hamming_distance.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hamming_distance.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

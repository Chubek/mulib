# simhash — weighted fingerprints

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_SIMHASH`; link `-lm`.

Provide token hashes and finite nonnegative weights. Each set bit adds its
weight to that bit's vote; unset bits subtract it. Positive votes set the
fingerprint bit; ties clear it. Weights are normalized by their maximum to avoid
overflow, with extended accumulators where available. Invalid input, no tokens
or all-zero weights returns zero, also a valid fingerprint. Precision still
limits near-canceling votes. Distance counts differing bits (0–64). Fingerprint
costs O(64 × tokens), constant storage; distance is allocation-free. Hash tokens
consistently before calling; this API does not tokenize text.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-simhash.h`](../include/mulib-simhash.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: 64-bit weighted SimHash fingerprints and Hamming distance. C11 single-header;
 * define MULIB_IMPL_SIMHASH once. */
#ifndef MULIB_SIMHASH_H
#define MULIB_SIMHASH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_simhash64 (const uint64_t *token_hash, const double *weight, size_t n);
  unsigned mulib_simhash_distance (uint64_t a, uint64_t b);
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
| Header | [`include/mulib-simhash.h`](../include/mulib-simhash.h) |
| Implementation switch | `MULIB_IMPL_SIMHASH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/simhash.c`](../examples/modules/simhash.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/simhash.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

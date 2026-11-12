# price-match

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

ASCII-normalized product matching, identifiers and bounded edit similarity.

## Contract and supported profile

`gtin` checks digits, lengths 8/12/13/14 and the GS1 modulo-10 check digit;
it does not check product allocation. `score` removes non-ASCII-alphanumeric
bytes, folds ASCII case, and returns one minus normalized Levenshtein distance.
Inputs are limited to 1024 bytes and must normalize to nonempty strings. It uses
two allocated rows, O(n*m) time and O(min(n,m)) memory; failure preserves score.

## Integration and ownership

Include `include/mulib-price-match.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_PRICE_MATCH`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-match.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-match.h`](../include/mulib-price-match.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-match.h
 * @brief ASCII-normalized product matching, identifiers and bounded edit
 * similarity. C11 single-header module. Define MULIB_IMPL_PRICE_MATCH in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_MATCH_H
#define MULIB_PRICE_MATCH_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-match.md for this operation's contract. */
  int mulib_price_match_gtin (const char *digits);
  /** @brief See manual/mulib-price-match.md for this operation's contract. */
  int mulib_price_match_score (const char *a, const char *b, double *score);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_MATCH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-match.h`](../include/mulib-price-match.h) |
| Implementation switch | `MULIB_IMPL_PRICE_MATCH` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/price-match.c`](../examples/modules/price-match.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-match.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

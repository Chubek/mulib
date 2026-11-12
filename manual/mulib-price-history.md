# price-history

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned ordered and deduplicated price observation history.

## Contract and supported profile

A zero-initialized history owns a timestamp-ordered array of copied records.
Insertion replaces the same offer at the same timestamp even at its configured
limit; other insertions honor the record limit (zero is unlimited). Input may
borrow an existing item. Insertions may move the array and invalidate views.
`range` includes both endpoints and returns the full matching count, writing at
most cap copies; NULL output counts only. Insertion is O(n); free resets limits.

## Integration and ownership

Include `include/mulib-price-history.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_HISTORY`, `MULIB_IMPL_PRICE_OBSERVATION`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-history.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-history.h`](../include/mulib-price-history.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-history.h
 * @brief Owned ordered and deduplicated price observation history.
 * C11 single-header module. Define MULIB_IMPL_PRICE_HISTORY in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_HISTORY_H
#define MULIB_PRICE_HISTORY_H
#include "mulib-price-observation.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_history
  {
    mulib_price_observation_t *items;
    size_t len, cap, limit;
  } mulib_price_history_t;
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  int mulib_price_history_add (mulib_price_history_t *h,
                               const mulib_price_observation_t *o);
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  size_t mulib_price_history_range (const mulib_price_history_t *h,
                                    int64_t from, int64_t to,
                                    mulib_price_observation_t *out,
                                    size_t cap);
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  void mulib_price_history_free (mulib_price_history_t *h);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_HISTORY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-history.h`](../include/mulib-price-history.h) |
| Implementation switch | `MULIB_IMPL_PRICE_HISTORY` |
| Repository-local includes | [mulib-price-observation](mulib-price-observation.md) |
| Complete example | [`examples/modules/price-history.c`](../examples/modules/price-history.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-history.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

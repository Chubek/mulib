# dom-range

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Validated immutable text/document offsets and range intersections.

## Contract and supported profile

Ranges are immutable value objects with half-open byte offsets and an opaque
document version. Make enforces start<=end<=document_length. Intersection requires
valid ranges and equal versions; touching ranges produce an empty intersection.
Shift moves both endpoints by a signed delta without under/overflow, requiring
the original and final range within length (including INT64_MIN handling).
Operations return one on success and preserve output on failure. No tree nodes,
live mutation adjustment or Unicode code-unit interpretation is provided.

## Integration and ownership

Include `include/mulib-dom-range.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_DOM_RANGE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/dom-range.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dom-range.h`](../include/mulib-dom-range.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dom-range.h
 * @brief Validated immutable text/document offsets and range intersections.
 * C11 single-header module. Define MULIB_IMPL_DOM_RANGE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_DOM_RANGE_H
#define MULIB_DOM_RANGE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_dom_range
  {
    size_t start, end;
    uint64_t document_version;
  } mulib_dom_range_t;
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_make (size_t start, size_t end, size_t document_length,
                            uint64_t version, mulib_dom_range_t *out);
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_intersect (mulib_dom_range_t a, mulib_dom_range_t b,
                                 mulib_dom_range_t *out);
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_shift (mulib_dom_range_t r, int64_t delta, size_t length,
                             mulib_dom_range_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_RANGE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dom-range.h`](../include/mulib-dom-range.h) |
| Implementation switch | `MULIB_IMPL_DOM_RANGE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dom-range.c`](../examples/modules/dom-range.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dom-range.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

# css-flex

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Checked weighted one-axis flex distribution with gaps.

## Contract and supported profile

Distribute places at most one million boxes along a row (nonzero row) or
column, allocating remaining axis length in proportion to finite nonnegative
weights after fixed nonnegative gaps. A nonempty set needs positive total weight;
zero weights produce zero-sized children. Parent geometry and available space
are validated before writing. Empty NULL arrays succeed. Input/output arrays
must not overlap. This weighted geometry helper does not implement CSS Flexbox
basis, shrinking, wrapping, alignment or intrinsic content sizes.

## Integration and ownership

Include `include/mulib-css-flex.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_BOX`, `MULIB_IMPL_CSS_FLEX`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-flex.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-flex.h`](../include/mulib-css-flex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-flex.h
 * @brief Checked weighted one-axis flex distribution with gaps.
 * C11 single-header module. Define MULIB_IMPL_CSS_FLEX in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_FLEX_H
#define MULIB_CSS_FLEX_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-flex.md for this operation's contract. */
  int mulib_css_flex_distribute (mulib_css_box_t parent, const double *weights,
                                 size_t n, double gap, int row,
                                 mulib_css_box_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_FLEX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-flex.h`](../include/mulib-css-flex.h) |
| Implementation switch | `MULIB_IMPL_CSS_FLEX` |
| Repository-local includes | [mulib-css-box](mulib-css-box.md) |
| Complete example | [`examples/modules/css-flex.c`](../examples/modules/css-flex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-flex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

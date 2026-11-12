# css-grid

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Checked uniform grid cell geometry generator.

## Contract and supported profile

Cells divides finite parent geometry into positive rows/columns with finite
nonnegative gaps, writing row-major boxes only when rows*columns fits capacity
without overflow and gap space fits the parent. No allocation occurs; failure
leaves outputs unchanged. The caller supplies the complete output array. This
uniform geometric subdivision does not implement CSS Grid tracks, placement,
spanning, intrinsic sizing or alignment.

## Integration and ownership

Include `include/mulib-css-grid.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_BOX`, `MULIB_IMPL_CSS_GRID`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-grid.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-grid.h`](../include/mulib-css-grid.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-grid.h
 * @brief Checked uniform grid cell geometry generator.
 * C11 single-header module. Define MULIB_IMPL_CSS_GRID in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_GRID_H
#define MULIB_CSS_GRID_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-grid.md for this operation's contract. */
  int mulib_css_grid_cells (mulib_css_box_t parent, size_t rows,
                            size_t columns, double column_gap, double row_gap,
                            mulib_css_box_t *out, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_GRID_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-grid.h`](../include/mulib-css-grid.h) |
| Implementation switch | `MULIB_IMPL_CSS_GRID` |
| Repository-local includes | [mulib-css-box](mulib-css-box.md) |
| Complete example | [`examples/modules/css-grid.c`](../examples/modules/css-grid.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-grid.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

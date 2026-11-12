# css-box

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Backend-neutral block box geometry and vertical flow placement.

## Contract and supported profile

Boxes are caller-owned values with finite coordinates, nonnegative extents
and finite far edges. Contains uses half-open x/y extents. Vertical validates
all heights and a nonnegative gap, requiring the total to fit parent height,
then writes child boxes without changing output on failure. Empty input with
NULL arrays is allowed. Arrays must not overlap input/output. This is geometric
placement, not CSS margin collapse, box sizing, clipping, text or document layout.

## Integration and ownership

Include `include/mulib-css-box.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_BOX`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-box.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-box.h`](../include/mulib-css-box.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-box.h
 * @brief Backend-neutral block box geometry and vertical flow placement.
 * C11 single-header module. Define MULIB_IMPL_CSS_BOX in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_BOX_H
#define MULIB_CSS_BOX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_box
  {
    double x, y, width, height;
  } mulib_css_box_t;
  /** @brief Finite coordinates, nonnegative extents and finite far edges. */
  int mulib_css_box_valid (mulib_css_box_t box);
  /** @brief See manual/mulib-css-box.md for this operation's contract. */
  int mulib_css_box_vertical (mulib_css_box_t parent, const double *heights,
                              size_t n, double gap, mulib_css_box_t *out);
  /** @brief See manual/mulib-css-box.md for this operation's contract. */
  int mulib_css_box_contains (mulib_css_box_t box, double x, double y);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_BOX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-box.h`](../include/mulib-css-box.h) |
| Implementation switch | `MULIB_IMPL_CSS_BOX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/css-box.c`](../examples/modules/css-box.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-box.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

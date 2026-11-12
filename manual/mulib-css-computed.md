# css-computed

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Selected CSS declaration lookup with inherited color and font-size fields.

## Contract and supported profile

Defaults are block/black/16px with width=height=-1 (auto). Apply consumes
declarations in order, case-folds property names and supports display (block,
inline, none, flex, grid), opaque color text<=63 bytes, and nonnegative numeric
font-size/width/height with px or unitless values; width/height accept auto.
Whitespace before the colon is supported. Unknown properties are ignored after
lexical parsing; malformed supported values and !important fail atomically.
No units other than px, variables, shorthand expansion or color interpretation.
Apply operates on an initialized style; inherited fields are handled by cascade.

## Integration and ownership

Include `include/mulib-css-computed.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_COMPUTED`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-computed.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-computed.h`](../include/mulib-css-computed.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-computed.h
 * @brief Selected CSS declaration lookup with inherited color and font-size
 * fields. C11 single-header module. Define MULIB_IMPL_CSS_COMPUTED in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_CSS_COMPUTED_H
#define MULIB_CSS_COMPUTED_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_computed
  {
    char display[24], color[64];
    double font_size_px, width_px, height_px;
  } mulib_css_computed_t;
  /** @brief See manual/mulib-css-computed.md for this operation's contract. */
  int mulib_css_computed_defaults (mulib_css_computed_t *style);
  /** @brief See manual/mulib-css-computed.md for this operation's contract. */
  int mulib_css_computed_apply (mulib_css_computed_t *style,
                                const char *declarations, size_t n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_COMPUTED_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-computed.h`](../include/mulib-css-computed.h) |
| Implementation switch | `MULIB_IMPL_CSS_COMPUTED` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/css-computed.c`](../examples/modules/css-computed.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-computed.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

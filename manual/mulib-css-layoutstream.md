# css-layoutstream

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Callback emission of calculated box geometry with stable node indexes.

## Contract and supported profile

Emit validates every box before calling the required synchronous observer
in increasing array-index order. Each box/index is borrowed for the callback;
NULL boxes are accepted only for zero count. Invalid geometry produces no
callbacks. Callback zero stops with failure, but earlier callbacks cannot be
rolled back. The array must remain immutable during dispatch. No rendering,
allocation or persistent observer registration is performed.

## Integration and ownership

Include `include/mulib-css-layoutstream.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_BOX`, `MULIB_IMPL_CSS_LAYOUTSTREAM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-layoutstream.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-layoutstream.h`](../include/mulib-css-layoutstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-layoutstream.h
 * @brief Callback emission of calculated box geometry with stable node
 * indexes. C11 single-header module. Define MULIB_IMPL_CSS_LAYOUTSTREAM in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_CSS_LAYOUTSTREAM_H
#define MULIB_CSS_LAYOUTSTREAM_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_css_layoutstream_fn) (void *ctx, size_t index,
                                            const mulib_css_box_t *box);
  /** @brief See manual/mulib-css-layoutstream.md for this operation's
   * contract. */
  int mulib_css_layoutstream_emit (const mulib_css_box_t *boxes, size_t n,
                                   mulib_css_layoutstream_fn callback,
                                   void *ctx);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_LAYOUTSTREAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-layoutstream.h`](../include/mulib-css-layoutstream.h) |
| Implementation switch | `MULIB_IMPL_CSS_LAYOUTSTREAM` |
| Repository-local includes | [mulib-css-box](mulib-css-box.md) |
| Complete example | [`examples/modules/css-layoutstream.c`](../examples/modules/css-layoutstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-layoutstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

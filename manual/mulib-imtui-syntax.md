# imtui-syntax — Code preview

Draw a bounded viewport into a frame from an editor snapshot and a
[termsyn](mulib-termsyn.md) lexer cache. C callers can retain the cache to
reuse lexer state after edits. A [Mune](mulib-imtui-mune.md) script can also
display a temporary highlighted preview.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-syntax.h`](../kernels/mulib-imtui-syntax.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_SYNTAX` |
| Repository-local includes | [mulib-imtui-core](mulib-imtui-core.md), [mulib-termsyn](mulib-termsyn.md) |
| Complete example | [`tests/test_imtui_mune.c`](../tests/test_imtui_mune.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels tests/test_imtui_mune.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

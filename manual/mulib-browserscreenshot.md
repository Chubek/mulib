# mulib-browserscreenshot: Validate screenshot dimensions and output raster buffer size.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Validate screenshot dimensions and output raster buffer size. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERSCREENSHOT` in exactly one C11 translation unit. Use `#include "mulib-browserscreenshot.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browserscreenshot.c -o /tmp/test-browserscreenshot
/tmp/test-browserscreenshot
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browserscreenshot.c -o /tmp/example-browserscreenshot
```

## API

```c
int mulib_browserscreenshot_size(size_t width,size_t height,size_t channels,size_t *out);
```

## Safety and ownership

Returns 1 and writes width × height × channels bytes for dimensions 1..32768 and channels 3 or 4. Returns 0 for invalid arguments or size_t overflow, preserving the output. No buffer is allocated and no pixels captured or encoded. This is a tightly packed raster sizing helper without row-stride/padding support; callers must separately enforce a suitable memory budget.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browserscreenshot.h`](../include/mulib-browserscreenshot.h) |
| Implementation switch | `MULIB_IMPL_BROWSERSCREENSHOT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browserscreenshot.c`](../examples/modules/browserscreenshot.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browserscreenshot.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

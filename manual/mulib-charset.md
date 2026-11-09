# mulib-charset: Bounded charset parameter extraction from Content-Type headers.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Bounded charset parameter extraction from Content-Type headers. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_CHARSET` in exactly one C11 translation unit. Use `#include "mulib-charset.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-charset.c -o /tmp/test-charset
/tmp/test-charset
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/charset.c -o /tmp/example-charset
```

## API

```c
/* Find charset token, write lowercase encoding name. */
int mulib_charset_extract(const char *value,char *out,size_t cap);
```

## Safety and ownership

Returns 1 after copying a lowercase charset token, 0 for absence, malformed parameters, duplicate charsets, or insufficient capacity. Output is unchanged on failure. Capacity includes NUL; input and output must not overlap. Parameter names are ASCII case-insensitive. Quoted tokens and optional horizontal whitespace are supported; other quoted parameter values, including escaped quotes and semicolons, are skipped correctly. Unterminated quotes, controls and non-token charset values are rejected. The media type itself and encoding availability are not validated; this does not transcode text.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-charset.h`](../include/mulib-charset.h) |
| Implementation switch | `MULIB_IMPL_CHARSET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/charset.c`](../examples/modules/charset.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/charset.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

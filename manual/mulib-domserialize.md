# mulib-domserialize: HTML text escaping into caller-owned output buffer.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

HTML text escaping into caller-owned output buffer. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_DOMSERIALIZE` in exactly one C11 translation unit. Use `#include "mulib-domserialize.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-domserialize.c -o /tmp/test-domserialize
/tmp/test-domserialize
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/domserialize.c -o /tmp/example-domserialize
```

## API

```c
/* Escape &, <, >, double quote, single quote; return number of output bytes or 0 for failure. */
size_t mulib_domserialize_escape(const char *text,char *out,size_t cap);
```

## Safety and ownership

Escapes `&`, `<`, `>`, double quote and apostrophe as `&amp;`, `&lt;`, `&gt;`, `&quot;` and `&#39;`. Returns bytes excluding NUL. Zero means empty success (writes NUL) or failure; invalid/capacity failure leaves output unchanged. Capacity includes NUL; overlap is unsupported. No allocation occurs. Other bytes pass through without Unicode validation. This is text/quoted-attribute escaping only, not DOM serialization, URL sanitization or safe encoding for JavaScript/CSS/raw-text contexts.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-domserialize.h`](../include/mulib-domserialize.h) |
| Implementation switch | `MULIB_IMPL_DOMSERIALIZE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/domserialize.c`](../examples/modules/domserialize.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/domserialize.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

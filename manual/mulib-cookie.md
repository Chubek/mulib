# mulib-cookie: HTTP Set-Cookie name/value validation and extraction.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

HTTP Set-Cookie name/value validation and extraction. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_COOKIE` in exactly one C11 translation unit. Use `#include "mulib-cookie.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-cookie.c -o /tmp/test-cookie
/tmp/test-cookie
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/cookie.c -o /tmp/example-cookie
```

## API

```c
/* Copy initial cookie-pair from Set-Cookie, preserving raw value; rejects controls. */
int mulib_cookie_pair(const char *header,char *name,size_t nc,char *value,size_t vc);
```

## Safety and ownership

Returns 1 for a copied initial Set-Cookie pair, 0 for invalid syntax or insufficient output capacity. Both outputs are unchanged on failure; capacities include NUL. Inputs and the two outputs must not overlap. The name must be a nonempty HTTP token and the value RFC 6265 cookie-octets, optionally surrounded by balanced quotes (preserved in the output). Empty values are accepted. Controls anywhere in the header are rejected. Attributes after the first semicolon are otherwise ignored: no domain, expiry, SameSite or Secure enforcement is provided.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cookie.h`](../include/mulib-cookie.h) |
| Implementation switch | `MULIB_IMPL_COOKIE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cookie.c`](../examples/modules/cookie.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cookie.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

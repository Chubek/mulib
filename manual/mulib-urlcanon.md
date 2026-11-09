# mulib-urlcanon: Conservative URL normalization for HTTP(S) resource keys.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Conservative URL normalization for HTTP(S) resource keys. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_URLCANON` in exactly one C11 translation unit. Use `#include "mulib-urlcanon.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-urlcanon.c -o /tmp/test-urlcanon
/tmp/test-urlcanon
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/urlcanon.c -o /tmp/example-urlcanon
```

## API

```c
/* Normalize ASCII scheme and host; strip fragment and default :80/:443 port. Does not resolve relative references, IDNA or dot segments. Returns 1 on success. */
int mulib_urlcanon_normalize(const char *src, char *dst, size_t cap);
```

## Safety and ownership

`mulib_urlcanon_normalize` returns 1 on success, 0 without modifying output on error. Capacity includes the NUL terminator. Exact in-place normalization is supported; other overlap is not. Input is NUL-terminated ASCII, at most 8192 bytes. Hostnames use DNS label syntax; hexadecimal bracketed IPv6 is accepted, but embedded IPv4 and zone IDs are not. Userinfo, whitespace, backslashes, invalid escapes, empty/zero ports and ports above 65535 are rejected.

Scheme/host case and numeric default ports are normalized, fragments removed, and an empty path becomes `/`. Path/query spelling, escapes, trailing host dots and dot segments are preserved. This is a conservative resource-key helper, not complete URL equivalence or a network security policy.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-urlcanon.h`](../include/mulib-urlcanon.h) |
| Implementation switch | `MULIB_IMPL_URLCANON` |
| Repository-local includes | None |
| Complete example | [`examples/modules/urlcanon.c`](../examples/modules/urlcanon.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/urlcanon.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

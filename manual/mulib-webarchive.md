# mulib-webarchive: Write WARC-like minimal record headers into a bounded caller buffer.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Write WARC-like minimal record headers into a bounded caller buffer. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBARCHIVE` in exactly one C11 translation unit. Use `#include "mulib-webarchive.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webarchive.c -o /tmp/test-webarchive
/tmp/test-webarchive
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/webarchive.c -o /tmp/example-webarchive
```

## API

```c
size_t mulib_webarchive_record_header(const char *url,size_t body_bytes,char *out,size_t cap);
```

## Safety and ownership

Writes a `WARC/1.0`-prefixed resource header with Target-URI and decimal Content-Length, ending in CRLF CRLF. Returns bytes excluding NUL, or 0 for invalid input/capacity/formatting failure without writing output. URI must be a nonempty printable ASCII string without whitespace; controls are rejected to prevent header injection. Full URI syntax is not validated. Capacity includes NUL; no overlap or allocation. Body length may be zero or SIZE_MAX and no body is written.

This is intentionally **WARC-like, not a conforming WARC record**: required record ID/date fields and complete record framing are absent. Do not label its output as a standards-compliant archive without supplying those features separately.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webarchive.h`](../include/mulib-webarchive.h) |
| Implementation switch | `MULIB_IMPL_WEBARCHIVE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webarchive.c`](../examples/modules/webarchive.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webarchive.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->

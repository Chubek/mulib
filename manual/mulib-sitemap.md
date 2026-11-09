# mulib-sitemap: Extract next loc entry from XML sitemap bytes without fetching.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Extract next loc entry from XML sitemap bytes without fetching. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SITEMAP` in exactly one C11 translation unit. Use `#include "mulib-sitemap.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-sitemap.c -o /tmp/test-sitemap
/tmp/test-sitemap
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/sitemap.c -o /tmp/example-sitemap
```

## API

```c
/* Parse next <loc>...</loc> starting at *offset; plain XML subset, no namespace or entity decoding. */
int mulib_sitemap_next(const char *xml,size_t len,size_t *offset,char *url,size_t cap);
```

## Safety and ownership

Set offset to zero and pass an explicit input byte length (input need not be NUL-terminated). Returns 1 with a copied nonempty literal `<loc>...</loc>` value and advances offset past its closing tag. Returns 0 for exhaustion, malformed content or insufficient capacity, leaving offset/output unchanged; retry with a larger buffer is possible. Capacity includes NUL; overlap is unsupported. NUL bytes and nested markup in a value are rejected. This is a literal scanner, not an XML validator: namespaces, CDATA, entities and comment filtering are not supported, and extracted text is not URL-validated.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sitemap.h`](../include/mulib-sitemap.h) |
| Implementation switch | `MULIB_IMPL_SITEMAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/sitemap.c`](../examples/modules/sitemap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sitemap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
